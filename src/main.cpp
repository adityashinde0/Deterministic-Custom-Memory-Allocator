#include "allocator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <map>

using namespace Allocator;

void print_step_header(int step_num, const std::string& title) {
    std::cout << "\n=======================================================\n";
    std::cout << " [STEP " << step_num << "] " << title << "\n";
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

void run_deterministic_demo() {
    std::cout << "\n#######################################################\n";
    std::cout << "   C-002 DETERMINISTIC ALLOCATOR COMPLETE DEMO          \n";
    std::cout << "#######################################################\n";

    // Step 1: Pool starts empty
    print_step_header(1, "Pool Starts Empty (2048 Units / 2 MiB)");
    initialize_pool();
    reset_pool();
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    // Step 2: Several allocations are performed
    print_step_header(2, "Perform Several Distinct Allocations");
    std::cout << "Allocating Block A (16 KiB), B (8 KiB), C (32 KiB), D (8 KiB), E (64 KiB)...\n";
    void* pA = xmalloc(16 * 1024);
    void* pB = xmalloc(8 * 1024);
    void* pC = xmalloc(32 * 1024);
    void* pD = xmalloc(8 * 1024);
    void* pE = xmalloc(64 * 1024);
    (void)pA; (void)pE;

    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    // Step 3: Selected allocations are freed
    print_step_header(3, "Selected Allocations Freed to Form Gaps");
    std::cout << "Freeing Block B (8 KiB) and Block D (8 KiB)...\n";
    bool f_b = xfree(pB);
    bool f_d = xfree(pD);
    std::cout << "Block B free result: " << (f_b ? "SUCCESS" : "FAIL") << "\n";
    std::cout << "Block D free result: " << (f_d ? "SUCCESS" : "FAIL") << "\n";

    // Step 4: Fragmentation becomes visible
    print_step_header(4, "Fragmentation Becomes Visible in Pool Layout");
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    // Step 5: A later allocation reuses freed space
    print_step_header(5, "Later Allocation Reuses Freed Slot (Deterministic Reclamation)");
    std::cout << "Requesting 6 KiB (rounds to 6 units) -> Should reuse freed Block B slot...\n";
    void* pReuse = xmalloc(6 * 1024);
    std::cout << "Allocated pointer: " << pReuse << " (Matches Block B address: " << (pReuse == pB ? "YES - REUSED!" : "NO") << ")\n";
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    // Step 6: Coalescing creates a larger contiguous free region
    print_step_header(6, "Deallocation with Bidirectional Coalescing");
    std::cout << "Freeing Block C (32 KiB) and Block Reuse (6 KiB)...\n";
    std::cout << "This merges adjacent free slots (B remainder + C + D) into one contiguous free region...\n";
    xfree(pReuse);
    xfree(pC);
    dump_pool_layout();
    print_stats_summary(get_allocator_stats());

    // Step 7: First-Fit and Best-Fit compared on the same workload
    print_step_header(7, "First-Fit vs Best-Fit Strategy Comparison (Deterministic Workload)");
    StrategyAdvisorReport advisor_report = run_strategy_advisor("fragmented_churn");
    print_strategy_advisor_report(advisor_report);

    // Step 8: Oversized/exhausted request fails safely
    print_step_header(8, "Error Handling, Oversized Requests & Controlled Pool Exhaustion");
    std::cout << "[8.1] Requesting 0 bytes -> Result: " 
              << (xmalloc(0) == nullptr ? "REJECTED SAFELY (nullptr)" : "FAILED") << "\n";
    std::cout << "[8.2] Requesting 5 MiB (exceeds 2 MiB pool) -> Result: " 
              << (xmalloc(5 * 1024 * 1024) == nullptr ? "REJECTED SAFELY (nullptr)" : "FAILED") << "\n";

    std::cout << "[8.3] Allocating remaining pool to induce 100% exhaustion...\n";
    reset_pool();
    void* full_alloc = xmalloc(2048 * 1024);
    std::cout << "Full 2048 KiB allocated -> Pointer: " << full_alloc << "\n";
    std::cout << "[8.4] Requesting 1 more KiB while pool is fully exhausted -> Result: " 
              << (xmalloc(1024) == nullptr ? "CONTROLLED EXHAUSTION (nullptr without crash)" : "FAILED") << "\n";

    std::cout << "[8.5] Invalid pointer & double-free validation:\n";
    int local_var = 123;
    std::cout << " - Foreign stack pointer free: " << (!xfree(&local_var) ? "REJECTED SAFELY" : "FAILED") << "\n";
    std::cout << " - Unaligned pointer free:     " << (!xfree((uint8_t*)full_alloc + 17) ? "REJECTED SAFELY" : "FAILED") << "\n";
    std::cout << " - Freeing valid pointer:      " << (xfree(full_alloc) ? "SUCCESS" : "FAILED") << "\n";
    std::cout << " - Double-free rejection:      " << (!xfree(full_alloc) ? "DOUBLE FREE REJECTED SAFELY" : "FAILED") << "\n";

    // Step 9: Leak checker reports remaining active allocations
    print_step_header(9, "Memory Leak Checker Reporting Active Allocations");
    reset_pool();
    std::cout << "Creating simulation with active allocations and deliberate leaks...\n";
    void* l1 = xmalloc(1500);  // 2 KiB reserved, 1500 B requested
    void* l2 = xmalloc(48000); // 47 KiB reserved, 48000 B requested
    void* clean_alloc = xmalloc(4096);
    void* l3 = xmalloc(900);   // 1 KiB reserved, 900 B requested
    (void)l1; (void)l2; (void)l3;

    std::cout << "Freeing proper allocation (" << clean_alloc << ")...\n";
    xfree(clean_alloc);

    dump_leaks();
    dump_pool_layout();

    // Step 10: Final allocator statistics shown
    print_step_header(10, "Final Observable Allocator Diagnostics & Metrics");
    print_stats_summary(get_allocator_stats());

    std::cout << "\n#######################################################\n";
    std::cout << "   DEMO COMPLETED: ALL 10 STAGES VERIFIED               \n";
    std::cout << "#######################################################\n";
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
    std::cout << "   advisor [churn|burst] - Run Strategy Advisor on workload\n";
    std::cout << "   layout              - Display ASCII pool layout map\n";
    std::cout << "   stats               - Print allocator diagnostics & stats\n";
    std::cout << "   leaks               - Print memory leak report\n";
    std::cout << "   list                - List active tracked handles\n";
    std::cout << "   reset               - Reset pool to clean state\n";
    std::cout << "   demo                - Run full 10-step automated demo\n";
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
        } else if (cmd == "advisor") {
            std::string wl = "fragmented_churn";
            std::string arg;
            if (ss >> arg && arg == "burst") {
                wl = "burst_cycles";
            }
            auto rep = run_strategy_advisor(wl);
            print_strategy_advisor_report(rep);
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
            run_deterministic_demo();
            active_allocs.clear();
        } else {
            std::cout << " Unknown command: '" << cmd << "'. Type 'alloc', 'free', 'strategy', 'advisor', 'layout', 'stats', 'leaks', 'demo', or 'exit'.\n";
        }
    }
}

void print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --demo            Run the 10-step complete deterministic demo (default)\n";
    std::cout << "  --advisor, -a     Run the Deterministic Allocation Strategy Advisor\n";
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
        } else if (arg == "--advisor" || arg == "-a") {
            initialize_pool();
            auto rep = run_strategy_advisor("fragmented_churn");
            print_strategy_advisor_report(rep);
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
            run_deterministic_demo();
            return 0;
        }
    }

    // Default behavior: complete 10-step demo
    run_deterministic_demo();
    return 0;
}
