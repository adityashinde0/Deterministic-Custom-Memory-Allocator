#include "allocator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <map>

using namespace Allocator;

void print_separator(const std::string& title) {
    std::cout << "\n=======================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "=======================================================\n";
}

void print_stats_summary(const AllocatorStats& stats) {
    std::cout << "\n--- Allocator Diagnostics ---\n";
    std::cout << " Strategy:                  " << (stats.current_strategy == AllocationStrategy::FIRST_FIT ? "First-Fit" : "Best-Fit") << "\n";
    std::cout << " Total Pool Units:          " << stats.total_units << " (" << (stats.total_pool_bytes / 1024 / 1024) << " MiB)\n";
    std::cout << " Allocated Units:           " << stats.allocated_units << " (" << stats.allocated_units << " KiB)\n";
    std::cout << " Free Units:                " << stats.free_units << " (" << stats.free_units << " KiB)\n";
    std::cout << " Active Allocations:        " << stats.active_allocations << "\n";
    std::cout << " Largest Contiguous Free:   " << stats.largest_free_block_units << " KiB\n";
    std::cout << " Internal Waste (Slack):    " << stats.internal_waste_bytes << " bytes\n";
    std::cout << " External Fragmentation:    " << std::fixed << std::setprecision(2) << (stats.external_fragmentation_ratio * 100.0) << " %\n";
    std::cout << " Total Allocation Requests: " << stats.total_alloc_requests << " (Success: " << stats.total_alloc_successes << ", Fail: " << stats.total_alloc_failures << ")\n";
    std::cout << " Reused Allocations:        " << stats.reused_alloc_count << "\n";
    std::cout << " Free Calls / Rejections:   " << stats.total_free_calls << " / " << stats.rejected_frees << "\n";
    std::cout << " Cumulative Strategy Steps: " << stats.strategy_search_steps << "\n";
    std::cout << "-----------------------------\n";
}

