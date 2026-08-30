#include "allocator.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <random>

using namespace Allocator;

struct BenchmarkResult {
    std::string workload_name;
    std::string strategy_name;
    double elapsed_ms;
    size_t alloc_successes;
    size_t alloc_failures;
    size_t search_steps;
    double final_external_frag_pct;
    size_t final_largest_free_kib;
};

void print_benchmark_table(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n========================================================================================================\n";
    std::cout << "                                  BENCHMARK PERFORMANCE COMPARISON                                      \n";
    std::cout << "========================================================================================================\n";
    std::cout << std::left << std::setw(28) << "Workload"
              << std::setw(14) << "Strategy"
              << std::setw(14) << "Time (ms)"
              << std::setw(16) << "Success / Fail"
              << std::setw(16) << "Search Steps"
              << std::setw(16) << "Frag (%)"
              << std::setw(14) << "Largest Free" << "\n";
    std::cout << "--------------------------------------------------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::string sf_str = std::to_string(r.alloc_successes) + " / " + std::to_string(r.alloc_failures);
        std::cout << std::left << std::setw(28) << r.workload_name
                  << std::setw(14) << r.strategy_name
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.elapsed_ms
                  << std::setw(16) << sf_str
                  << std::setw(16) << r.search_steps
                  << std::setw(16) << std::fixed << std::setprecision(2) << r.final_external_frag_pct
                  << std::setw(14) << (std::to_string(r.final_largest_free_kib) + " KiB") << "\n";
    }
    std::cout << "========================================================================================================\n";
}

BenchmarkResult run_churn_workload(AllocationStrategy strat, const std::string& strat_name) {
    reset_pool();
    set_allocation_strategy(strat);

    std::mt19937 rng(42); // Deterministic seed
    std::uniform_int_distribution<size_t> size_dist(512, 16 * 1024); // 512 B to 16 KiB

    const size_t NUM_INITIAL_ALLOCS = 250;
    std::vector<void*> ptrs;
    ptrs.reserve(NUM_INITIAL_ALLOCS);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Step 1: Initial allocations
    for (size_t i = 0; i < NUM_INITIAL_ALLOCS; ++i) {
        void* p = xmalloc(size_dist(rng));
        if (p) {
            ptrs.push_back(p);
        }
    }

    // Step 2: Free every alternating block to introduce fragmented holes
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        if (ptrs[i]) {
            xfree(ptrs[i]);
            ptrs[i] = nullptr;
        }
    }

    // Step 3: Second allocation wave trying to fit into created holes
    const size_t SECOND_WAVE = 150;
    for (size_t i = 0; i < SECOND_WAVE; ++i) {
        void* p = xmalloc(size_dist(rng));
        if (p) {
            ptrs.push_back(p);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    AllocatorStats stats = get_allocator_stats();

    BenchmarkResult res;
    res.workload_name = "Fragmented Churn (400 ops)";
    res.strategy_name = strat_name;
    res.elapsed_ms = elapsed;
    res.alloc_successes = stats.total_alloc_successes;
    res.alloc_failures = stats.total_alloc_failures;
    res.search_steps = stats.strategy_search_steps;
    res.final_external_frag_pct = stats.external_fragmentation_ratio * 100.0;
    res.final_largest_free_kib = stats.largest_free_block_units;

    return res;
}

BenchmarkResult run_burst_workload(AllocationStrategy strat, const std::string& strat_name) {
    reset_pool();
    set_allocation_strategy(strat);

    std::mt19937 rng(1337); // Deterministic seed
    std::uniform_int_distribution<size_t> size_dist(1024, 32 * 1024);

    const size_t BURST_CYCLES = 5;
    const size_t ALLOCS_PER_CYCLE = 50;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t cycle = 0; cycle < BURST_CYCLES; ++cycle) {
        std::vector<void*> cycle_ptrs;
        for (size_t i = 0; i < ALLOCS_PER_CYCLE; ++i) {
            void* p = xmalloc(size_dist(rng));
            if (p) {
                cycle_ptrs.push_back(p);
            }
        }
        for (void* p : cycle_ptrs) {
            xfree(p);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    AllocatorStats stats = get_allocator_stats();

    BenchmarkResult res;
    res.workload_name = "Burst Alloc/Free Cycles";
    res.strategy_name = strat_name;
    res.elapsed_ms = elapsed;
    res.alloc_successes = stats.total_alloc_successes;
    res.alloc_failures = stats.total_alloc_failures;
    res.search_steps = stats.strategy_search_steps;
    res.final_external_frag_pct = stats.external_fragmentation_ratio * 100.0;
    res.final_largest_free_kib = stats.largest_free_block_units;

    return res;
}

int main() {
    initialize_pool();

    std::vector<BenchmarkResult> results;
    results.push_back(run_churn_workload(AllocationStrategy::FIRST_FIT, "First-Fit"));
    results.push_back(run_churn_workload(AllocationStrategy::BEST_FIT, "Best-Fit"));

    results.push_back(run_burst_workload(AllocationStrategy::FIRST_FIT, "First-Fit"));
    results.push_back(run_burst_workload(AllocationStrategy::BEST_FIT, "Best-Fit"));

    print_benchmark_table(results);

    return 0;
}
