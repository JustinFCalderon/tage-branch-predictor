#pragma once
// TAGE tables and lookup.
//
// This file owns the storage and the parallel probe. Update rules, usefulness
// counters, and allocation come later -- keeping lookup separable is what makes
// provider/altpred selection testable on its own.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "tage_config.h"
#include "tage_indexer.h"

namespace tage {

// Signed saturating counter, CTR_BITS wide: -4..3 for 3 bits.
// Prediction is simply the sign: ctr >= 0 means taken.
inline constexpr int CTR_MIN = -(1 << (CTR_BITS - 1));
inline constexpr int CTR_MAX =  (1 << (CTR_BITS - 1)) - 1;
inline constexpr int U_MAX   =  (1 << U_BITS) - 1;

// Bimodal is unsigned, BIMODAL_CTR_BITS wide: 0..3, taken if >= 2.
inline constexpr int BIM_MAX  = (1 << BIMODAL_CTR_BITS) - 1;
inline constexpr int BIM_TAKEN_THRESHOLD = 1 << (BIMODAL_CTR_BITS - 1);

struct TaggedEntry {
    std::int8_t   ctr = 0;
    std::uint16_t tag = 0;
    std::uint8_t  u   = 0;
};

// Everything predict() computed that update() will need. In the CBP2 flow this
// rides from one call to the other inside the branch_update object, which is
// exactly what that indirection exists for.
struct Lookup {
    std::uint32_t idx[NUM_TAGGED] = {};
    std::uint32_t tg [NUM_TAGGED] = {};
    bool          hit[NUM_TAGGED] = {};

    int  provider      = -1;     // -1 means no tagged table matched
    int  altpred       = -1;     // -1 means the bimodal is the fallback
    bool provider_pred = false;
    bool alt_pred      = false;

    std::uint32_t bim_idx  = 0;
    bool          bim_pred = false;
};

class TagePredictor {
public:
    TagePredictor() { std::memset(bimodal_, 0, sizeof(bimodal_)); }

    Lookup lookup(std::uint32_t pc) const {
        Lookup L;

        L.bim_idx  = (pc >> 2) & (BIMODAL_ENTRIES - 1);
        L.bim_pred = bimodal_[L.bim_idx] >= BIM_TAKEN_THRESHOLD;

        // Probe every tagged table. In hardware these are simultaneous.
        //
        // Note there is no valid bit: a freshly-zeroed entry has tag 0 and will
        // falsely match a branch whose computed tag is 0. That is real TAGE
        // behaviour, not a shortcut -- a valid bit per entry costs more than the
        // 1-in-1024 false match it would prevent.
        for (std::size_t t = 0; t < NUM_TAGGED; ++t) {
            L.idx[t] = ix_.index(t, pc);
            L.tg [t] = ix_.tag  (t, pc);
            L.hit[t] = (tables_[t][L.idx[t]].tag == std::uint16_t(L.tg[t]));
        }

        // Walk from the longest history down: first match provides, second is
        // the altpred. This loop IS the "longest matching history wins" rule,
        // and it only means anything because tags let a table decline to match.
        for (int t = int(NUM_TAGGED) - 1; t >= 0; --t) {
            if (!L.hit[t]) continue;
            if (L.provider < 0)      L.provider = t;
            else                   { L.altpred  = t; break; }
        }

        L.provider_pred = (L.provider >= 0)
                        ? (tables_[L.provider][L.idx[L.provider]].ctr >= 0)
                        : L.bim_pred;
        L.alt_pred      = (L.altpred >= 0)
                        ? (tables_[L.altpred][L.idx[L.altpred]].ctr >= 0)
                        : L.bim_pred;
        return L;
    }

    bool predict(const Lookup& L) const { return L.provider_pred; }

    // Advance all folded histories. Conditional branches only.
    void push_history(bool taken) { ix_.push(taken); }

    // Write an entry for the CURRENT (pc, history) context. Used by the
    // allocation policy later; for now it lets lookup be tested before any
    // allocation logic exists.
    void install(std::size_t table, std::uint32_t pc, int ctr, std::uint8_t u) {
        TaggedEntry& e = tables_[table][ix_.index(table, pc)];
        e.tag = std::uint16_t(ix_.tag(table, pc));
        e.ctr = std::int8_t(ctr);
        e.u   = u;
    }

private:
    TageIndexer  ix_;
    TaggedEntry  tables_[NUM_TAGGED][TAGGED_ENTRIES];
    std::uint8_t bimodal_[BIMODAL_ENTRIES];
};

}  // namespace tage