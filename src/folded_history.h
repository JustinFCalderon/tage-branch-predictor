#pragma once
// Folded (XOR-compressed) global history.
//
// A table indexed with L bits of history but only W index bits needs those L
// bits compressed to W. Definition: history bit i contributes to fold bit
// (i mod W). Recomputing that over L bits every branch is far too slow, so the
// value is maintained incrementally.
//
// Derivation of the update. Pushing outcome b shifts every old bit up one
// position, which within W bits is a rotate-left-by-1. But the rotate acts on
// all L old bits, while the new fold only covers L-1 of them: the bit that
// just left the window is still in there, now sitting at position (L mod W).
// XOR is self-inverse, so cancel it. Hence:
//
//     F(H') = rotl(F(H)) XOR b@0 XOR outgoing@(L mod W)
//
// Constant time regardless of L. This is why a 135-bit history costs the same
// per branch as a 5-bit one, and therefore why every table can afford a fold.

#include <cstddef>
#include <cstdint>

class FoldedHistory {
public:
    void init(std::size_t history_len, std::size_t width) {
        len_     = history_len;
        width_   = width;
        mask_    = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
        out_pos_ = history_len % width;   // NOT (len-1) % width: the rotate
                                          // already moved the outgoing bit up.
        value_   = 0;
    }

    std::uint32_t value() const { return value_; }

    // incoming     = the outcome being shifted in
    // outgoing_bit = the history bit leaving the window (raw history bit L-1,
    //                sampled BEFORE the push)
    void update(bool incoming, bool outgoing_bit) {
        value_ = ((value_ << 1) | (value_ >> (width_ - 1))) & mask_;  // rotl
        if (incoming)     value_ ^= 1u;
        if (outgoing_bit) value_ ^= (1u << out_pos_);
        value_ &= mask_;
    }

private:
    std::size_t   len_ = 0, width_ = 1, out_pos_ = 0;
    std::uint32_t mask_ = 0, value_ = 0;
};
