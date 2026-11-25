#pragma once

#include <array>
#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Random.h"

namespace aetherfall
{

/**
    Spectral Freeze / Smear (module 2).

    Short-time Fourier transform (2048 or 4096 points, Hann windows, 75 %
    overlap). Per frame and per bin:
      smear    magnitudes are averaged over time (sounds blur into pads),
      freeze   holds the captured spectrum; phases keep advancing (+ random)
               so the frozen sound stays alive instead of buzzing,
      blur     magnitudes are smoothed across neighbouring bins,
      shimmer  the previous output spectrum, shifted by "Shift" semitones, is
               fed back -> spectral cascades upwards (or downwards),
      phase    randomises phases (diffuse, wash-like), tilt darkens/brightens.

    Latency: always latencySamples (4096) for both FFT sizes, so the plug-in
    latency does not change when the FFT size is switched. The chain delays the
    slot's dry signal by the same amount.
*/
class SpectralFreeze final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Spectral; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    int getLatencySamples() const override { return latencySamples; }
    double getTailSeconds() const override { return 2.0; }

    static constexpr int maxOrder = 12;                  // 4096
    static constexpr int maxSize = 1 << maxOrder;
    static constexpr int latencySamples = maxSize;

protected:
    void prepareModule() override;

private:
    struct Channel
    {
        std::vector<float> inRing;      // maxSize
        std::vector<float> outRing;     // 2 * maxSize (power of two)
        std::vector<float> smearMag, frozenMag, prevOut, phaseAcc;
    };

    void processFrame (Channel& c, const ModContext& ctx);

    std::unique_ptr<juce::dsp::FFT> fft11, fft12;
    std::array<std::vector<float>, 2> windows;   // Hann for 2048 / 4096
    std::vector<float> fftData, mag, phase, work;
    std::array<Channel, 2> ch;

    int size = maxSize, order = maxOrder, hop = maxSize / 4;
    int inWrite = 0, hopCounter = 0;
    long long sampleCounter = 0;
    int outMask = 0;
    bool wasFrozen = false;
    dsp::Random32 rng { 0x5FECu };
};

} // namespace aetherfall
