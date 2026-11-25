#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Random.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Texture Layer (module 17).
    Adds a procedural noise bed — no samples, everything is synthesised:
      Vinyl     crackle (random filtered clicks) + surface noise + rumble,
      Tape Hiss pink noise tilted bright, gently fluttering,
      Wind      noise through a drifting resonant band-pass, with gusts,
      Rain      many tiny decaying droplets over a soft pink bed,
      Hum       50 Hz mains hum with harmonics and a little buzz.
    "Follow" makes the layer track the input level (envelope follower);
    "Width" decorrelates left/right; "Tone" darkens or brightens.
    Output = input + texture.
*/
class TextureLayer final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Texture; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    /** Paul Kellet's economy pink-noise filter. */
    struct Pink
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float process (float white) noexcept
        {
            b0 = 0.99765f * b0 + white * 0.0990460f;
            b1 = 0.96300f * b1 + white * 0.2965164f;
            b2 = 0.57000f * b2 + white * 1.0526913f;
            return (b0 + b1 + b2 + white * 0.1848f) * 0.25f;
        }
    };

    struct Voice   // one decorrelated generator per channel + one shared
    {
        Pink pink;
        dsp::SVF<float> band;          // wind
        dsp::OnePole<float> clickLP;   // crackle colour
        float click = 0.0f;            // current crackle / droplet envelope
        float clickSign = 1.0f;
        float drop = 0.0f;
    };

    float generate (Voice& v, int type, float density) noexcept;

    std::array<Voice, 3> voices;       // L, R, common
    std::array<dsp::OnePole<float>, 2> toneLP;
    std::array<dsp::OnePole<float>, 2> toneHP;
    dsp::Random32 rng { 0x7E57u };
    double humPhase = 0.0;
    double windDrift = 0.0, windTarget = 0.0, gust = 0.0, gustTarget = 0.0;
    int driftCounter = 0;
    float env = 0.0f, envAtk = 0.0f, envRel = 0.0f;
    dsp::LinearSmoother<float> level, follow, width;
    float lastTone = -1.0f;
    int lastType = -1;
    bool primed = false;
};

} // namespace aetherfall
