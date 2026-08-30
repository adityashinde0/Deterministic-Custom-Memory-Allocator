#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "allocator_types.h"
#include "strategy.h"
#include <vector>
#include <memory>

namespace Allocator {

// Core Public C-style / Global Interface as defined in ARCHITECTURE.md
bool initialize_pool();
void shutdown_pool();
void reset_pool();

void* xmalloc(size_t bytes);
bool xfree(void* ptr);

void set_allocation_strategy(AllocationStrategy strategy);
AllocationStrategy get_allocation_strategy();

AllocatorStats get_allocator_stats();
void dump_leaks();
void dump_pool_layout();
std::vector<LeakInfo> get_active_leaks();

// Detailed Allocator Class implementation
class CustomPoolAllocator {
public:
    CustomPoolAllocator();
    ~CustomPoolAllocator();

    bool initialize();
    void shutdown();
    void reset();

    void* allocate(size_t bytes);
    bool deallocate(void* ptr);

    void set_strategy(AllocationStrategy strategy);
    AllocationStrategy get_strategy() const;

    AllocatorStats get_stats() const;
    std::vector<LeakInfo> get_leaks() const;
    void print_leaks() const;
    void print_layout() const;

    const std::vector<BlockDescriptor>& get_blocks() const { return blocks_; }
    const uint8_t* get_pool_base() const { return pool_memory_; }
    bool is_initialized() const { return initialized_; }

private:
    void coalesce_at(size_t index);
    bool validate_pointer(void* ptr, size_t& block_index) const;
    void update_largest_free_block();

    uint8_t* pool_memory_;
    bool initialized_;
    uint64_t next_alloc_id_;
    std::unique_ptr<IAllocationStrategy> strategy_;
    std::vector<BlockDescriptor> blocks_;

    // Telemetry and statistics
    size_t total_alloc_requests_;
    size_t total_alloc_successes_;
    size_t total_alloc_failures_;
    size_t total_free_calls_;
    size_t successful_frees_;
    size_t rejected_frees_;
    size_t reused_alloc_count_;
    mutable size_t strategy_search_steps_;
};

// Accessor for the global allocator instance
CustomPoolAllocator& get_global_allocator();

} // namespace Allocator

#endif // ALLOCATOR_H
