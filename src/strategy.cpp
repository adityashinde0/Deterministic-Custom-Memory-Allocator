#include "strategy.h"
#include <limits>

namespace Allocator {

int FirstFitStrategy::find_free_block(const std::vector<BlockDescriptor>& blocks,
                                     size_t units_needed,
                                     size_t& search_steps_out) const {
    for (size_t i = 0; i < blocks.size(); ++i) {
        ++search_steps_out;
        if (blocks[i].state == BlockState::FREE && blocks[i].unit_count >= units_needed) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int BestFitStrategy::find_free_block(const std::vector<BlockDescriptor>& blocks,
                                    size_t units_needed,
                                    size_t& search_steps_out) const {
    int best_index = -1;
    size_t min_excess_units = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < blocks.size(); ++i) {
        ++search_steps_out;
        if (blocks[i].state == BlockState::FREE && blocks[i].unit_count >= units_needed) {
            size_t excess = blocks[i].unit_count - units_needed;
            if (excess < min_excess_units) {
                min_excess_units = excess;
                best_index = static_cast<int>(i);
                // Perfect fit optimization: cannot do better than 0 excess
                if (excess == 0) {
                    break;
                }
            }
        }
    }
    return best_index;
}

} // namespace Allocator
