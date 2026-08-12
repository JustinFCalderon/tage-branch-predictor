// Folded history: incremental update must equal a from-scratch recomputation
// at every step, for every (history length, fold width) pair we ship.
//
// The naive fold is the DEFINITION. The incremental fold is the OPTIMIZATION.
// Any divergence is a bug in the optimization, and this test names the branch
// number where it first appears.

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include "../src/global_history.h"
#include "../src/folded_history.h"

static constexpr std::size_t MAX_HISTORY = 256;
using GH = GlobalHistory<MAX_HISTORY>;

// history bit i -> fold bit (i mod width). This is the specification.
static std::uint32_t naive_fold(const GH& h, std::size_t len, std::size_t width) {
    std::uint32_t acc = 0;
    for (std::size_t i = 0; i < len; ++i)
        if (h.bit(i)) acc ^= (1u << (i % width));
    return acc & ((1u << width) - 1);
}

// Deterministic pseudo-random outcome stream (xorshift), so failures reproduce.
static std::uint32_t rng_state = 0x1234567u;
static bool next_outcome() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state & 1u;
}

struct Case { std::size_t len, width; const char* name; };

int main() {
    const Case cases[] = {
        {  5, 10, "T1 index  (len 5,   width 10)" },
        { 15, 10, "T2 index  (len 15,  width 10)" },
        { 45, 10, "T3 index  (len 45,  width 10)" },
        {135, 10, "T4 index  (len 135, width 10)" },
        {135,  9, "T4 tag-b  (len 135, width 9)"  },
        { 45,  9, "T3 tag-b  (len 45,  width 9)"  },
    };
    const int N = sizeof(cases) / sizeof(cases[0]);
    const int STEPS = 20000;

    int failures = 0;
    for (int c = 0; c < N; ++c) {
        rng_state = 0x1234567u;
        GH h;
        FoldedHistory f;
        f.init(cases[c].len, cases[c].width);

        for (int step = 0; step < STEPS; ++step) {
            // The bit about to leave the window sits at index len-1 BEFORE the push.
            bool outgoing = h.bit(cases[c].len - 1);
            bool taken = next_outcome();

            h.push(taken);
            f.update(taken, outgoing);

            std::uint32_t want = naive_fold(h, cases[c].len, cases[c].width);
            if (f.value() != want) {
                std::printf("FAIL  %-30s step %d: incremental=0x%03x naive=0x%03x\n",
                            cases[c].name, step, f.value(), want);
                failures++;
                break;
            }
        }
    }

    if (failures) {
        std::printf("\n%d/%d folded-history cases FAILED\n", failures, N);
        return 1;
    }
    std::printf("all %d folded-history cases passed (%d steps each)\n", N, STEPS);
    return 0;
}
