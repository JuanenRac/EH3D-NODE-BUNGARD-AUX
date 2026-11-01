// EH3D-NODE-BUNGARD-AUX - core/pulse_ring.hpp: the ring where the interrupt of the speed sensor leaves the time of each pulse until the task that asks for the speed takes them.
// GPL-3.0-or-later.
//
// The indices are unsigned and only ever grow: a ring of a power of two turns them into a place with a mask, and the difference of two of them is right even when they wrap
// around after 2^32 pulses (a signed index went negative after about twenty-five days of a spindle at 1 kHz and wrote outside the array). The ring does no locking:
// the one who pushes and the one who reads hold the lock of the firmware around the calls that touch the head, as before.
#pragma once

#include <cstddef>
#include <cstdint>

namespace bungard_aux {

template <std::size_t N>
class PulseRing {
    static_assert(N > 1 && (N & (N - 1)) == 0, "the size of the ring is a power of two");

public:
    static constexpr std::size_t kSize = N;

    /// `first` is the count the ring starts from (0 in the firmware; the tests start it near the wrap of the index).
    explicit PulseRing(std::uint32_t first = 0) : head_(first) {}

    /// Leaves the time of a pulse (the oldest is lost when the reader is more than a ring behind).
    // always inline: the interrupt that calls it runs from the instruction RAM and must not jump into the flash
    __attribute__((always_inline)) inline void push(std::uint64_t t_us) {
        slot_[head_ & (N - 1)] = t_us;
        head_ = head_ + 1;
    }

    /// How many pulses were ever pushed (wraps around at 2^32).
    std::uint32_t head() const { return head_; }

    /// Where a reader that stopped at `tail` goes on from, when the head is at `head`: `tail` itself, or the oldest pulse still in the ring when more than a ring came.
    static std::uint32_t resume_from(std::uint32_t tail, std::uint32_t head) {
        return static_cast<std::uint32_t>(head - tail) > N ? static_cast<std::uint32_t>(head - N) : tail;
    }

    /// The pulse at the place `index` of the count.
    std::uint64_t at(std::uint32_t index) const { return slot_[index & (N - 1)]; }

private:
    volatile std::uint64_t slot_[N] = {};
    volatile std::uint32_t head_;
};

}  // namespace bungard_aux
