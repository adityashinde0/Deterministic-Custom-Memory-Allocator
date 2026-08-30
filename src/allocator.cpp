#include "allocator.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace Allocator {

// Global Singleton Instance
static CustomPoolAllocator g_allocator;

CustomPoolAllocator& get_global_allocator() {
    return g_allocator;
}

bool initialize_pool() {
    return g_allocator.initialize();
}

void shutdown_pool() {
    g_allocator.shutdown();
}

void reset_pool() {
    g_allocator.reset();
}

void* xmalloc(size_t bytes) {
    return g_allocator.allocate(bytes);
}

bool xfree(void* ptr) {
    return g_allocator.deallocate(ptr);
}

void set_allocation_strategy(AllocationStrategy strategy) {
    g_allocator.set_strategy(strategy);
}

AllocationStrategy get_allocation_strategy() {
    return g_allocator.get_strategy();
}

AllocatorStats get_allocator_stats() {
    return g_allocator.get_stats();
}

void dump_leaks() {
    g_allocator.print_leaks();
}

void dump_pool_layout() {
    g_allocator.print_layout();
}

std::vector<LeakInfo> get_active_leaks() {
    return g_allocator.get_leaks();
}

// CustomPoolAllocator Implementation
CustomPoolAllocator::CustomPoolAllocator()
    : pool_memory_(nullptr),
      initialized_(false),
      next_alloc_id_(1),
      strategy_(std::make_unique<FirstFitStrategy>()),
      total_alloc_requests_(0),
      total_alloc_successes_(0),
      total_alloc_failures_(0),
      total_free_calls_(0),
      successful_frees_(0),
      rejected_frees_(0),
      reused_alloc_count_(0),
      strategy_search_steps_(0) {
}

CustomPoolAllocator::~CustomPoolAllocator() {
    shutdown();
}

bool CustomPoolAllocator::initialize() {
    if (initialized_) {
        return true;
    }

    pool_memory_ = new (std::nothrow) uint8_t[POOL_SIZE_BYTES];
    if (!pool_memory_) {
        return false;
    }

    std::memset(pool_memory_, 0, POOL_SIZE_BYTES);

    blocks_.clear();
    BlockDescriptor initial_block;
    initial_block.start_unit = 0;
    initial_block.unit_count = TOTAL_UNITS;
    initial_block.requested_bytes = 0;
    initial_block.alloc_id = 0;
    initial_block.state = BlockState::FREE;
    blocks_.push_back(initial_block);

    initialized_ = true;
    next_alloc_id_ = 1;
    total_alloc_requests_ = 0;
    total_alloc_successes_ = 0;
    total_alloc_failures_ = 0;
    total_free_calls_ = 0;
    successful_frees_ = 0;
    rejected_frees_ = 0;
    reused_alloc_count_ = 0;
    strategy_search_steps_ = 0;

    return true;
}

void CustomPoolAllocator::shutdown() {
    if (pool_memory_) {
        delete[] pool_memory_;
        pool_memory_ = nullptr;
    }
    blocks_.clear();
    initialized_ = false;
}

void CustomPoolAllocator::reset() {
    if (!initialized_) {
        initialize();
        return;
    }

    std::memset(pool_memory_, 0, POOL_SIZE_BYTES);
    blocks_.clear();
    BlockDescriptor initial_block;
    initial_block.start_unit = 0;
    initial_block.unit_count = TOTAL_UNITS;
    initial_block.requested_bytes = 0;
    initial_block.alloc_id = 0;
    initial_block.state = BlockState::FREE;
    blocks_.push_back(initial_block);

    next_alloc_id_ = 1;
    total_alloc_requests_ = 0;
    total_alloc_successes_ = 0;
    total_alloc_failures_ = 0;
    total_free_calls_ = 0;
    successful_frees_ = 0;
    rejected_frees_ = 0;
    reused_alloc_count_ = 0;
    strategy_search_steps_ = 0;
}

void CustomPoolAllocator::set_strategy(AllocationStrategy strategy) {
    if (strategy == AllocationStrategy::FIRST_FIT) {
        strategy_ = std::make_unique<FirstFitStrategy>();
    } else {
        strategy_ = std::make_unique<BestFitStrategy>();
    }
}

