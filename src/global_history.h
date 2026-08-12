#pragma once
// Raw global branch history: a shift register of conditional-branch outcomes.
// bit(0) is the most recent outcome, bit(1) the one before it, and so on.
//
// The folded registers (folded_history.h) are what the predictor actually
// indexes with; this raw buffer exists so a fold can know which bit is about
// to fall out of its window, and so tests can recompute a fold from scratch.

#include <array>
#include <cstddef>
#include <cstdint>

template <std::size_t MAX_HISTORY>
class GlobalHistory {
public:
    // Shift in one outcome. Only conditional branches should be pushed.
    void push(bool taken) {
        for (std::size_t w = WORDS; w-- > 1;)
            words_[w] = (words_[w] << 1) | (words_[w - 1] >> 63);
        words_[0] = (words_[0] << 1) | (taken ? 1ull : 0ull);
    }

    // bit(0) == most recent outcome.
    bool bit(std::size_t i) const {
        return (words_[i / 64] >> (i % 64)) & 1ull;
    }

    static constexpr std::size_t capacity() { return MAX_HISTORY; }

private:
    static constexpr std::size_t WORDS = (MAX_HISTORY + 64) / 64;
    std::array<std::uint64_t, WORDS> words_{};
};
