#pragma once
// Index and tag computation for the tagged tables.
//
// Deliberately separated from prediction logic: indexing is the part the 2025
// submission got wrong, and it is testable with no predictor state at all.
//
// The tag is a hash of PC and this table's folded history. It was briefly
// stubbed out as the 2025 CA1 tag function so that tag_uniqueness_test could
// be run against the actual defect; see git history for that failing state.

#include <cstddef>
#include <cstdint>
#include "tage_config.h"
#include "global_history.h"
#include "folded_history.h"

namespace tage {

class TageIndexer {
public:
    TageIndexer() {
        for (std::size_t i = 0; i < NUM_TAGGED; ++i) {
            idx_fold_[i].init(HIST_LEN[i], TAGGED_IDX_BITS);
            tag_fold_a_[i].init(HIST_LEN[i], TAG_FOLD_A_BITS);
			tag_fold_b_[i].init(HIST_LEN[i], TAG_FOLD_B_BITS);
            tag_fold_c_[i].init(HIST_LEN[i], TAG_FOLD_C_BITS);
        }
    }

    // Advance all folded registers by one conditional-branch outcome.
    void push(bool taken) {
        for (std::size_t i = 0; i < NUM_TAGGED; ++i) {
            // Sample the departing bit BEFORE the raw history moves.
            const bool outgoing = hist_.bit(HIST_LEN[i] - 1);
            idx_fold_[i].update(taken, outgoing);
            tag_fold_a_[i].update(taken, outgoing);
			tag_fold_b_[i].update(taken, outgoing);
            tag_fold_c_[i].update(taken, outgoing);
        }
        hist_.push(taken);
    }

    static constexpr std::uint32_t IDX_MASK =
        (std::uint32_t(1) << TAGGED_IDX_BITS) - 1;
    static constexpr std::uint32_t TAG_MASK =
        (std::uint32_t(1) << TAG_BITS) - 1;

    std::uint32_t index(std::size_t table, std::uint32_t pc) const {
        return ((pc >> 2) ^ idx_fold_[table].value()) & IDX_MASK;
    }

    // The tag answers: "was this entry allocated by the (PC, history) context
    // I am in right now?" It must therefore hash BOTH, and must differ per
    // table -- each table folds a different amount of history.
    //
    // Two folds at different widths (TAG_BITS and TAG_BITS-1), staggered by one
    // bit. If the tag reused the index's fold, an index collision would imply a
    // tag collision and the tag would carry no information at all.
    std::uint32_t tag(std::size_t table, std::uint32_t pc) const {
        const std::uint32_t p = pc >> 2;
        return (p
                ^  tag_fold_a_[table].value()
                ^ (tag_fold_b_[table].value() << 1)
                ^ (tag_fold_c_[table].value() << 2)) & TAG_MASK;
    }

    const GlobalHistory<MAX_HIST>& history() const { return hist_; }

private:
    GlobalHistory<MAX_HIST> hist_;
    FoldedHistory idx_fold_[NUM_TAGGED];
    FoldedHistory tag_fold_a_[NUM_TAGGED];
	FoldedHistory tag_fold_b_[NUM_TAGGED];
    FoldedHistory tag_fold_c_[NUM_TAGGED];
};

}  // namespace tage
