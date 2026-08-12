// Provider/altpred selection. Tests the "longest matching history wins" rule
// in isolation: no update policy, no allocation, entries placed by hand.

#include <cstdio>
#include <cstdint>
#include "../src/tage_predictor.h"

using namespace tage;

static int failures = 0;
static void check(bool ok, const char* what) {
    std::printf("  %-52s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

static constexpr std::uint32_t PC = 0x00401234u;

int main() {
    // 1 -- nothing installed: no tagged table provides, bimodal answers.
    {
        TagePredictor p;
        Lookup L = p.lookup(PC);
        check(L.provider == -1, "empty: no provider");
        check(L.altpred  == -1, "empty: no altpred");
        check(p.predict(L) == false, "empty: falls back to bimodal (not taken)");
    }

    // 2 -- one entry in T3 (index 2): it provides, nothing is altpred.
    {
        TagePredictor p;
        p.install(2, PC, +2, 0);
        Lookup L = p.lookup(PC);
        check(L.provider == 2,  "single entry in T3: provider is T3");
        check(L.altpred  == -1, "single entry in T3: altpred is bimodal");
        check(p.predict(L) == true, "single entry in T3: predicts its counter");
    }

    // 3 -- entries in T1 and T4: longest history wins, shorter is altpred.
    {
        TagePredictor p;
        p.install(0, PC, -2, 0);   // T1, predicts not-taken
        p.install(3, PC, +2, 0);   // T4, predicts taken
        Lookup L = p.lookup(PC);
        check(L.provider == 3, "T1+T4 installed: provider is T4 (longest)");
        check(L.altpred  == 0, "T1+T4 installed: altpred is T1");
        check(L.provider_pred == true,  "provider predicts taken");
        check(L.alt_pred      == false, "altpred predicts not-taken");
        check(p.predict(L) == true, "prediction follows provider, not altpred");
    }

    // 4 -- all four installed: provider T4, altpred T3, shorter two ignored.
    {
        TagePredictor p;
        for (std::size_t t = 0; t < NUM_TAGGED; ++t) p.install(t, PC, +1, 0);
        Lookup L = p.lookup(PC);
        //check(L.provider == 3, "all four: provider is T4");
        //check(L.altpred  == 2, "all four: altpred is T3");
		check(L.provider == int(NUM_TAGGED) - 1, "all installed: provider is the longest table");
        check(L.altpred  == int(NUM_TAGGED) - 2, "all installed: altpred is the next longest");
    }

    // 5 -- the entry belongs to a CONTEXT, not a PC. Move the history and the
    //      same PC must stop matching. This is the property the 2025 tag
    //      function could not have.
    {
        TagePredictor p;
        p.install(3, PC, +2, 0);
        Lookup before = p.lookup(PC);
        check(before.hit[3] == true, "context: matches in the context it was written");

        int still_hit = 0;
        for (int trial = 0; trial < 64; ++trial) {
            TagePredictor q;
            q.install(3, PC, +2, 0);
            for (int i = 0; i < 200; ++i) q.push_history((trial * 7 + i * 13) & 1);
            if (q.lookup(PC).hit[3]) still_hit++;
        }
        check(still_hit <= 2, "context: rarely matches after history moves on");
        std::printf("       (matched in %d of 64 shifted contexts)\n", still_hit);
    }

    // 6 -- a different PC in the same context does not inherit the entry.
    {
        TagePredictor p;
        p.install(3, PC, +2, 0);
        Lookup L = p.lookup(PC + 0x1000);
        check(L.hit[3] == false, "different PC does not match the entry");
    }

    if (failures) { std::printf("\n%d lookup checks FAILED\n", failures); return 1; }
    std::printf("\nall lookup checks passed\n");
    return 0;
}