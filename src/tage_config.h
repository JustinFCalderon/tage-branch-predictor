#pragma once
// TAGE geometry, entry layout, and storage accounting.
//
// Every structural constant lives here so that (a) configurations are swept by
// editing one file, and (b) the storage cost is computed from the same numbers
// the tables are allocated from. The assignment sets no storage limit but
// requires the area overhead to be explained; BUDGET_BITS is our own choice,
// enforced at compile time.

#include <cstddef>
#include <cstdint>

namespace tage {

// ---------------------------------------------------------------- geometry --
inline constexpr std::size_t NUM_TAGGED = 4;

// T0: tagless bimodal. Always matches, so there is always a prediction.
inline constexpr std::size_t BIMODAL_IDX_BITS = 13;             // 8192 entries
inline constexpr std::size_t BIMODAL_CTR_BITS = 2;

// T1..Tn: tagged tables.
inline constexpr std::size_t TAGGED_IDX_BITS = 10;              // 1024 entries
inline constexpr std::size_t TAG_BITS        = 10;
inline constexpr std::size_t CTR_BITS        = 3;
inline constexpr std::size_t U_BITS          = 2;

// Geometric history lengths: L(i) = round(alpha^(i-1) * L(1)), L(1)=5, alpha=3.
// The individual values are not load-bearing -- allocation migrates each branch
// to whichever table suits it, so the series only has to COVER the distribution
// of useful correlation distances, densely at the short end.
inline constexpr std::size_t HIST_LEN[NUM_TAGGED] = { 5, 15, 45, 135 };
inline constexpr std::size_t MAX_HIST = 135;

// THREE folds, not two -- and the count being odd is load-bearing.
//
// For any width w, every history bit lands in exactly one position of fold_w,
// so parity(fold_w(H)) == parity(H) for every w. XOR an EVEN number of folds
// and the parities cancel: the result is confined to the even-parity subspace,
// half the tag values become unreachable, and cross-context collisions double
// (measured 1/512 instead of 1/1024). An odd number leaves parity(H) in the
// result, restoring the full space. Shifts are 0/1/2, all within TAG_BITS, so
// no fold is truncated.
inline constexpr std::size_t TAG_FOLD_A_BITS = TAG_BITS;        // 10, shift 0
inline constexpr std::size_t TAG_FOLD_B_BITS = TAG_BITS - 1;    //  9, shift 1
inline constexpr std::size_t TAG_FOLD_C_BITS = TAG_BITS - 2;    //  8, shift 2

// ------------------------------------------------------------ derived sizes --
inline constexpr std::size_t BIMODAL_ENTRIES = std::size_t(1) << BIMODAL_IDX_BITS;
inline constexpr std::size_t TAGGED_ENTRIES  = std::size_t(1) << TAGGED_IDX_BITS;
inline constexpr std::size_t TAGGED_ENTRY_BITS = CTR_BITS + U_BITS + TAG_BITS;

inline constexpr std::size_t BIMODAL_BITS = BIMODAL_ENTRIES * BIMODAL_CTR_BITS;
inline constexpr std::size_t TAGGED_BITS  = NUM_TAGGED * TAGGED_ENTRIES * TAGGED_ENTRY_BITS;
inline constexpr std::size_t TOTAL_BITS   = BIMODAL_BITS + TAGGED_BITS;

// Usefulness counters are halved every this many conditional branches, so an
// entry that proved useful long ago cannot hold its slot forever.
inline constexpr std::size_t U_AGING_PERIOD = 512 * 1024;

// ------------------------------------------------------------------ budget --
// Self-imposed, not required by the assignment. Reasoning: the stock gshare
// baseline holds 32768 two-bit counters = 8 KiB of logical state (the sample
// wastes six bits per entry by storing each counter in a byte; hardware would
// not). Staying within 2x that keeps the comparison honest -- beating the
// baseline by outspending it proves nothing -- while leaving room for the loop
// predictor and statistical corrector to be added later without a redesign.
inline constexpr std::size_t BUDGET_BITS = 16 * 1024 * 8;       // 16 KiB

static_assert(TOTAL_BITS <= BUDGET_BITS,
              "TAGE configuration exceeds the declared storage budget");
static_assert(MAX_HIST >= HIST_LEN[NUM_TAGGED - 1],
              "MAX_HIST must cover the longest table history");
static_assert(TAG_FOLD_C_BITS >= 1, "tag fold C width must be positive");
static_assert(TAG_FOLD_C_BITS + 2 <= TAG_BITS, "fold C would be truncated");

}  // namespace tage
