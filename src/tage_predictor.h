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

    // Counter updates only. NO ALLOCATION YET -- entries are never created, so
    // no tagged table meaningfully matches and this is currently a bimodal
    // predictor with extra machinery attached. Deliberate: it measures the
    // floor, and it is a row in the ablation table.
    void update(const Lookup& L, bool taken) {
        int b = bimodal_[L.bim_idx] + (taken ? 1 : -1);
        if (b < 0)       b = 0;
        if (b > BIM_MAX) b = BIM_MAX;
        bimodal_[L.bim_idx] = std::uint8_t(b);

        const bool correct = (L.provider_pred == taken);

        if (L.provider >= 0) {
            TaggedEntry& e = tables_[L.provider][L.idx[L.provider]];
            int c = e.ctr + (taken ? 1 : -1);
            if (c < CTR_MIN) c = CTR_MIN;
            if (c > CTR_MAX) c = CTR_MAX;
            e.ctr = std::int8_t(c);

            // u moves ONLY when provider and altpred disagreed. Agreement
            // proves nothing: the shorter entry would have said the same.
            if (L.provider_pred != L.alt_pred) {
                if (correct) { if (e.u < U_MAX) e.u++; }
                else         { if (e.u > 0)     e.u--; }
            }
        }

        if (!correct) allocate(L, taken);

        if (++branches_ % U_AGING_PERIOD == 0) age_u();
    }

private:
    // Allocation on misprediction: the branch is being told "you need to see
    // further back."
    void allocate(const Lookup& L, bool taken) {
        const int start = L.provider + 1;          // provider == -1 -> table 0
        if (start >= int(NUM_TAGGED)) return;      // already the longest table

        int cand[NUM_TAGGED], n = 0;
        for (int t = start; t < int(NUM_TAGGED); ++t)
            if (tables_[t][L.idx[t]].u == 0) cand[n++] = t;

        if (n == 0) {
            // No eviction. A u>0 entry has PROVEN it beats its altpred; the
            // branch demanding a slot has merely missed once. Decrementing
            // makes sustained pressure lower the bar over time, while a
            // transient spike disturbs nothing.
            for (int t = start; t < int(NUM_TAGGED); ++t) {
                TaggedEntry& e = tables_[t][L.idx[t]];
                if (e.u > 0) e.u--;
            }
            return;
        }

        // Bias toward the NEAREST longer table: 1/2 the first candidate, 1/4
        // the second, and so on. The history requirement grows gradually
        // instead of jumping straight to the deepest table on one miss.
        int k = 0;
        while (k + 1 < n && !rand_bit()) ++k;
        const int pick = cand[k];

        TaggedEntry& e = tables_[pick][L.idx[pick]];
        e.tag = std::uint16_t(L.tg[pick]);
        e.ctr = taken ? 0 : -1;    // weakest counter on the correct side
        e.u   = 0;
    }

    void age_u() {
        for (std::size_t t = 0; t < NUM_TAGGED; ++t)
            for (std::size_t i = 0; i < TAGGED_ENTRIES; ++i)
                tables_[t][i].u >>= 1;
    }

    // Deterministic xorshift: allocation is randomised, but runs must be
    // reproducible or the results are not citable.
    bool rand_bit() {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        return rng_ & 1u;
    }

public:

    // Advance all folded histories. Conditional branches only.
    void push_history(bool taken) { ix_.push(taken); }

	// Test and diagnostic accessors.
    const TaggedEntry& entry(std::size_t t, std::uint32_t idx) const {
        return tables_[t][idx];
    }
    void set_u(std::size_t t, std::uint32_t idx, std::uint8_t v) {
        tables_[t][idx].u = v;
    }
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
    std::uint8_t  bimodal_[BIMODAL_ENTRIES];
    std::uint64_t branches_ = 0;
    std::uint32_t rng_ = 0x2545F491u;
};

}  // namespace tage