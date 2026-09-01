#include "allocator.h"
#include "embedded_simulator.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace Allocator;

static int g_pass_count = 0;
static int g_fail_count = 0;

#define TEST_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            std::cout << "  [PASS] " << msg << "\n"; \
            ++g_pass_count; \
        } else { \
            std::cerr << "  [FAIL] " << msg << " (" << #expr << ") at line " << __LINE__ << "\n"; \
            ++g_fail_count; \
        } \
    } while(0)

void test_initialization() {
    std::cout << "\n=== Running: test_initialization ===\n";
    bool ok = initialize_pool();
    TEST_ASSERT(ok, "Pool initialized successfully");

    reset_pool();
    AllocatorStats stats = get_allocator_stats();
    TEST_ASSERT(stats.total_units == 2048, "Total units is 2048 (2 MiB)");
    TEST_ASSERT(stats.free_units == 2048, "All 2048 units are initially free");
    TEST_ASSERT(stats.allocated_units == 0, "Allocated units is 0");
    TEST_ASSERT(stats.active_allocations == 0, "Active allocations is 0");
    TEST_ASSERT(stats.largest_free_block_units == 2048, "Largest free block is 2048 units");
    TEST_ASSERT(stats.external_fragmentation_ratio == 0.0, "External fragmentation is 0.0%");
}

void test_unit_rounding() {
    std::cout << "\n=== Running: test_unit_rounding ===\n";
    reset_pool();

    // 1 byte -> 1 unit
    void* p1 = xmalloc(1);
    TEST_ASSERT(p1 != nullptr, "1-byte allocation succeeds");
    AllocatorStats s1 = get_allocator_stats();
    TEST_ASSERT(s1.allocated_units == 1, "1 byte rounds to 1 unit (1 KiB)");
    TEST_ASSERT(s1.internal_waste_bytes == 1023, "Internal waste is 1023 bytes");

    // 1024 bytes -> exactly 1 unit
    void* p2 = xmalloc(1024);
    TEST_ASSERT(p2 != nullptr, "1024-byte allocation succeeds");
    AllocatorStats s2 = get_allocator_stats();
    TEST_ASSERT(s2.allocated_units == 2, "Total allocated is 2 units");

    // 1025 bytes -> 2 units (2048 bytes)
    void* p3 = xmalloc(1025);
    TEST_ASSERT(p3 != nullptr, "1025-byte allocation succeeds");
    AllocatorStats s3 = get_allocator_stats();
    TEST_ASSERT(s3.allocated_units == 4, "Total allocated is 4 units (1 + 1 + 2)");

    xfree(p1);
    xfree(p2);
    xfree(p3);
}

void test_splitting_and_coalescing() {
    std::cout << "\n=== Running: test_splitting_and_coalescing ===\n";
    reset_pool();

    void* a = xmalloc(10 * 1024); // 10 units: units 0-9
    void* b = xmalloc(10 * 1024); // 10 units: units 10-19
    void* c = xmalloc(10 * 1024); // 10 units: units 20-29
    void* d = xmalloc(10 * 1024); // 10 units: units 30-39

    TEST_ASSERT(a && b && c && d, "Allocated 4 contiguous blocks");

    // Free b (gap in middle)
    bool f_b = xfree(b);
    TEST_ASSERT(f_b, "Freed block b");

    // Free d
    bool f_d = xfree(d);
    TEST_ASSERT(f_d, "Freed block d");

    // Now free c: should coalesce with left (b) and right (d + remainder)
    bool f_c = xfree(c);
    TEST_ASSERT(f_c, "Freed block c (3-way coalesce)");

    AllocatorStats s = get_allocator_stats();
    TEST_ASSERT(s.allocated_units == 10, "Only block a remains allocated (10 units)");
    TEST_ASSERT(s.free_units == 2038, "Free units is 2038");
    TEST_ASSERT(s.largest_free_block_units == 2038, "Largest free block coalesced to 2038 units");

    // Free a -> full pool restored
    xfree(a);
    AllocatorStats s_full = get_allocator_stats();
    TEST_ASSERT(s_full.free_units == 2048, "Full pool restored to 2048 free units");
    TEST_ASSERT(s_full.largest_free_block_units == 2048, "Largest free block is full 2048 units");
}

