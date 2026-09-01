#include "allocator.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <sstream>

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

// Deterministic Workload Execution for Tier-2 Strategy Advisor
static StrategyAdvisorMetrics execute_workload_on_strategy(AllocationStrategy strat, const std::string& workload_type) {
    reset_pool();
    set_allocation_strategy(strat);

    std::mt19937 rng(42); // Exact deterministic seed

    if (workload_type == "burst_cycles") {
        std::uniform_int_distribution<size_t> size_dist(1024, 32 * 1024);
        const size_t BURST_CYCLES = 5;
        const size_t ALLOCS_PER_CYCLE = 50;

        for (size_t cycle = 0; cycle < BURST_CYCLES; ++cycle) {
            std::vector<void*> cycle_ptrs;
            for (size_t i = 0; i < ALLOCS_PER_CYCLE; ++i) {
                void* p = xmalloc(size_dist(rng));
                if (p) cycle_ptrs.push_back(p);
            }
            for (void* p : cycle_ptrs) {
                xfree(p);
            }
        }
    } else {
        // Default: "fragmented_churn" (400 ops)
        std::uniform_int_distribution<size_t> size_dist(512, 16 * 1024);
        const size_t NUM_INITIAL_ALLOCS = 250;
        std::vector<void*> ptrs;
        ptrs.reserve(NUM_INITIAL_ALLOCS);

        // Phase 1: 250 Allocations
        for (size_t i = 0; i < NUM_INITIAL_ALLOCS; ++i) {
            void* p = xmalloc(size_dist(rng));
            if (p) ptrs.push_back(p);
        }

        // Phase 2: Free every alternating block
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            if (ptrs[i]) {
                xfree(ptrs[i]);
                ptrs[i] = nullptr;
            }
        }

        // Phase 3: 150 Allocations into fragmented free holes
        const size_t SECOND_WAVE = 150;
        for (size_t i = 0; i < SECOND_WAVE; ++i) {
            void* p = xmalloc(size_dist(rng));
            if (p) ptrs.push_back(p);
        }
    }

    AllocatorStats stats = get_allocator_stats();
    StrategyAdvisorMetrics m;
    m.strategy_name = (strat == AllocationStrategy::FIRST_FIT) ? "First-Fit" : "Best-Fit";
    m.successful_allocations = stats.total_alloc_successes;
    m.failed_allocations = stats.total_alloc_failures;
    m.search_work_steps = stats.strategy_search_steps;
    m.total_free_kib = stats.free_units;
    m.largest_free_region_kib = stats.largest_free_block_units;
    m.fragmentation_ratio = stats.external_fragmentation_ratio;
    m.internal_waste_bytes = stats.internal_waste_bytes;
    m.reuse_events = stats.reused_alloc_count;

    return m;
}

StrategyAdvisorReport run_strategy_advisor(const std::string& workload_type) {
    StrategyAdvisorReport report;
    report.workload_name = (workload_type == "burst_cycles") ? "Burst Alloc/Free Cycles" : "Fragmented Churn Pattern";
    report.total_requests = (workload_type == "burst_cycles") ? 250 : 400;

    report.first_fit_metrics = execute_workload_on_strategy(AllocationStrategy::FIRST_FIT, workload_type);
    report.best_fit_metrics = execute_workload_on_strategy(AllocationStrategy::BEST_FIT, workload_type);

    const auto& ff = report.first_fit_metrics;
    const auto& bf = report.best_fit_metrics;

    std::ostringstream obs;
    std::ostringstream trade;

    if (bf.successful_allocations > ff.successful_allocations) {
        obs << "Best-Fit achieved a higher allocation success rate (" 
            << bf.successful_allocations << "/" << report.total_requests << " vs " 
            << ff.successful_allocations << "/" << report.total_requests 
            << ") and lower external fragmentation (" 
            << std::fixed << std::setprecision(1) << (bf.fragmentation_ratio * 100.0) << "% vs " 
            << (ff.fragmentation_ratio * 100.0) << "%).";

        trade << "Best-Fit tightly matches free block sizes, minimizing residue fragmentation and preserving larger contiguous runs. "
              << "However, Best-Fit evaluates all candidate blocks in the list (search work: " 
              << bf.search_work_steps << " steps vs " << ff.search_work_steps << " steps for First-Fit). "
              << "Recommendation: Use Best-Fit for memory-constrained heterogeneous workloads where allocation success rate is critical.";
    } else if (ff.successful_allocations > bf.successful_allocations) {
        obs << "First-Fit achieved a higher allocation success rate (" 
            << ff.successful_allocations << " vs " << bf.successful_allocations << ").";
        trade << "First-Fit reduced search overhead (" << ff.search_work_steps << " steps vs " 
              << bf.search_work_steps << " steps).";
    } else {
        obs << "Both strategies achieved identical allocation success (" 
            << ff.successful_allocations << "/" << report.total_requests << " requests satisfied).";
        
        trade << "First-Fit terminates search early upon finding the first suitable block (" 
              << ff.search_work_steps << " steps vs " << bf.search_work_steps << " steps), "
              << "offering lower allocation latency. Best-Fit preserves block layout compactness. "
              << "Recommendation: For uniform or burst cycles, First-Fit is preferred due to lower search work.";
    }

    report.observed_result = obs.str();
    report.tradeoff_analysis = trade.str();

    return report;
}

