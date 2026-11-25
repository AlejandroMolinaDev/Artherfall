#pragma once

#include <atomic>

namespace aetherfall::dsp
{

/**
    Lock-free "latest value" exchange between ONE writer thread and ONE reader
    thread (classic triple buffering). The writer never blocks the reader and
    vice versa; the reader always sees a complete, consistent T.

    Used to hand the modulation routing (edited on the message thread) to the
    audio thread without locks or allocations.
*/
template <typename T>
class TripleBuffer
{
public:
    /** Writer side: publishes a new value. */
    void write (const T& value) noexcept
    {
        buffers[back] = value;
        const int previous = state.exchange (back | dirtyBit, std::memory_order_acq_rel);
        back = previous & indexMask;
    }

    /** Reader side: returns the newest published value (stable until the next read()). */
    const T& read() noexcept
    {
        if ((state.load (std::memory_order_acquire) & dirtyBit) != 0)
        {
            const int previous = state.exchange (front, std::memory_order_acq_rel);
            front = previous & indexMask;
        }
        return buffers[front];
    }

    /** True if a value was written since the last read(). */
    bool hasNewData() const noexcept { return (state.load (std::memory_order_acquire) & dirtyBit) != 0; }

private:
    static constexpr int dirtyBit = 4, indexMask = 3;
    T buffers[3] {};
    std::atomic<int> state { 1 };   // index of the middle buffer (+ dirty flag)
    int front = 0, back = 2;
};

} // namespace aetherfall::dsp
