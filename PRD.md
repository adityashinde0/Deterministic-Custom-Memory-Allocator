1. Problem Summary & Core Value Proposition
Build a C++ custom allocator that manages a fixed 2 MiB pool as 2048 × 1 KiB units, supporting allocation, deallocation, reuse, fragmentation management, and leak reporting without relying on the system allocator for pool operations. The MVP compares deterministic allocation strategies while maintaining allocator correctness and observable memory state. Wow: demonstrate freed memory being deterministically reclaimed and reused inside a constrained 2 MiB pool.
2. Assumptions
- 1 KiB is the allocation granularity; requests are rounded up to whole 1 KiB units while the pool remains exactly 2048 units.
- Metadata is maintained outside the 2 MiB pool so the complete pool remains available for allocations.
- The MVP is single-threaded; thread safety is not an explicit problem-statement requirement.
3. Personas & Key Journeys
- Allocator user/developer: calls xmalloc()/xfree(); edge case: invalid or double free must be rejected without corrupting allocator state.
- Allocator evaluator: creates allocation/free patterns and inspects reuse and fragmentation; edge case: sufficient total free memory exists but no sufficiently large contiguous region exists.
- Benchmark/demo operator: compares strategies and requests diagnostics; edge case: pool exhaustion must produce a controlled allocation failure rather than crash.
4. MVP Scope (Tier-1 only, 12h)
- [Technical Excellence] Fixed 2 MiB pool with 2048 × 1 KiB allocation units and externally maintained metadata.
- [Technical Excellence] xmalloc()/xfree() with request rounding, contiguous allocation, pointer validation, and safe failure handling.
- [Innovation] First-Fit and Best-Fit allocation strategies selectable through a common strategy interface.
- [Technical Excellence] Free-region splitting, adjacent-region coalescing, and measurable fragmentation/reuse statistics.
- [Impact] Memory leak checker reporting active allocations and block metadata.
- [Feasibility] Deterministic CLI test/benchmark harness covering allocation, reuse, fragmentation, exhaustion, invalid free, double free, and strategy comparison.
Wishlist
- Buddy-system strategy.
- Optional mutex-based thread safety.
- Richer allocation-map visualization.
- Additional workload generators.
5. Success Metric
Demonstrate reproducibly that the allocator passes all required correctness scenarios while reporting allocation success/failure, freed-memory reuse, total free units, largest contiguous free region, and strategy search work without exceeding the fixed 2 MiB pool.
6. Demo Script
- Allocate, fragment, free, reuse, compare strategies, trigger failure, then dump live allocations and fragmentation metrics.