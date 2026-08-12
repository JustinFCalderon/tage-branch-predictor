// Prints the storage breakdown and confirms the configuration fits the
// self-imposed budget. The static_asserts in tage_config.h already make an
// over-budget build fail; this exists so the numbers are readable by a human
// and quotable in the report without being retyped by hand.

#include <cstdio>
#include "../src/tage_config.h"

using namespace tage;

int main() {
    std::printf("%-8s %8s %6s %4s %3s %4s %10s\n",
                "table", "entries", "hist", "ctr", "u", "tag", "bits");
    std::printf("%-8s %8zu %6d %4zu %3s %4s %10zu\n",
                "T0", BIMODAL_ENTRIES, 0, BIMODAL_CTR_BITS, "-", "-", BIMODAL_BITS);
    for (std::size_t i = 0; i < NUM_TAGGED; ++i) {
        char name[8];
        std::snprintf(name, sizeof(name), "T%zu", i + 1);
        std::printf("%-8s %8zu %6zu %4zu %3zu %4zu %10zu\n",
                    name, TAGGED_ENTRIES, HIST_LEN[i], CTR_BITS, U_BITS, TAG_BITS,
                    TAGGED_ENTRIES * TAGGED_ENTRY_BITS);
    }
    std::printf("\n%-8s %54zu bits  (%.2f KiB)\n", "TOTAL", TOTAL_BITS, TOTAL_BITS / 8192.0);
    std::printf("%-8s %54zu bits  (%.2f KiB)\n", "budget", BUDGET_BITS, BUDGET_BITS / 8192.0);
    std::printf("%-8s %53.1f%%\n", "used", 100.0 * TOTAL_BITS / BUDGET_BITS);
    std::printf("\nbaseline gshare logical state: %zu bits (%.2f KiB)\n",
                std::size_t(32768 * 2), 32768 * 2 / 8192.0);
    return TOTAL_BITS <= BUDGET_BITS ? 0 : 1;
}
