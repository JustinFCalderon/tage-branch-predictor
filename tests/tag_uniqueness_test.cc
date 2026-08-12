// A TAGE tag answers one question: "was this entry allocated by the (PC,
// history) context I am in right now?" Three properties follow, and a tag that
// fails any of them cannot answer it.
//
//   A  same PC, same history, different tables -> tags must differ
//   B  same PC, different history              -> tag must change
//   C  different PC, same history              -> tags must differ
//
// A and B are impossible for a PC-only tag, by construction. C only tests
// width. Rates are measured, not asserted pointwise, because a hash is allowed
// its birthday collisions: with TAG_BITS=10 the floor is ~1/1024.

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "../src/tage_indexer.h"

using namespace tage;

static std::uint32_t rng_state = 0xC0FFEEu;
static std::uint32_t rnd() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
// Plausible 4-byte-aligned branch addresses.
static std::uint32_t rnd_pc() { return (rnd() & 0x00FFFFFCu) | 0x00400000u; }

static constexpr int    SAMPLES   = 200000;
static constexpr double IDEAL     = 1.0 / (1u << TAG_BITS);   // ~0.000977
// 3x, not 10x. A 10x gate passed the two-fold tag construction, whose
// cross-context collision rate was 2x the floor -- see the parity note in
// tage_config.h. Anything above 3x now fails rather than merely looking odd.
static constexpr double THRESHOLD = 3.0 * IDEAL;              // ~0.0029

int main() {
    int failures = 0;
	const int CHECKS = 4;

    // Warm the history so the folds are in a realistic state.
    TageIndexer ix;
    for (int i = 0; i < 5000; ++i) ix.push(rnd() & 1u);

    // ---- A: same PC, same history, across tables ---------------------------
    {
        int all_same = 0;
        for (int s = 0; s < SAMPLES; ++s) {
            const std::uint32_t pc = rnd_pc();
            const std::uint32_t t0 = ix.tag(0, pc);
            bool same = true;
            for (std::size_t t = 1; t < NUM_TAGGED; ++t)
                if (ix.tag(t, pc) != t0) { same = false; break; }
            if (same) all_same++;
        }
        const double rate = double(all_same) / SAMPLES;
        const bool ok = rate < THRESHOLD;
        std::printf("A  cross-table   all %zu tags identical: %8.5f  (want <%.5f)  %s\n",
                    NUM_TAGGED, rate, THRESHOLD, ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // ---- B: same PC, different history -------------------------------------
    // Uses the longest table only: with 135 bits of history, two independently
    // driven streams effectively never share a context, so any tag match is a
    // genuine hash collision rather than an identical input.
    {
        const std::size_t T = NUM_TAGGED - 1;
        TageIndexer a, b;
        for (int i = 0; i < 5000; ++i) a.push(rnd() & 1u);
        for (int i = 0; i < 5000; ++i) b.push(rnd() & 1u);

        // Advance BOTH histories every sample. Without this, all SAMPLES
        // iterations evaluate one identical comparison and the rate can only be
        // 0.0 or 1.0 -- 200000 repetitions of a single Bernoulli trial.
        //
        // Note the tag is XOR-linear in the PC, so `p` cancels when comparing
        // two contexts: this check measures fold divergence, which is exactly
        // the property a PC-only tag cannot have. The PC stays in the call so
        // the test exercises the real signature.
        int unchanged = 0;
        for (int s = 0; s < SAMPLES; ++s) {
            a.push(rnd() & 1u);
            b.push(rnd() & 1u);
            const std::uint32_t pc = rnd_pc();
            if (a.tag(T, pc) == b.tag(T, pc)) unchanged++;
        }
        const double rate = double(unchanged) / SAMPLES;
        const bool ok = rate < THRESHOLD;
        std::printf("B  cross-context tag unchanged (T%zu):    %8.5f  (want <%.5f)  %s\n",
                    T + 1, rate, THRESHOLD, ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // ---- C: different PC, same history -------------------------------------
    {
        int collisions = 0, trials = 0;
        for (int s = 0; s < SAMPLES; ++s) {
            const std::uint32_t pa = rnd_pc(), pb = rnd_pc();
            if (pa == pb) continue;
            trials++;
            if (ix.tag(NUM_TAGGED - 1, pa) == ix.tag(NUM_TAGGED - 1, pb)) collisions++;
        }
        const double rate = double(collisions) / trials;
        const bool ok = rate < THRESHOLD;
        std::printf("C  cross-PC      tag collision:          %8.5f  (want <%.5f)  %s\n",
                    rate, THRESHOLD, ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

	// ---- D: among PCs that COLLIDE in the index, tags must still differ -----
    // The conditional version of C, and the one that matters. If index and tag
    // draw on the same PC bits, an index collision implies a tag collision and
    // the tag contributes nothing to cross-PC discrimination -- while C still
    // reports a healthy rate, because most random PC pairs never collide.
    {
        const std::size_t T = NUM_TAGGED - 1;
        const std::size_t N = std::size_t(1) << TAGGED_IDX_BITS;
        std::vector<std::uint32_t> first(N, 0);
        std::vector<char>          seen(N, 0);

        int pairs = 0, collide = 0;
        for (int s = 0; s < SAMPLES * 4 && pairs < 20000; ++s) {
            const std::uint32_t pc = rnd_pc();
            const std::uint32_t i  = ix.index(T, pc);
            if (!seen[i]) { seen[i] = 1; first[i] = pc; continue; }
            if (first[i] == pc) continue;
            pairs++;
            if (ix.tag(T, first[i]) == ix.tag(T, pc)) collide++;
        }
        const double rate = pairs ? double(collide) / pairs : 1.0;
        const bool ok = rate < THRESHOLD;
        std::printf("D  index-collide tag also collides:      %8.5f  (want <%.5f)  %s\n",
                    rate, THRESHOLD, ok ? "PASS" : "FAIL");
        std::printf("       (%d index-colliding PC pairs sampled)\n", pairs);
        if (!ok) failures++;
    }
	
    std::printf("\nideal collision rate for a %zu-bit tag: %.5f\n", TAG_BITS, IDEAL);
    if (failures) {
        std::printf("%d/%d tag properties FAILED\n", failures, CHECKS);
        return 1;
    }
    std::printf("all %d tag properties passed\n", CHECKS);
    return 0;
}
