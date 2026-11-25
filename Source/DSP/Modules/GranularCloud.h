#pragma once

#include <array>
#include <vector>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Random.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Granular Cloud (module 1).

    The input (mono sum) is written into a circular buffer (~8 s). A scheduler
    spawns up to 64 overlapping grains; each grain reads a short windowed
    slice of the buffer at its own position, pitch, direction and pan:

      density   grains per second (randomly jittered),
      size      grain length,
      position  how far back in the buffer grains start (0 = just now),
      spray     random extra offset per grain,
      pitch     transposition (+ random per grain: free / octaves / oct+5th),
      reverse   probability of a grain playing backwards,
      window    Hann / Tukey / Triangle envelope,
      feedback  the cloud is written back into the buffer (self-thickening),
      freeze    stops writing: the buffer becomes a static sample to scan.

    The output level is normalised by the expected grain overlap.
*/
class GranularCloud final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Granular; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return 0.6; }

    static constexpr int maxGrains = 64;
    static constexpr double bufferSeconds = 8.0;

    int getActiveGrainCount() const noexcept;

protected:
    void prepareModule() override;

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0;       // read position (absolute buffer index)
        double inc = 1.0;       // read increment (negative = reverse)
        int length = 0;
        int age = 0;
        float gainL = 0.0f, gainR = 0.0f;
    };

    void spawnGrain (const ModContext& ctx);
    float window (int type, float phase) const noexcept;

    std::vector<float> buffer;
    int mask = 0, writePos = 0;

    static constexpr int windowSize = 1024;
    std::array<std::vector<float>, 3> windows;   // Hann, Tukey, Triangle

    std::array<Grain, maxGrains> grains {};
    double samplesToNext = 0.0;
    dsp::Random32 rng { 0x6A11u };

    dsp::LinearSmoother<float> norm, feedback;
    int windowType = 0;
    bool primed = false;
};

} // namespace aetherfall