void run_automated_demo() {
    print_separator("DEMO SCENARIO 1: Basic Allocation, Unit Rounding & Reuse");
    initialize_pool();
    reset_pool();

    std::cout << "[1] Requesting 500 bytes (Expect 1 KiB unit rounded)...\n";
    void* p1 = xmalloc(500);
    std::cout << "    -> Result pointer: " << p1 << "\n";

    std::cout << "[2] Requesting 2500 bytes (Expect 3 KiB units rounded)...\n";
    void* p2 = xmalloc(2500);
    std::cout << "    -> Result pointer: " << p2 << "\n";

    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    std::cout << "\n[3] Freeing first block (p1 = 500 B / 1 KiB unit)...\n";
    bool f1 = xfree(p1);
    std::cout << "    -> xfree result: " << (f1 ? "SUCCESS" : "FAILED") << "\n";

    std::cout << "[4] Allocating new 800 bytes (Should reuse the 1 KiB freed slot)...\n";
    void* p3 = xmalloc(800);
    std::cout << "    -> Result pointer: " << p3 << " (Matches p1: " << (p3 == p1 ? "YES - REUSED!" : "NO") << ")\n";

    dump_pool_layout();
    print_stats_summary(get_allocator_stats());


    print_separator("DEMO SCENARIO 2: Splitting and Adjacent Coalescing");
    reset_pool();

    std::cout << "[1] Allocating 4 contiguous blocks: A (4 KiB), B (4 KiB), C (4 KiB), D (4 KiB)...\n";
    void* pA = xmalloc(4 * 1024);
    void* pB = xmalloc(4 * 1024);
    void* pC = xmalloc(4 * 1024);
    void* pD = xmalloc(4 * 1024);

    dump_pool_layout();

    std::cout << "\n[2] Freeing block B (4 KiB) and D (4 KiB) to create fragmented gaps...\n";
    xfree(pB);
    xfree(pD);
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    std::cout << "\n[3] Freeing block C (4 KiB) -> Should coalesce with B (left) and D (right) into single 12 KiB block...\n";
    xfree(pC);
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    std::cout << "\n[4] Freeing block A (4 KiB) -> Should coalesce with remainder pool into whole 2048 KiB free pool...\n";
    xfree(pA);
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());


    print_separator("DEMO SCENARIO 3: First-Fit vs Best-Fit Strategy Comparison");
    for (int strat = 0; strat < 2; ++strat) {
        AllocationStrategy s = (strat == 0) ? AllocationStrategy::FIRST_FIT : AllocationStrategy::BEST_FIT;
        reset_pool();
        set_allocation_strategy(s);

        std::cout << "\n--- Running on Strategy: " << (strat == 0 ? "FIRST-FIT" : "BEST-FIT") << " ---\n";
        void* b1 = xmalloc(64 * 1024);  // Gap 1: 64 KiB
        void* sep1 = xmalloc(1024);     // Separator
        void* b2 = xmalloc(16 * 1024);  // Gap 2: 16 KiB
        void* sep2 = xmalloc(1024);     // Separator
        void* b3 = xmalloc(128 * 1024); // Gap 3: 128 KiB
        void* sep3 = xmalloc(1024);     // Separator
        void* b4 = xmalloc(32 * 1024);  // Gap 4: 32 KiB
        void* sep4 = xmalloc(1024);     // Separator
        (void)sep1; (void)sep2; (void)sep3; (void)sep4;

        // Create the free gaps
        xfree(b1);
        xfree(b2);
        xfree(b3);
        xfree(b4);

        std::cout << "Layout with gaps of 64 KiB, 16 KiB, 128 KiB, 32 KiB:\n";
        dump_pool_layout();

        std::cout << "Now requesting allocation of 16 KiB (16 units)...\n";
        void* chosen = xmalloc(16 * 1024);
        std::cout << "Chosen pointer: " << chosen << "\n";
        if (strat == 0) {
            std::cout << "First-Fit result: Placed into first block (64 KiB gap at start)\n";
        } else {
            std::cout << "Best-Fit result: Placed into exact 16 KiB gap (b2)\n";
            std::cout << "Matched b2: " << (chosen == b2 ? "YES (Exact Best-Fit placement!)" : "NO") << "\n";
        }

        dump_pool_layout();
        print_stats_summary(get_allocator_stats());
    }


    print_separator("DEMO SCENARIO 4: Error Handling & Edge Case Safety");
    reset_pool();

    std::cout << "[1] Requesting 0 bytes (Invalid size)...\n";
    void* zero_ptr = xmalloc(0);
    std::cout << "    -> Result: " << (zero_ptr == nullptr ? "REJECTED SAFELY (nullptr)" : "FAILED") << "\n";

    std::cout << "[2] Requesting 3 MiB (Oversized, exceeds 2 MiB pool)...\n";
    void* over_ptr = xmalloc(3 * 1024 * 1024);
    std::cout << "    -> Result: " << (over_ptr == nullptr ? "REJECTED SAFELY (nullptr)" : "FAILED") << "\n";

    std::cout << "[3] Valid allocation of 2048 KiB (Entire pool)...\n";
    void* full_ptr = xmalloc(2048 * 1024);
    std::cout << "    -> Result: " << full_ptr << "\n";

    std::cout << "[4] Requesting another 1 KiB while pool is fully exhausted...\n";
    void* exhaust_ptr = xmalloc(1024);
    std::cout << "    -> Result: " << (exhaust_ptr == nullptr ? "CONTROLLED EXHAUSTION FAILURE (nullptr)" : "FAILED") << "\n";

    std::cout << "[5] Attempting to free unaligned / middle-of-block pointer...\n";
    uint8_t* mid_ptr = static_cast<uint8_t*>(full_ptr) + 500;
    bool mid_free = xfree(mid_ptr);
    std::cout << "    -> Result: " << (!mid_free ? "REJECTED SAFELY" : "FAILED") << "\n";

    std::cout << "[6] Freeing full pool pointer...\n";
    bool full_free = xfree(full_ptr);
    std::cout << "    -> Result: " << (full_free ? "SUCCESS" : "FAILED") << "\n";

    std::cout << "[7] Attempting Double Free on already freed pointer...\n";
    bool double_free = xfree(full_ptr);
    std::cout << "    -> Result: " << (!double_free ? "DOUBLE FREE DETECTED AND REJECTED SAFELY" : "FAILED") << "\n";

    print_stats_summary(get_allocator_stats());


    print_separator("DEMO SCENARIO 5: Memory Leak Checker Report");
    reset_pool();

    std::cout << "Simulating leaked allocations...\n";
    void* leak1 = xmalloc(1200);   // Alloc 1 (2 KiB reserved, 1200 B requested)
    void* leak2 = xmalloc(64000);  // Alloc 2 (63 KiB reserved, 64000 B requested)
    void* freed = xmalloc(4096);   // Alloc 3 (Properly freed)
    void* leak3 = xmalloc(800);    // Alloc 4 (1 KiB reserved, 800 B requested)
    (void)leak1; (void)leak2; (void)leak3;

    xfree(freed);

    dump_leaks();
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());
    std::cout << "\nAll demo scenarios completed successfully!\n";
}

