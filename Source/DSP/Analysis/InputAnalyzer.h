#pragma once

#include "DSP/Chain/ModContext.h"
#include "PitchTracker.h"

namespace aetherfall
{

/**
    Input Analyzer (spec section 5): level (RMS, peak), transient detector,
    pitch (YIN) and brightness (spectral-centroid estimate) of the plug-in
    input. Feeds ModContext::analysis and the analyzer modulation sources.
*/
class InputAnalyzer
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Analyses one chunk of the (stereo) input. */
    void process (const float* left, const float* right, int numSamples, bool trackPitch = true) noexcept;

    const AnalysisFrame& getFrame() const noexcept { return frame; }

    /** True if a transient started in the last processed chunk. */
    bool hadOnset() const noexcept { return onset; }

    // Normalised (0..1) versions used as modulation sources.
    float rmsNormalised() const noexcept;
    float pitchNormalised() const noexcept;
    float centroidNormalised() const noexcept;

private:
    double sr = 48000.0;
    PitchTracker pitch;

    float ms = 0.0f, msCoeff = 0.0f;
    float fast = 0.0f, slow = 0.0f;
    float fastAtk = 0.0f, fastRel = 0.0f, slowAtk = 0.0f, slowRel = 0.0f;
    float transientDecay = 0.0f;
    bool armed = true, onset = false;

    float prevSample = 0.0f, diffEnergy = 0.0f, sigEnergy = 0.0f, centroidCoeff = 0.0f;
    float lastPitchNorm = 0.0f;

    AnalysisFrame frame;
};

} // namespace aetherfall
