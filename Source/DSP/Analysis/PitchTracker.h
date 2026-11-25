#pragma once

#include <vector>
#include "DSP/Utils/Filters.h"

namespace aetherfall
{

/**
    Monophonic pitch detector (YIN, de Cheveigne & Kawahara 2002).

    The input is low-passed and decimated to ~12 kHz, collected in a sliding
    window, and every hop the YIN "cumulative mean normalised difference" is
    searched for its first dip below a threshold. Parabolic interpolation
    refines the period. Range ~45 Hz .. 1.5 kHz (voices, basses, pads).

    Cost is bounded (one analysis per hop) and the class never allocates after
    prepare(), so it can run on the audio thread.
*/
class PitchTracker
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Feed one input sample (host rate). */
    void push (float x) noexcept;

    float getFrequency() const noexcept  { return frequency; }   // 0 = unvoiced
    float getConfidence() const noexcept { return confidence; }  // 0..1

private:
    void analyse() noexcept;

    double inputRate = 48000.0, rate = 12000.0;
    int decimation = 4, decimCounter = 0;
    dsp::Biquad<float> aa1, aa2;   // anti-alias before decimation

    static constexpr int windowSize = 1024;
    static constexpr int hopSize = 256;
    std::vector<float> ring;       // last 2 * windowSize decimated samples
    std::vector<float> frame, diff;
    int writeIndex = 0, hopCounter = 0;
    int tauMin = 8, tauMax = 300;

    float frequency = 0.0f, confidence = 0.0f;
};

} // namespace aetherfall
