#ifndef ALLOCATOR_TYPES_H
#define ALLOCATOR_TYPES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Allocator {

constexpr size_t POOL_SIZE_BYTES = 2 * 1024 * 1024; // 2 MiB (2,097,152 bytes)
constexpr size_t UNIT_SIZE_BYTES = 1024;            // 1 KiB (1,024 bytes)
constexpr size_t TOTAL_UNITS = POOL_SIZE_BYTES / UNIT_SIZE_BYTES; // 2048 units

enum class AllocationStrategy {
    FIRST_FIT,
    BEST_FIT
};

enum class BlockState {
    FREE = 0,
    ALLOCATED = 1
};

struct BlockDescriptor {
    size_t start_unit;
    size_t unit_count;
    size_t requested_bytes;
    uint64_t alloc_id;
    BlockState state;
};

struct AllocatorStats {
    size_t total_pool_bytes;
    size_t unit_size_bytes;
    size_t total_units;
    size_t allocated_units;
    size_t free_units;
    size_t active_allocations;
    size_t total_alloc_requests;
    size_t total_alloc_successes;
    size_t total_alloc_failures;
    size_t total_free_calls;
    size_t successful_frees;
    size_t rejected_frees;
    size_t reused_alloc_count;
    size_t largest_free_block_units;
    size_t internal_waste_bytes;
    double external_fragmentation_ratio;
    size_t strategy_search_steps;
    AllocationStrategy current_strategy;
};

struct LeakInfo {
    uint64_t alloc_id;
    size_t start_unit;
    size_t unit_count;
    size_t requested_bytes;
    size_t reserved_bytes;
    void* pool_address;
};

} // namespace Allocator

#endif // ALLOCATOR_TYPES_H