void test_reuse_freed_memory() {
    std::cout << "\n=== Running: test_reuse_freed_memory ===\n";
    reset_pool();

    void* p1 = xmalloc(4096); // 4 units
    void* p2 = xmalloc(8192); // 8 units

    TEST_ASSERT(p1 && p2, "Allocated p1 and p2");
    xfree(p1);

    // New allocation that fits into p1
    void* p3 = xmalloc(2048); // 2 units
    TEST_ASSERT(p3 == p1, "Freed block p1 is deterministically reused for p3");

    AllocatorStats stats = get_allocator_stats();
    TEST_ASSERT(stats.reused_alloc_count >= 1, "Reuse counter accurately incremented");

    xfree(p2);
    xfree(p3);
}

void test_error_handling_and_exhaustion() {
    std::cout << "\n=== Running: test_error_handling_and_exhaustion ===\n";
    reset_pool();

    // 0 bytes
    void* zero = xmalloc(0);
    TEST_ASSERT(zero == nullptr, "0 bytes request rejected safely");

    // Oversized request (> 2 MiB)
    void* over = xmalloc(3 * 1024 * 1024);
    TEST_ASSERT(over == nullptr, "Oversized request > 2 MiB rejected safely");

    // Allocate entire pool
    void* full = xmalloc(2048 * 1024);
    TEST_ASSERT(full != nullptr, "Entire 2048 KiB allocated successfully");

    AllocatorStats s_full = get_allocator_stats();
    TEST_ASSERT(s_full.allocated_units == 2048, "Pool is 100% allocated");
    TEST_ASSERT(s_full.free_units == 0, "0 free units remain");

    // Exhaustion failure
    void* extra = xmalloc(1024);
    TEST_ASSERT(extra == nullptr, "Allocation on exhausted pool returns nullptr safely");

    // Free full pool
    bool f_ok = xfree(full);
    TEST_ASSERT(f_ok, "Freed entire pool successfully");
}

void test_pointer_validation_and_double_free() {
    std::cout << "\n=== Running: test_pointer_validation_and_double_free ===\n";
    reset_pool();

    void* valid = xmalloc(4096);
    TEST_ASSERT(valid != nullptr, "Allocated valid pointer");

    // Foreign pointers
    int stack_var = 42;
    TEST_ASSERT(!xfree(&stack_var), "Stack pointer rejected safely");
    TEST_ASSERT(!xfree(nullptr), "nullptr rejected safely");

    // Unaligned pointer inside pool
    uint8_t* unaligned = static_cast<uint8_t*>(valid) + 17;
    TEST_ASSERT(!xfree(unaligned), "Unaligned pointer inside pool rejected safely");

    // Middle-of-block aligned pointer (e.g. + 1024 bytes)
    uint8_t* mid_block = static_cast<uint8_t*>(valid) + 1024;
    TEST_ASSERT(!xfree(mid_block), "Middle-of-block pointer rejected safely");

    // Valid free
    TEST_ASSERT(xfree(valid), "Valid free succeeds");

    // Double free
    TEST_ASSERT(!xfree(valid), "Double free detected and rejected safely");

    AllocatorStats stats = get_allocator_stats();
    TEST_ASSERT(stats.rejected_frees == 5, "All 5 invalid frees counted in statistics");
}

void test_strategy_behavior() {
    std::cout << "\n=== Running: test_strategy_behavior ===\n";
    // First-Fit
    reset_pool();
    set_allocation_strategy(AllocationStrategy::FIRST_FIT);

    void* b1 = xmalloc(32 * 1024); // 32 units
    void* s1 = xmalloc(1024);      // Separator
    void* b2 = xmalloc(8 * 1024);  // 8 units
    void* s2 = xmalloc(1024);      // Separator
    (void)s1; (void)s2;

    xfree(b1);
    xfree(b2);

    // Allocate 8 units: First-Fit will pick b1 (first free block >= 8)
    void* ff_choice = xmalloc(8 * 1024);
    TEST_ASSERT(ff_choice == b1, "First-Fit selects first suitable block (b1)");

    // Best-Fit
    reset_pool();
    set_allocation_strategy(AllocationStrategy::BEST_FIT);

    void* bf_b1 = xmalloc(32 * 1024);
    void* bf_s1 = xmalloc(1024);
    void* bf_b2 = xmalloc(8 * 1024);
    void* bf_s2 = xmalloc(1024);
    (void)bf_s1; (void)bf_s2;

    xfree(bf_b1);
    xfree(bf_b2);

    // Allocate 8 units: Best-Fit will pick bf_b2 (exact match)
    void* bf_choice = xmalloc(8 * 1024);
    TEST_ASSERT(bf_choice == bf_b2, "Best-Fit selects exact matching block (bf_b2)");
}

