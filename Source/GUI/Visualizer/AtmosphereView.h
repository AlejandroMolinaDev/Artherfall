#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Utils/Meters.h"

namespace aetherfall
{

/**
    Central visualiser of the MAIN view: a drifting cloud of particles over a
    fog layer. The fog density follows the wet/output level, particles speed
    up and brighten with the level. Runs at 30 fps only while visible
    (software renderer; an OpenGL path can be added in F8/F10).
*/
class AtmosphereView final : public juce::Component, private juce::Timer
{
public:
    explicit AtmosphereView (const Meters&);
    ~AtmosphereView() override;

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;
    void setAccent (juce::Colour c) { accent = c; }

private:
    void timerCallback() override;

    struct Particle { float x, y, vx, vy, size, life; };
    const Meters& meters;
    std::array<Particle, 90> particles {};
    juce::Random rng { 1234 };
    float level = 0.0f, phase = 0.0f;
    juce::Colour accent;
};

} // namespace aetherfall
