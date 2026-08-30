#ifndef STRATEGY_H
#define STRATEGY_H

#include "allocator_types.h"
#include <vector>

namespace Allocator {

class IAllocationStrategy {
public:
    virtual ~IAllocationStrategy() = default;
    
    // Returns index in blocks vector of chosen free block, or -1 if no suitable block found.
    // Also accumulates search_steps_out with the number of blocks examined.
    virtual int find_free_block(const std::vector<BlockDescriptor>& blocks,
                                size_t units_needed,
                                size_t& search_steps_out) const = 0;
                                
    virtual const char* get_name() const = 0;
    virtual AllocationStrategy get_strategy_type() const = 0;
};

class FirstFitStrategy : public IAllocationStrategy {
public:
    int find_free_block(const std::vector<BlockDescriptor>& blocks,
                        size_t units_needed,
                        size_t& search_steps_out) const override;
                        
    const char* get_name() const override { return "First-Fit"; }
    AllocationStrategy get_strategy_type() const override { return AllocationStrategy::FIRST_FIT; }
};

class BestFitStrategy : public IAllocationStrategy {
public:
    int find_free_block(const std::vector<BlockDescriptor>& blocks,
                        size_t units_needed,
                        size_t& search_steps_out) const override;
                        
    const char* get_name() const override { return "Best-Fit"; }
    AllocationStrategy get_strategy_type() const override { return AllocationStrategy::BEST_FIT; }
};

} // namespace Allocator

#endif // STRATEGY_H