void run_interactive_mode() {
    initialize_pool();
    std::map<int, void*> active_allocs;
    int next_handle = 1;

    std::cout << "\n=======================================================\n";
    std::cout << "        CUSTOM ALLOCATOR INTERACTIVE CONSOLE           \n";
    std::cout << "=======================================================\n";
    std::cout << " Commands:\n";
    std::cout << "   alloc <bytes>       - Allocate <bytes> memory\n";
    std::cout << "   free <handle_id>    - Free block by handle ID\n";
    std::cout << "   strategy <ff|bf>    - Set strategy (ff=First-Fit, bf=Best-Fit)\n";
    std::cout << "   layout              - Display ASCII pool layout map\n";
    std::cout << "   stats               - Print allocator diagnostics & stats\n";
    std::cout << "   leaks               - Print memory leak report\n";
    std::cout << "   list                - List active tracked handles\n";
    std::cout << "   reset               - Reset pool to clean state\n";
    std::cout << "   demo                - Run full automated demo\n";
    std::cout << "   exit / quit         - Exit console\n";
    std::cout << "=======================================================\n";

    std::string line;
    while (true) {
        std::cout << "\nallocator> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "alloc") {
            size_t bytes = 0;
            if (ss >> bytes) {
                void* ptr = xmalloc(bytes);
                if (ptr) {
                    int handle = next_handle++;
                    active_allocs[handle] = ptr;
                    std::cout << " [SUCCESS] Allocated " << bytes << " bytes. Handle: #" << handle << " -> Pointer: " << ptr << "\n";
                } else {
                    std::cout << " [FAILED] Allocation of " << bytes << " bytes failed.\n";
                }
            } else {
                std::cout << " Usage: alloc <bytes>\n";
            }
        } else if (cmd == "free") {
            int handle = 0;
            if (ss >> handle) {
                auto it = active_allocs.find(handle);
                if (it != active_allocs.end()) {
                    bool ok = xfree(it->second);
                    if (ok) {
                        std::cout << " [SUCCESS] Freed block #" << handle << " (" << it->second << ")\n";
                        active_allocs.erase(it);
                    } else {
                        std::cout << " [FAILED] xfree rejected pointer.\n";
                    }
                } else {
                    std::cout << " [ERROR] Handle #" << handle << " not found in active list.\n";
                }
            } else {
                std::cout << " Usage: free <handle_id>\n";
            }
        } else if (cmd == "strategy") {
            std::string s;
            if (ss >> s) {
                if (s == "ff" || s == "first") {
                    set_allocation_strategy(AllocationStrategy::FIRST_FIT);
                    std::cout << " [OK] Strategy set to FIRST-FIT.\n";
                } else if (s == "bf" || s == "best") {
                    set_allocation_strategy(AllocationStrategy::BEST_FIT);
                    std::cout << " [OK] Strategy set to BEST-FIT.\n";
                } else {
                    std::cout << " Unknown strategy. Use 'ff' or 'bf'.\n";
                }
            } else {
                std::cout << " Current strategy: " << (get_allocation_strategy() == AllocationStrategy::FIRST_FIT ? "FIRST-FIT" : "BEST-FIT") << "\n";
            }
        } else if (cmd == "layout") {
            dump_pool_layout();
        } else if (cmd == "stats") {
            print_stats_summary(get_allocator_stats());
        } else if (cmd == "leaks") {
            dump_leaks();
        } else if (cmd == "list") {
            std::cout << "--- Active Tracked Handles ---\n";
            if (active_allocs.empty()) {
                std::cout << " (No active handles)\n";
            } else {
                for (const auto& kv : active_allocs) {
                    std::cout << "  Handle #" << kv.first << " -> " << kv.second << "\n";
                }
            }
        } else if (cmd == "reset") {
            reset_pool();
            active_allocs.clear();
            std::cout << " [OK] Pool and handles reset.\n";
        } else if (cmd == "demo") {
            run_automated_demo();
            active_allocs.clear();
        } else {
            std::cout << " Unknown command: '" << cmd << "'. Type 'layout', 'stats', 'alloc <n>', 'free <id>', 'leaks', 'demo', or 'exit'.\n";
        }
    }
}

void print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --demo            Run the 5-scenario automated demo (default)\n";
    std::cout << "  --interactive, -i Start interactive CLI shell\n";
    std::cout << "  --stats           Display initial pool statistics\n";
    std::cout << "  --leaks           Run leak checker report\n";
    std::cout << "  --help, -h        Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--interactive" || arg == "-i") {
            run_interactive_mode();
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--stats") {
            initialize_pool();
            print_stats_summary(get_allocator_stats());
            return 0;
        } else if (arg == "--leaks") {
            initialize_pool();
            dump_leaks();
            return 0;
        } else if (arg == "--demo") {
            run_automated_demo();
            return 0;
        }
    }

    // Default behavior
    run_automated_demo();
    return 0;
}
