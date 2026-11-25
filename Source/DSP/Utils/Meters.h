#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace aetherfall
{

/**
    Lock-free meter values written by the audio thread once per chunk and read
    by the GUI (phase F7/F8). Peaks are the max of the chunk (the GUI applies
    hold/decay); RMS values are smoothed (~300 ms). Gain reductions in dB (<= 0).
*/
struct Meters
{
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f }, inRmsL { 0.0f }, inRmsR { 0.0f };
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f }, outRmsL { 0.0f }, outRmsR { 0.0f };
    std::atomic<float> correlation { 1.0f };   // -1 .. +1 (output)
    std::atomic<float> glueGainReductionDb { 0.0f };
    std::atomic<float> duckGainReductionDb { 0.0f };
    std::atomic<float> limiterGainReductionDb { 0.0f };
};

/** Helper accumulating level statistics of a stereo signal. */
class LevelMeter
{
public:
    void prepare (double sampleRate) noexcept
    {
        coeff = (float) std::exp (-1.0 / (0.3 * sampleRate));
        reset();
    }

    void reset() noexcept { msL = msR = msLR = 0.0f; }

    void process (const float* l, const float* r, int n, float& peakL, float& peakR) noexcept
    {
        peakL = peakR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            peakL = std::max (peakL, std::abs (l[i]));
            peakR = std::max (peakR, std::abs (r[i]));
            msL  = l[i] * l[i] + coeff * (msL - l[i] * l[i]);
            msR  = r[i] * r[i] + coeff * (msR - r[i] * r[i]);
            msLR = l[i] * r[i] + coeff * (msLR - l[i] * r[i]);
        }
    }

    float rmsL() const noexcept { return std::sqrt (msL); }
    float rmsR() const noexcept { return std::sqrt (msR); }

    /** Pearson correlation of L and R (1 = mono, 0 = wide, -1 = out of phase). */
    float correlation() const noexcept
    {
        const float den = std::sqrt (msL * msR);
        return den > 1.0e-9f ? std::clamp (msLR / den, -1.0f, 1.0f) : 1.0f;
    }

private:
    float coeff = 0.0f, msL = 0.0f, msR = 0.0f, msLR = 0.0f;
};

} // namespace aetherfall