void test_leak_detection() {
    std::cout << "\n=== Running: test_leak_detection ===\n";
    reset_pool();

    void* l1 = xmalloc(500);  // 1 unit, 500 B
    void* l2 = xmalloc(2048); // 2 units, 2048 B
    void* clean = xmalloc(1024);

    xfree(clean);

    auto leaks = get_active_leaks();
    TEST_ASSERT(leaks.size() == 2, "Accurately detects 2 active leaks");
    TEST_ASSERT(leaks[0].requested_bytes == 500 && leaks[0].unit_count == 1, "Leak 1 metadata accurate");
    TEST_ASSERT(leaks[1].requested_bytes == 2048 && leaks[1].unit_count == 2, "Leak 2 metadata accurate");

    xfree(l1);
    xfree(l2);
    auto no_leaks = get_active_leaks();
    TEST_ASSERT(no_leaks.empty(), "Zero leaks after freeing all blocks");
}

void test_strategy_advisor() {
    std::cout << "\n=== Running: test_strategy_advisor (Tier-2) ===\n";
    auto report = run_strategy_advisor("fragmented_churn");
    TEST_ASSERT(report.total_requests == 400, "Advisor workload contains exactly 400 requests");
    TEST_ASSERT(report.first_fit_metrics.successful_allocations + report.first_fit_metrics.failed_allocations == 400,
                "First-Fit total accounted operations equals 400");
    TEST_ASSERT(report.best_fit_metrics.successful_allocations + report.best_fit_metrics.failed_allocations == 400,
                "Best-Fit total accounted operations equals 400");
    TEST_ASSERT(!report.observed_result.empty(), "Observed result analysis is generated");
    TEST_ASSERT(!report.tradeoff_analysis.empty(), "Trade-off explanation is generated");

    // Test burst cycles workload
    auto burst_report = run_strategy_advisor("burst_cycles");
    TEST_ASSERT(burst_report.total_requests == 250, "Burst workload contains exactly 250 requests");
    TEST_ASSERT(burst_report.first_fit_metrics.successful_allocations == 250, "First-Fit satisfies all 250 burst requests");
    TEST_ASSERT(burst_report.best_fit_metrics.successful_allocations == 250, "Best-Fit satisfies all 250 burst requests");
}

void test_embedded_simulator() {
    std::cout << "\n=== Running: test_embedded_simulator (Embedded Workload) ===\n";
    EmbeddedWorkloadSimulator sim;
    EmbeddedSimulationEvidence ev = sim.run_full_simulation(false);

    TEST_ASSERT(ev.deterministic_workload_pass, "Embedded simulator runs deterministic workload");
    TEST_ASSERT(ev.allocator_backed_buffers_pass, "Embedded components backed by custom pool");
    TEST_ASSERT(ev.fragmentation_demonstrated_pass, "Fragmentation event successfully created");
    TEST_ASSERT(ev.memory_reuse_demonstrated_pass, "Memory reuse detected and verified");
    TEST_ASSERT(ev.coalescing_demonstrated_pass, "Bidirectional coalescing merges adjacent free regions");
    TEST_ASSERT(ev.memory_pressure_detected_pass, "Memory pressure successfully classified");
    TEST_ASSERT(ev.controlled_exhaustion_pass, "Controlled exhaustion safely returns nullptr");
    TEST_ASSERT(ev.recovery_demonstrated_pass, "Full pool capacity restored upon teardown");
    TEST_ASSERT(ev.final_free_kib == 2048, "Final free memory is full 2048 KiB");
    TEST_ASSERT(ev.final_largest_free_kib == 2048, "Final largest free block is full 2048 KiB");
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << "       RUNNING CUSTOM ALLOCATOR TEST SUITE             \n";
    std::cout << "=======================================================\n";

    test_initialization();
    test_unit_rounding();
    test_splitting_and_coalescing();
    test_reuse_freed_memory();
    test_error_handling_and_exhaustion();
    test_pointer_validation_and_double_free();
    test_strategy_behavior();
    test_leak_detection();
    test_strategy_advisor();
    test_embedded_simulator();

    std::cout << "\n=======================================================\n";
    std::cout << " TEST SUMMARY: " << g_pass_count << " PASSED, " << g_fail_count << " FAILED\n";
    std::cout << "=======================================================\n";

    return (g_fail_count == 0) ? 0 : 1;
}

