#ifndef EMBEDDED_SIMULATOR_H
#define EMBEDDED_SIMULATOR_H

#include "allocator.h"
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace Allocator {

enum class MemoryPressureLevel {
    LOW,
    MODERATE,
    HIGH,
    CRITICAL
};

struct EmbeddedComponent {
    std::string name;
    size_t requested_bytes;
    size_t reserved_bytes;
    void* ptr;
    bool is_active;
    uint64_t alloc_id;
    size_t start_unit;
    size_t unit_count;
};

struct EmbeddedSimulationEvidence {
    bool deterministic_workload_pass;
    bool allocator_backed_buffers_pass;
    bool fragmentation_demonstrated_pass;
    bool memory_reuse_demonstrated_pass;
    bool coalescing_demonstrated_pass;
    bool memory_pressure_detected_pass;
    bool controlled_exhaustion_pass;
    bool recovery_demonstrated_pass;
    size_t pool_capacity_kib;
    size_t final_free_kib;
    size_t final_largest_free_kib;
    size_t total_allocation_failures;
    size_t total_reuse_events;
};

class EmbeddedWorkloadSimulator {
public:
    EmbeddedWorkloadSimulator();
    ~EmbeddedWorkloadSimulator();

    // Execute the complete 10-stage embedded workload demonstration
    EmbeddedSimulationEvidence run_full_simulation(bool verbose = true);

    // Run comparison of the embedded workload across First-Fit and Best-Fit
    void run_strategy_comparison();

    // Determine deterministic memory pressure level from allocator stats
    static MemoryPressureLevel compute_memory_pressure(const AllocatorStats& stats);
    static const char* pressure_to_string(MemoryPressureLevel level);

    // Component lifecycle actions using custom pool
    bool allocate_component(const std::string& name, size_t bytes);
    bool release_component(const std::string& name);
    bool release_component_by_ptr(void* ptr);

    // Observability & Dashboard
    void print_dashboard(const std::string& system_state_label) const;
    void print_evidence_summary(const EmbeddedSimulationEvidence& evidence) const;

    const std::vector<EmbeddedComponent>& get_components() const { return components_; }

private:
    std::vector<EmbeddedComponent> components_;
};

// Global helper for CLI and demo runners
void run_embedded_demo();

} // namespace Allocator

#endif // EMBEDDED_SIMULATOR_H