AllocationStrategy CustomPoolAllocator::get_strategy() const {
    return strategy_ ? strategy_->get_strategy_type() : AllocationStrategy::FIRST_FIT;
}

void* CustomPoolAllocator::allocate(size_t bytes) {
    ++total_alloc_requests_;

    if (!initialized_ || bytes == 0 || bytes > POOL_SIZE_BYTES) {
        ++total_alloc_failures_;
        return nullptr;
    }

    size_t units_needed = (bytes + UNIT_SIZE_BYTES - 1) / UNIT_SIZE_BYTES;
    if (units_needed > TOTAL_UNITS) {
        ++total_alloc_failures_;
        return nullptr;
    }

    int block_idx = strategy_->find_free_block(blocks_, units_needed, strategy_search_steps_);
    if (block_idx < 0) {
        ++total_alloc_failures_;
        return nullptr;
    }

    size_t idx = static_cast<size_t>(block_idx);
    size_t orig_units = blocks_[idx].unit_count;

    // Track reuse: if there were prior frees, any allocation into a free block counts as reuse
    if (successful_frees_ > 0) {
        ++reused_alloc_count_;
    }

    if (orig_units > units_needed) {
        // Split block into allocated block + remaining free block
        BlockDescriptor remainder;
        remainder.start_unit = blocks_[idx].start_unit + units_needed;
        remainder.unit_count = orig_units - units_needed;
        remainder.requested_bytes = 0;
        remainder.alloc_id = 0;
        remainder.state = BlockState::FREE;

        blocks_[idx].unit_count = units_needed;
        blocks_[idx].requested_bytes = bytes;
        blocks_[idx].alloc_id = next_alloc_id_++;
        blocks_[idx].state = BlockState::ALLOCATED;

        blocks_.insert(blocks_.begin() + idx + 1, remainder);
    } else {
        // Exact fit
        blocks_[idx].requested_bytes = bytes;
        blocks_[idx].alloc_id = next_alloc_id_++;
        blocks_[idx].state = BlockState::ALLOCATED;
    }

    ++total_alloc_successes_;
    return pool_memory_ + (blocks_[idx].start_unit * UNIT_SIZE_BYTES);
}

bool CustomPoolAllocator::validate_pointer(void* ptr, size_t& block_index) const {
    if (!initialized_ || !ptr || !pool_memory_) {
        return false;
    }

    uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
    if (byte_ptr < pool_memory_ || byte_ptr >= pool_memory_ + POOL_SIZE_BYTES) {
        return false;
    }

    size_t offset = byte_ptr - pool_memory_;
    if (offset % UNIT_SIZE_BYTES != 0) {
        return false; // Not aligned to unit boundary
    }

    size_t target_unit = offset / UNIT_SIZE_BYTES;
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].start_unit == target_unit) {
            if (blocks_[i].state == BlockState::ALLOCATED) {
                block_index = i;
                return true;
            }
            return false; // Found matching unit, but it's already FREE (Double Free)
        }
    }

    return false; // Pointer does not match start of any active block
}

bool CustomPoolAllocator::deallocate(void* ptr) {
    ++total_free_calls_;

    size_t block_idx = 0;
    if (!validate_pointer(ptr, block_idx)) {
        ++rejected_frees_;
        return false;
    }

    // Mark as free
    blocks_[block_idx].state = BlockState::FREE;
    blocks_[block_idx].requested_bytes = 0;
    blocks_[block_idx].alloc_id = 0;

    // Coalesce adjacent free regions
    coalesce_at(block_idx);

    ++successful_frees_;
    return true;
}

void CustomPoolAllocator::coalesce_at(size_t index) {
    // Coalesce with right neighbor if free
    if (index + 1 < blocks_.size() && blocks_[index + 1].state == BlockState::FREE) {
        blocks_[index].unit_count += blocks_[index + 1].unit_count;
        blocks_.erase(blocks_.begin() + index + 1);
    }

    // Coalesce with left neighbor if free
    if (index > 0 && blocks_[index - 1].state == BlockState::FREE) {
        blocks_[index - 1].unit_count += blocks_[index].unit_count;
        blocks_.erase(blocks_.begin() + index);
    }
}

} // namespace Allocator
