// Allocation and usefulness-counter policy.
//
//   1  a misprediction with no provider allocates exactly one entry
//   2  allocation goes into a table with LONGER history than the provider
//   3  when no candidate has u == 0, nothing is evicted and every candidate's
//      u is decremented instead  ("no eviction, lower the bar")
//   4  u moves only when provider and altpred DISAGREED
//   5  ...and not when they agreed

#include <cstdio>
#include <cstdint>
#include "../src/tage_predictor.h"

using namespace tage;

static int failures = 0;
static void check(bool ok, const char* what) {
    std::printf("  %-56s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

static constexpr std::uint32_t PC = 0x00401234u;

// How many of the tagged slots this PC maps to were modified?
static int changed(const TagePredictor& p, const Lookup& L,
                   const TaggedEntry* before, int from) {
    int n = 0;
    for (int t = from; t < int(NUM_TAGGED); ++t) {
        const TaggedEntry& a = before[t];
        const TaggedEntry& b = p.entry(t, L.idx[t]);
        if (a.tag != b.tag || a.ctr != b.ctr || a.u != b.u) n++;
    }
    return n;
}

int main() {
    // 1 -- no provider, mispredict -> exactly one allocation
    {
        TagePredictor p;
        Lookup L = p.lookup(PC);
        TaggedEntry before[NUM_TAGGED];
        for (std::size_t t = 0; t < NUM_TAGGED; ++t) before[t] = p.entry(t, L.idx[t]);

        check(L.provider == -1, "setup: no provider");
        p.update(L, true);              // predicted not-taken, actually taken
        check(changed(p, L, before, 0) == 1, "mispredict with no provider allocates one entry");
    }

    // 2 -- provider is T2 (index 1); allocation must land in T3 or T4
    {
        TagePredictor p;
        p.install(1, PC, -2, 0);        // T2 predicts not-taken
        Lookup L = p.lookup(PC);
        check(L.provider == 1, "setup: provider is T2");

        TaggedEntry before[NUM_TAGGED];
        for (std::size_t t = 0; t < NUM_TAGGED; ++t) before[t] = p.entry(t, L.idx[t]);

        p.update(L, true);              // mispredict
        check(changed(p, L, before, 2) == 1, "allocates into a longer-history table");
        check(p.entry(0, L.idx[0]).tag == before[0].tag,
              "shorter tables untouched by allocation");
    }

    // 3 -- every longer candidate is u == U_MAX: no eviction, u decremented
    {
        TagePredictor p;
        p.install(1, PC, -2, 0);
        Lookup L = p.lookup(PC);
        for (int t = 2; t < int(NUM_TAGGED); ++t) p.set_u(t, L.idx[t], U_MAX);

        TaggedEntry before[NUM_TAGGED];
        for (std::size_t t = 0; t < NUM_TAGGED; ++t) before[t] = p.entry(t, L.idx[t]);

        p.update(L, true);              // mispredict

        bool no_tag_written = true, all_decremented = true;
        for (int t = 2; t < int(NUM_TAGGED); ++t) {
            if (p.entry(t, L.idx[t]).tag != before[t].tag) no_tag_written = false;
            if (p.entry(t, L.idx[t]).u   != U_MAX - 1)     all_decremented = false;
        }
        check(no_tag_written,  "no u==0 candidate: nothing is evicted");
        check(all_decremented, "no u==0 candidate: every candidate u decremented");
    }

    // 4 -- provider and altpred disagree, provider right -> u increments
    {
        TagePredictor p;
        p.install(0, PC, -2, 0);        // T1 says not-taken  (altpred)
        p.install(3, PC, +2, 0);        // T4 says taken      (provider)
        Lookup L = p.lookup(PC);
        check(L.provider == 3 && L.altpred == 0, "setup: provider T4, altpred T1");
        check(L.provider_pred != L.alt_pred,     "setup: they disagree");

        p.update(L, true);              // provider was right
        check(p.entry(3, L.idx[3]).u == 1, "disagreed and correct -> u increments");
    }

    // 5 -- they agree -> u unchanged, whatever the outcome
    {
        TagePredictor p;
        p.install(0, PC, +2, 0);
        p.install(3, PC, +2, 0);
        Lookup L = p.lookup(PC);
        check(L.provider_pred == L.alt_pred, "setup: provider and altpred agree");

        p.update(L, true);
        check(p.entry(3, L.idx[3]).u == 0, "agreed -> u unchanged (nothing was proven)");
    }

    if (failures) { std::printf("\n%d allocation checks FAILED\n", failures); return 1; }
    std::printf("\nall allocation checks passed\n");
    return 0;
}