void print_strategy_advisor_report(const StrategyAdvisorReport& report) {
    const auto& ff = report.first_fit_metrics;
    const auto& bf = report.best_fit_metrics;

    std::cout << "\n============================================================\n";
    std::cout << "ALLOCATOR STRATEGY ANALYSIS\n";
    std::cout << "============================================================\n";
    std::cout << "\nWorkload: " << report.workload_name << "\n";
    std::cout << "Requests: " << report.total_requests << "\n\n";

    std::cout << std::left << std::setw(24) << "Metric" 
              << std::setw(18) << "First-Fit" 
              << std::setw(18) << "Best-Fit" << "\n";
    std::cout << "------------------------------------------------------------\n";

    std::cout << std::left << std::setw(24) << "Successful allocations" 
              << std::setw(18) << ff.successful_allocations 
              << std::setw(18) << bf.successful_allocations << "\n";

    std::cout << std::left << std::setw(24) << "Failed allocations" 
              << std::setw(18) << ff.failed_allocations 
              << std::setw(18) << bf.failed_allocations << "\n";

    std::cout << std::left << std::setw(24) << "Search work (steps)" 
              << std::setw(18) << ff.search_work_steps 
              << std::setw(18) << bf.search_work_steps << "\n";

    std::cout << std::left << std::setw(24) << "Total free" 
              << std::setw(18) << (std::to_string(ff.total_free_kib) + " KiB") 
              << std::setw(18) << (std::to_string(bf.total_free_kib) + " KiB") << "\n";

    std::cout << std::left << std::setw(24) << "Largest free region" 
              << std::setw(18) << (std::to_string(ff.largest_free_region_kib) + " KiB") 
              << std::setw(18) << (std::to_string(bf.largest_free_region_kib) + " KiB") << "\n";

    std::ostringstream ff_frag, bf_frag;
    ff_frag << std::fixed << std::setprecision(2) << (ff.fragmentation_ratio * 100.0) << " %";
    bf_frag << std::fixed << std::setprecision(2) << (bf.fragmentation_ratio * 100.0) << " %";
    std::cout << std::left << std::setw(24) << "Fragmentation" 
              << std::setw(18) << ff_frag.str() 
              << std::setw(18) << bf_frag.str() << "\n";

    std::cout << std::left << std::setw(24) << "Internal slack waste" 
              << std::setw(18) << (std::to_string(ff.internal_waste_bytes) + " B") 
              << std::setw(18) << (std::to_string(bf.internal_waste_bytes) + " B") << "\n";

    std::cout << std::left << std::setw(24) << "Reuse events" 
              << std::setw(18) << ff.reuse_events 
              << std::setw(18) << bf.reuse_events << "\n";

    std::cout << "------------------------------------------------------------\n";
    std::cout << "Observed result:\n" << report.observed_result << "\n\n";
    std::cout << "Trade-off:\n" << report.tradeoff_analysis << "\n";
    std::cout << "============================================================\n";
}

} // namespace Allocator
