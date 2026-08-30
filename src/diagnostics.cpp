#include "allocator.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace Allocator {

AllocatorStats CustomPoolAllocator::get_stats() const {
    AllocatorStats stats = {};
    stats.total_pool_bytes = POOL_SIZE_BYTES;
    stats.unit_size_bytes = UNIT_SIZE_BYTES;
    stats.total_units = TOTAL_UNITS;
    stats.allocated_units = 0;
    stats.free_units = 0;
    stats.active_allocations = 0;
    stats.total_alloc_requests = total_alloc_requests_;
    stats.total_alloc_successes = total_alloc_successes_;
    stats.total_alloc_failures = total_alloc_failures_;
    stats.total_free_calls = total_free_calls_;
    stats.successful_frees = successful_frees_;
    stats.rejected_frees = rejected_frees_;
    stats.reused_alloc_count = reused_alloc_count_;
    stats.largest_free_block_units = 0;
    stats.internal_waste_bytes = 0;
    stats.strategy_search_steps = strategy_search_steps_;
    stats.current_strategy = get_strategy();

    for (const auto& block : blocks_) {
        if (block.state == BlockState::ALLOCATED) {
            stats.allocated_units += block.unit_count;
            stats.active_allocations++;
            size_t reserved_bytes = block.unit_count * UNIT_SIZE_BYTES;
            if (reserved_bytes >= block.requested_bytes) {
                stats.internal_waste_bytes += (reserved_bytes - block.requested_bytes);
            }
        } else {
            stats.free_units += block.unit_count;
            if (block.unit_count > stats.largest_free_block_units) {
                stats.largest_free_block_units = block.unit_count;
            }
        }
    }

    if (stats.free_units > 0) {
        stats.external_fragmentation_ratio = 1.0 - (static_cast<double>(stats.largest_free_block_units) / static_cast<double>(stats.free_units));
    } else {
        stats.external_fragmentation_ratio = 0.0;
    }

    return stats;
}

std::vector<LeakInfo> CustomPoolAllocator::get_leaks() const {
    std::vector<LeakInfo> leaks;
    for (const auto& block : blocks_) {
        if (block.state == BlockState::ALLOCATED) {
            LeakInfo info;
            info.alloc_id = block.alloc_id;
            info.start_unit = block.start_unit;
            info.unit_count = block.unit_count;
            info.requested_bytes = block.requested_bytes;
            info.reserved_bytes = block.unit_count * UNIT_SIZE_BYTES;
            info.pool_address = pool_memory_ ? (pool_memory_ + (block.start_unit * UNIT_SIZE_BYTES)) : nullptr;
            leaks.push_back(info);
        }
    }
    return leaks;
}

void CustomPoolAllocator::print_leaks() const {
    auto leaks = get_leaks();
    std::cout << "\n=======================================================\n";
    std::cout << "                 MEMORY LEAK REPORT                     \n";
    std::cout << "=======================================================\n";
    if (leaks.empty()) {
        std::cout << " No memory leaks detected. All allocations released.\n";
        std::cout << "=======================================================\n";
        return;
    }

    std::cout << " Total Active Leaked Blocks: " << leaks.size() << "\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << std::setw(8) << "AllocID" 
              << std::setw(12) << "Start Unit" 
              << std::setw(12) << "Units (KiB)" 
              << std::setw(14) << "Requested (B)" 
              << std::setw(14) << "Reserved (B)" 
              << std::setw(14) << "Address" << "\n";
    std::cout << "-------------------------------------------------------\n";

    size_t total_leaked_requested = 0;
    size_t total_leaked_reserved = 0;

    for (const auto& leak : leaks) {
        total_leaked_requested += leak.requested_bytes;
        total_leaked_reserved += leak.reserved_bytes;
        std::cout << std::setw(8) << leak.alloc_id
                  << std::setw(12) << leak.start_unit
                  << std::setw(12) << leak.unit_count
                  << std::setw(14) << leak.requested_bytes
                  << std::setw(14) << leak.reserved_bytes
                  << std::setw(14) << leak.pool_address << "\n";
    }
    std::cout << "-------------------------------------------------------\n";
    std::cout << " Total Requested Leaked: " << total_leaked_requested << " bytes (" 
              << (total_leaked_requested / 1024.0) << " KiB)\n";
    std::cout << " Total Reserved Leaked:  " << total_leaked_reserved << " bytes (" 
              << (total_leaked_reserved / 1024.0) << " KiB)\n";
    std::cout << "=======================================================\n";
}

void CustomPoolAllocator::print_layout() const {
    std::cout << "\n=======================================================\n";
    std::cout << "                 POOL LAYOUT MAP                       \n";
    std::cout << "=======================================================\n";
    std::cout << " Total Descriptors: " << blocks_.size() << " | Pool: 2048 Units (2 MiB)\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << std::setw(6) << "Index"
              << std::setw(12) << "Status"
              << std::setw(12) << "Units"
              << std::setw(14) << "Range [Start-End)"
              << std::setw(10) << "AllocID"
              << std::setw(14) << "Req/Res (B)" << "\n";
    std::cout << "-------------------------------------------------------\n";

    for (size_t i = 0; i < blocks_.size(); ++i) {
        const auto& b = blocks_[i];
        std::string status_str = (b.state == BlockState::ALLOCATED) ? "[ALLOCATED]" : "[FREE]";
        std::string range_str = "[" + std::to_string(b.start_unit) + " - " + 
                                std::to_string(b.start_unit + b.unit_count) + ")";
        std::string req_res_str = (b.state == BlockState::ALLOCATED) ?
            (std::to_string(b.requested_bytes) + "/" + std::to_string(b.unit_count * UNIT_SIZE_BYTES)) : "-";

        std::cout << std::setw(6) << i
                  << std::setw(12) << status_str
                  << std::setw(12) << b.unit_count
                  << std::setw(14) << range_str
                  << std::setw(10) << (b.state == BlockState::ALLOCATED ? std::to_string(b.alloc_id) : "-")
                  << std::setw(14) << req_res_str << "\n";
    }
    std::cout << "-------------------------------------------------------\n";

    // Visual ASCII bar representing 64 characters (each char = 32 units = 32 KiB)
    std::cout << " Memory Bar [64 chars, 32 KiB/char]:\n [";
    for (size_t char_idx = 0; char_idx < 64; ++char_idx) {
        size_t unit_sample = char_idx * 32;
        // Find which block contains unit_sample
        bool is_alloc = false;
        for (const auto& b : blocks_) {
            if (unit_sample >= b.start_unit && unit_sample < b.start_unit + b.unit_count) {
                if (b.state == BlockState::ALLOCATED) {
                    is_alloc = true;
                }
                break;
            }
        }
        std::cout << (is_alloc ? "#" : ".");
    }
    std::cout << "]  (# = Allocated, . = Free)\n";
    std::cout << "=======================================================\n";
}

} // namespace Allocator
