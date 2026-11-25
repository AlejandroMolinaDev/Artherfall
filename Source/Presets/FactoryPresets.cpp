#include "FactoryPresets.h"

namespace aetherfall
{

namespace
{
    constexpr const char* categoryNames[] = { "Outro Skies", "Intro Haze", "Vocal Atmos", "Synth Walls", "Granular Dreams",
                                              "Dark & Rude", "Frozen", "Rhythmic Space", "Subtle", "User" };
}

const char* categoryName (PresetCategory c) noexcept
{
    const int i = (int) c;
    return i >= 0 && i < (int) PresetCategory::Count ? categoryNames[i] : "User";
}

PresetCategory categoryFromName (const juce::String& name) noexcept
{
    for (int i = 0; i < (int) PresetCategory::Count; ++i)
        if (name.equalsIgnoreCase (categoryNames[i]))
            return (PresetCategory) i;
    return PresetCategory::User;
}

juce::Colour categoryAccent (PresetCategory c) noexcept
{
    switch (c)
    {
        case PresetCategory::OutroSkies:     return juce::Colour (0xff8c7cff);
        case PresetCategory::IntroHaze:      return juce::Colour (0xff6fa8ff);
        case PresetCategory::VocalAtmos:     return juce::Colour (0xffff9ec7);
        case PresetCategory::SynthWalls:     return juce::Colour (0xffff8a5c);
        case PresetCategory::GranularDreams: return juce::Colour (0xff5fd6c4);
        case PresetCategory::DarkRude:       return juce::Colour (0xffff4d5e);
        case PresetCategory::Frozen:         return juce::Colour (0xff9ae6ff);
        case PresetCategory::RhythmicSpace:  return juce::Colour (0xffffd36b);
        case PresetCategory::Subtle:         return juce::Colour (0xffa8b3c7);
        case PresetCategory::User:
        case PresetCategory::Count:          break;
    }
    return juce::Colour (0xff8c7cff);
}

// -----------------------------------------------------------------------------
// The factory bank. Values are REAL units (seconds, Hz, %, choice indices).
// Anything not listed keeps its default. Documented in presets/README.md.
// -----------------------------------------------------------------------------
const std::vector<FactoryPreset>& factoryPresets()
{
    using P = PID;
    using M = ModuleType;
    using S = ModSource;
    using C = PresetCategory;

    static const std::vector<FactoryPreset> bank {
        // ================================================================ Outro Skies
        { "Blackout Skies", C::OutroSkies,
          { { P::CoreDecay, 18.0f }, { P::CoreSize, 0.95f }, { P::CoreColor, 1 }, { P::CoreDampHF, 4500.0f },
            { P::CoreBloom, 0.5f }, { P::CoreShimmer, 0.15f }, { P::CoreModDepth, 0.45f }, { P::OutMix, 0.45f },
            { P::OutHPF, 120.0f }, { P::StereoWidth, 1.4f }, { P::StereoBassMono, 150.0f }, { P::Lfo1Rate, 0.07f } },
          { M::Stereo }, { { S::Lfo1, P::CoreDampHF, 0.2f } } },
        { "Last Light", C::OutroSkies,
          { { P::CoreDecay, 25.0f }, { P::CoreSize, 1.0f }, { P::CoreShimmer, 0.35f }, { P::CoreShimBlend, 0.3f },
            { P::CoreBloom, 0.6f }, { P::CoreDampHF, 6000.0f }, { P::OutMix, 0.5f }, { P::ShdlyMix, 0.25f },
            { P::ShdlyDiv, 12 }, { P::Lfo1Rate, 0.05f } },
          { M::ShimmerDelay }, { { S::Lfo1, P::CoreShimmer, 0.15f } } },
        { "Fading City", C::OutroSkies,
          { { P::CoreDecay, 12.0f }, { P::CoreSize, 0.8f }, { P::CoreColor, 2 }, { P::TapeMix, 0.3f },
            { P::TapeAge, 0.6f }, { P::TapeFeedback, 0.55f }, { P::LofiSat, 0.3f }, { P::LofiWobble, 0.4f },
            { P::LofiMix, 0.6f }, { P::OutMix, 0.4f } },
          { M::TapeDelay, M::LoFi }, {} },
        { "Void Horizon", C::OutroSkies,
          { { P::CoreColor, 4 }, { P::CoreDecay, 30.0f }, { P::CoreSize, 0.9f }, { P::CoreDampHF, 3500.0f },
            { P::CoreBloom, 0.7f }, { P::OutMix, 0.5f }, { P::OutTilt, -2.0f } },
          {}, {} },
        { "Ember Outro", C::OutroSkies,
          { { P::CoreDecay, 10.0f }, { P::CoreColor, 1 }, { P::SatMode, 1 }, { P::SatDrive, 0.35f }, { P::SatMix, 0.5f },
            { P::FiltCutoff, 2500.0f }, { P::FiltRes, 0.25f }, { P::OutMix, 0.42f }, { P::Lfo1Rate, 0.1f } },
          { M::Saturator, M::Filter }, { { S::Lfo1, P::FiltCutoff, 0.25f } } },
        { "Tail Spin", C::OutroSkies,
          { { P::CoreDecay, 15.0f }, { P::CoreSize, 0.85f }, { P::CoreModDepth, 0.6f }, { P::CoreModRate, 0.25f },
            { P::MfxMode, 2 }, { P::MfxDepth, 0.6f }, { P::MfxMix, 0.5f }, { P::StereoOrbit, 0.4f },
            { P::StereoRate, 0.08f }, { P::OutMix, 0.45f } },
          { M::ModFX, M::Stereo }, {} },
        { "Midnight Exit", C::OutroSkies,
          { { P::CoreDecay, 20.0f }, { P::CoreColor, 1 }, { P::DroneMix, 0.25f }, { P::DroneVoicing, 1 },
            { P::DroneCutoff, 900.0f }, { P::TexType, 0 }, { P::TexLevel, -30.0f }, { P::TexFollow, 0.6f },
            { P::OutMix, 0.45f } },
          { M::Drone, M::Texture }, {} },

        // ================================================================ Intro Haze
        { "Bad Trip Intro", C::IntroHaze,
          { { P::CoreBloom, 0.85f }, { P::CoreDecay, 9.0f }, { P::CoreSize, 0.9f }, { P::RevMode, 1 }, { P::RevDiv, 16 },
            { P::RevMix, 0.5f }, { P::GranMix, 0.4f }, { P::GranSize, 220.0f }, { P::GranDensity, 30.0f },
            { P::GranSpray, 0.5f }, { P::GranPitchRand, 12.0f }, { P::GranRandMode, 1 }, { P::GranReverse, 0.4f },
            { P::DroneMix, 0.3f }, { P::DroneVoicing, 5 }, { P::DroneWave, 0.4f }, { P::DroneCutoff, 1400.0f },
            { P::OutMix, 0.55f } },
          { M::ReverseSwell, M::Granular, M::Drone }, {} },
        { "Morning Fog", C::IntroHaze,
          { { P::CoreBloom, 0.7f }, { P::CoreDecay, 7.0f }, { P::CoreSize, 0.75f }, { P::CoreDampHF, 5000.0f },
            { P::GranMix, 0.35f }, { P::GranSize, 300.0f }, { P::GranDensity, 15.0f }, { P::GranSpray, 0.6f },
            { P::GranPosition, 0.3f }, { P::FiltCutoff, 3500.0f }, { P::Lfo1Rate, 0.08f }, { P::OutMix, 0.5f } },
          { M::Granular, M::Filter }, { { S::Lfo1, P::GranPosition, 0.3f } } },
        { "Swell Theory", C::IntroHaze,
          { { P::CoreBloom, 1.0f }, { P::CoreDecay, 6.0f }, { P::CorePreDelay, 60.0f }, { P::RevSync, 0 },
            { P::RevLength, 1500.0f }, { P::RevMix, 0.6f }, { P::RevMode, 1 }, { P::OutMix, 0.5f } },
          { M::ReverseSwell }, {} },
        { "Rising Drone", C::IntroHaze,
          { { P::DroneMix, 0.45f }, { P::DroneVoicing, 2 }, { P::DroneGlide, 2000.0f }, { P::DroneFollow, 0.2f },
            { P::DroneCutoff, 1800.0f }, { P::DroneRes, 0.3f }, { P::CoreDecay, 8.0f }, { P::CoreShimmer, 0.2f },
            { P::CoreBloom, 0.6f }, { P::Lfo2Rate, 0.03f }, { P::OutMix, 0.5f } },
          { M::Drone }, { { S::Lfo2, P::DroneCutoff, 0.35f } } },
        { "Hazy Dawn", C::IntroHaze,
          { { P::CoreColor, 2 }, { P::CoreDecay, 8.0f }, { P::CoreBloom, 0.5f }, { P::LofiSat, 0.2f },
            { P::LofiWobble, 0.5f }, { P::LofiMix, 0.5f }, { P::MfxMix, 0.4f }, { P::OutMix, 0.5f } },
          { M::LoFi, M::ModFX }, {} },
        { "Cathedral Mist", C::IntroHaze,
          { { P::CoreDecay, 11.0f }, { P::CoreSize, 1.0f }, { P::CoreDiffEarly, 0.9f }, { P::CoreDiffLate, 0.85f },
            { P::CoreBloom, 0.75f }, { P::CoreShimmer, 0.25f }, { P::OutMix, 0.55f } },
          {}, {} },
        { "Slow Ignition", C::IntroHaze,
          { { P::SpecSmear, 0.8f }, { P::SpecBlur, 0.4f }, { P::SpecPhaseRand, 0.6f }, { P::SpecMix, 0.5f },
            { P::CoreDecay, 9.0f }, { P::CoreBloom, 0.8f }, { P::OutMix, 0.5f } },
          { M::Spectral }, {} },

        // ================================================================ Vocal Atmos
        { "Vocal Cushion", C::VocalAtmos,
          { { P::CoreDecay, 3.5f }, { P::CoreSize, 0.6f }, { P::CorePreDelay, 40.0f }, { P::CoreDampHF, 6000.0f },
            { P::OutDuck, 0.6f }, { P::OutDuckThresh, -28.0f }, { P::OutMix, 0.3f }, { P::OutHPF, 180.0f } },
          {}, {} },
        { "Mora Halo", C::VocalAtmos,
          { { P::CoreDecay, 5.0f }, { P::CoreSize, 0.75f }, { P::CoreShimmer, 0.2f }, { P::CoreBloom, 0.35f },
            { P::OutDuck, 0.5f }, { P::OutMix, 0.32f }, { P::OutHPF, 200.0f }, { P::ShdlyMix, 0.2f },
            { P::ShdlyDiv, 9 }, { P::ShdlyFeedback, 0.4f }, { P::ShdlyShimmer, 0.3f } },
          { M::ShimmerDelay }, {} },
        { "Whisper Room", C::VocalAtmos,
          { { P::CoreDecay, 2.2f }, { P::CoreSize, 0.45f }, { P::CoreColor, 1 }, { P::OutDuck, 0.4f },
            { P::OutMix, 0.25f }, { P::CorePreDelay, 25.0f } },
          {}, {} },
        { "Echo Bloom", C::VocalAtmos,
          { { P::TapeMix, 0.3f }, { P::TapeDiv, 11 }, { P::TapeFeedback, 0.4f }, { P::TapeAge, 0.5f },
            { P::CoreDecay, 4.5f }, { P::CoreBloom, 0.4f }, { P::OutDuck, 0.65f }, { P::OutMix, 0.33f } },
          { M::TapeDelay }, {} },
        { "Silk Plate", C::VocalAtmos,
          { { P::CoreDecay, 2.8f }, { P::CoreSize, 0.5f }, { P::CoreDiffEarly, 0.85f }, { P::CoreModDepth, 0.4f },
            { P::CoreDampHF, 9000.0f }, { P::OutMix, 0.28f }, { P::OutDuck, 0.3f } },
          {}, {} },
        { "Ghost Double", C::VocalAtmos,
          { { P::MfxMode, 2 }, { P::MfxDepth, 0.4f }, { P::MfxMix, 0.35f }, { P::MtapMix, 0.25f }, { P::MtapTaps, 4.0f },
            { P::MtapDiffusion, 0.7f }, { P::MtapTime, 500.0f }, { P::CoreDecay, 3.0f }, { P::OutDuck, 0.5f },
            { P::OutMix, 0.3f } },
          { M::ModFX, M::MultiTap }, {} },
        { "Hook Air", C::VocalAtmos,
          { { P::CoreDecay, 4.0f }, { P::CoreShimmer, 0.12f }, { P::OutHPF, 250.0f }, { P::OutTilt, 2.0f },
            { P::OutDuck, 0.55f }, { P::OutMix, 0.3f } },
          {}, {} },

        // ================================================================ Synth Walls
        { "Concrete Wall", C::SynthWalls,
          { { P::CoreDecay, 7.0f }, { P::CoreSize, 0.9f }, { P::SatDrive, 0.5f }, { P::FiltCutoff, 4000.0f },
            { P::FiltRes, 0.2f }, { P::OutGlueOn, 1 }, { P::OutGlueThresh, -24.0f }, { P::OutGlueRatio, 3.0f },
            { P::OutMix, 0.6f } },
          { M::Saturator, M::Filter }, {} },
        { "Neon Wall", C::SynthWalls,
          { { P::MfxMode, 0 }, { P::MfxStages, 8.0f }, { P::MfxDepth, 0.6f }, { P::MfxMix, 0.5f }, { P::SatMode, 1 },
            { P::SatDrive, 0.4f }, { P::CoreDecay, 6.0f }, { P::CoreShimmer, 0.25f }, { P::OutMix, 0.55f } },
          { M::ModFX, M::Saturator }, {} },
        { "Supersaw Fog", C::SynthWalls,
          { { P::DroneWave, 0.9f }, { P::DroneVoicing, 3 }, { P::DroneMix, 0.35f }, { P::DroneDetune, 25.0f },
            { P::SatDrive, 0.3f }, { P::CoreDecay, 8.0f }, { P::OutMix, 0.55f } },
          { M::Drone, M::Saturator }, {} },
        { "Pressure Front", C::SynthWalls,
          { { P::CoreDecay, 10.0f }, { P::CoreColor, 4 }, { P::SatMode, 2 }, { P::SatDrive, 0.45f }, { P::SatMix, 0.7f },
            { P::OutGlueOn, 1 }, { P::OutGlueThresh, -20.0f }, { P::OutMix, 0.6f } },
          { M::Saturator }, {} },
        { "Analog Monolith", C::SynthWalls,
          { { P::CoreColor, 2 }, { P::FiltType, 1 }, { P::FiltCutoff, 1800.0f }, { P::FiltRes, 0.45f },
            { P::FiltDrive, 0.4f }, { P::SatDrive, 0.35f }, { P::CoreDecay, 8.0f }, { P::OutMix, 0.55f },
            { P::Lfo1Sync, 1 }, { P::Lfo1Div, 17 } },
          { M::Filter, M::Saturator }, { { S::Lfo1, P::FiltCutoff, 0.3f } } },
        { "Stack Of Fifths", C::SynthWalls,
          { { P::ResoChord, 7 }, { P::ResoVoices, 6.0f }, { P::ResoDecay, 3.0f }, { P::ResoMix, 0.35f },
            { P::SatDrive, 0.3f }, { P::CoreDecay, 6.0f }, { P::OutMix, 0.5f } },
          { M::Resonator, M::Saturator }, {} },
        { "Wide Screen", C::SynthWalls,
          { { P::StereoWidth, 1.8f }, { P::StereoHaas, 12.0f }, { P::StereoBassMono, 150.0f }, { P::MfxMix, 0.4f },
            { P::CoreDecay, 7.0f }, { P::CoreSize, 0.95f }, { P::OutMix, 0.55f } },
          { M::ModFX, M::Stereo }, {} },

        // ================================================================ Granular Dreams
        { "Cloud Seeding", C::GranularDreams,
          { { P::GranMix, 0.6f }, { P::GranDensity, 40.0f }, { P::GranSize, 150.0f }, { P::GranSpray, 0.5f },
            { P::GranPitchRand, 7.0f }, { P::GranRandMode, 2 }, { P::GranReverse, 0.3f }, { P::GranWidth, 1.0f },
            { P::CoreDecay, 6.0f }, { P::OutMix, 0.5f } },
          { M::Granular }, {} },
        { "Glass Particles", C::GranularDreams,
          { { P::GranMix, 0.55f }, { P::GranSize, 40.0f }, { P::GranDensity, 80.0f }, { P::GranPitch, 12.0f },
            { P::GranSpray, 0.3f }, { P::GranWindow, 2 }, { P::CoreShimmer, 0.2f }, { P::CoreDecay, 5.0f },
            { P::OutMix, 0.45f } },
          { M::Granular }, {} },
        { "Reverse Snow", C::GranularDreams,
          { { P::GranReverse, 0.8f }, { P::GranSize, 250.0f }, { P::GranDensity, 20.0f }, { P::GranSpray, 0.7f },
            { P::GranMix, 0.6f }, { P::CoreDecay, 7.0f }, { P::CoreBloom, 0.5f }, { P::OutMix, 0.5f } },
          { M::Granular }, {} },
        { "Time Smear", C::GranularDreams,
          { { P::GranPosition, 0.5f }, { P::GranSpray, 0.8f }, { P::GranSize, 400.0f }, { P::GranDensity, 12.0f },
            { P::GranFeedback, 0.5f }, { P::GranMix, 0.55f }, { P::SpecSmear, 0.7f }, { P::SpecMix, 0.3f },
            { P::CoreDecay, 6.0f }, { P::OutMix, 0.5f } },
          { M::Granular, M::Spectral }, {} },
        { "Octave Rain", C::GranularDreams,
          { { P::GranPitchRand, 12.0f }, { P::GranRandMode, 1 }, { P::GranDensity, 60.0f }, { P::GranSize, 90.0f },
            { P::GranMix, 0.5f }, { P::TexType, 3 }, { P::TexLevel, -32.0f }, { P::CoreDecay, 5.0f },
            { P::OutMix, 0.45f } },
          { M::Granular, M::Texture }, {} },
        { "Drifting Grains", C::GranularDreams,
          { { P::GranMix, 0.55f }, { P::GranDensity, 25.0f }, { P::Lfo1Shape, 6 }, { P::Lfo1Rate, 0.2f },
            { P::Lfo2Rate, 0.13f }, { P::CoreDecay, 5.0f }, { P::OutMix, 0.5f } },
          { M::Granular }, { { S::Lfo1, P::GranPosition, 0.4f }, { S::Lfo2, P::GranPitch, 0.1f } } },
        { "Grain Field", C::GranularDreams,
          { { P::GranFeedback, 0.6f }, { P::GranDensity, 50.0f }, { P::GranSize, 200.0f }, { P::GranMix, 0.6f },
            { P::CoreDecay, 9.0f }, { P::OutMix, 0.55f } },
          { M::Granular }, {} },

        // ================================================================ Dark & Rude
        { "Rude Boy Room", C::DarkRude,
          { { P::CrushBits, 7.0f }, { P::CrushRate, 9000.0f }, { P::CrushMix, 0.5f }, { P::SatMode, 4 },
            { P::SatDrive, 0.5f }, { P::SatMix, 0.6f }, { P::CoreDecay, 3.5f }, { P::CoreColor, 3 }, { P::OutMix, 0.45f } },
          { M::Bitcrush, M::Saturator }, {} },
        { "Broken Speaker", C::DarkRude,
          { { P::SatMode, 2 }, { P::SatDrive, 0.7f }, { P::FiltMode, 2 }, { P::FiltCutoff, 1200.0f }, { P::FiltRes, 0.5f },
            { P::CrushBits, 9.0f }, { P::CrushMix, 0.3f }, { P::CoreDecay, 2.5f }, { P::OutMix, 0.5f } },
          { M::Saturator, M::Filter, M::Bitcrush }, {} },
        { "Metal Mouth", C::DarkRude,
          { { P::CombFreq, 180.0f }, { P::CombFeedback, 0.9f }, { P::CombSmear, 0.5f }, { P::CombMix, 0.45f },
            { P::RingFreq, 90.0f }, { P::RingMix, 0.3f }, { P::CoreDecay, 4.0f }, { P::CoreColor, 1 }, { P::OutMix, 0.45f } },
          { M::Comb, M::RingFreq }, {} },
        { "Sub Pressure", C::DarkRude,
          { { P::SatMode, 1 }, { P::SatDrive, 0.6f }, { P::FiltType, 1 }, { P::FiltCutoff, 600.0f }, { P::FiltRes, 0.6f },
            { P::CoreDecay, 5.0f }, { P::CoreColor, 4 }, { P::OutMix, 0.5f }, { P::OutGlueOn, 1 } },
          { M::Saturator, M::Filter }, {} },
        { "Digital Ghetto", C::DarkRude,
          { { P::CrushBits, 5.0f }, { P::CrushRate, 4000.0f }, { P::CrushJitter, 0.4f }, { P::CrushMix, 0.5f },
            { P::RingMode, 1 }, { P::RingShift, -40.0f }, { P::RingFeedback, 0.4f }, { P::RingMix, 0.35f },
            { P::CoreDecay, 4.0f }, { P::OutMix, 0.45f } },
          { M::Bitcrush, M::RingFreq }, {} },
        { "Grit Engine", C::DarkRude,
          { { P::SatMode, 3 }, { P::SatDrive, 0.55f }, { P::SatMix, 0.5f }, { P::TapeFeedback, 0.7f }, { P::TapeSat, 0.7f },
            { P::TapeAge, 0.7f }, { P::TapeMix, 0.3f }, { P::CoreDecay, 3.0f }, { P::OutMix, 0.5f } },
          { M::Saturator, M::TapeDelay }, {} },
        { "Noise Floor", C::DarkRude,
          { { P::TexType, 1 }, { P::TexLevel, -24.0f }, { P::TexFollow, 0.7f }, { P::LofiSat, 0.6f }, { P::LofiComp, 0.7f },
            { P::LofiTone, 0.7f }, { P::LofiNoise, 0.4f }, { P::LofiMix, 0.8f }, { P::CoreDecay, 3.0f }, { P::CoreColor, 3 },
            { P::OutMix, 0.45f } },
          { M::Texture, M::LoFi }, {} },

        // ================================================================ Frozen
        { "Glacier", C::Frozen,
          { { P::CoreDecay, 60.0f }, { P::CoreSize, 1.0f }, { P::CoreDampHF, 5000.0f }, { P::CoreModDepth, 0.5f },
            { P::OutMix, 0.55f } },
          {}, {} },
        { "Ice Cathedral", C::Frozen,
          { { P::CoreDecay, 50.0f }, { P::CoreShimmer, 0.3f }, { P::CoreShimB, 4 }, { P::CoreShimBlend, 0.4f },
            { P::OutMix, 0.55f } },
          {}, {} },
        { "Spectral Ice", C::Frozen,
          { { P::SpecSmear, 0.95f }, { P::SpecBlur, 0.5f }, { P::SpecPhaseRand, 0.8f }, { P::SpecMix, 0.6f },
            { P::CoreDecay, 40.0f }, { P::OutMix, 0.55f } },
          { M::Spectral }, {} },
        { "Permafrost", C::Frozen,
          { { P::CoreColor, 4 }, { P::CoreDecay, 60.0f }, { P::CoreBloom, 0.8f }, { P::DroneMix, 0.3f },
            { P::DroneVoicing, 4 }, { P::OutMix, 0.55f } },
          { M::Drone }, {} },
        { "Crystal Lattice", C::Frozen,
          { { P::ResoChord, 2 }, { P::ResoDecay, 8.0f }, { P::ResoBright, 0.7f }, { P::ResoMix, 0.3f },
            { P::CoreDecay, 45.0f }, { P::CoreShimmer, 0.2f }, { P::OutMix, 0.5f } },
          { M::Resonator }, {} },
        { "Still Water", C::Frozen,
          { { P::CoreDecay, 35.0f }, { P::CoreSize, 0.8f }, { P::CoreModDepth, 0.15f }, { P::CoreDampHF, 9000.0f },
            { P::GranDensity, 10.0f }, { P::GranSize, 450.0f }, { P::GranSpray, 0.2f }, { P::GranMix, 0.35f },
            { P::OutMix, 0.5f } },
          { M::Granular }, {} },
        { "Frozen Breath", C::Frozen,
          { { P::CoreDecay, 55.0f }, { P::Lfo1Rate, 0.05f }, { P::Lfo2Rate, 0.03f }, { P::OutMix, 0.55f } },
          {}, { { S::Lfo1, P::CoreDampHF, 0.3f }, { S::Lfo2, P::CoreSize, 0.1f } } },

        // ================================================================ Rhythmic Space
        { "Trance Gate Room", C::RhythmicSpace,
          { { P::GateDiv, 2 }, { P::GateSmooth, 0.2f }, { P::CoreDecay, 6.0f }, { P::CoreSize, 0.85f }, { P::OutMix, 0.5f } },
          { M::Gate }, {} },
        { "Dotted Echoes", C::RhythmicSpace,
          { { P::TapeDiv, 9 }, { P::TapeFeedback, 0.5f }, { P::TapeMix, 0.35f }, { P::CoreDecay, 3.5f }, { P::OutMix, 0.4f } },
          { M::TapeDelay }, {} },
        { "Pulse Field", C::RhythmicSpace,
          { { P::GateDiv, 1 }, { P::GateSwing, 0.3f }, { P::ShdlyDiv, 11 }, { P::ShdlyMix, 0.3f }, { P::CoreDecay, 5.0f },
            { P::OutMix, 0.5f } },
          { M::Gate, M::ShimmerDelay }, {}, 0x5555u },
        { "Ping Pong City", C::RhythmicSpace,
          { { P::ShdlyShimmer, 0.0f }, { P::ShdlyDiv, 9 }, { P::ShdlyFeedback, 0.55f }, { P::ShdlyMix, 0.4f },
            { P::CoreDecay, 2.5f }, { P::OutMix, 0.4f } },
          { M::ShimmerDelay }, {} },
        { "Stutter Bloom", C::RhythmicSpace,
          { { P::GateDiv, 3 }, { P::GateSmooth, 0.1f }, { P::RevDiv, 14 }, { P::RevMix, 0.4f }, { P::CoreDecay, 4.0f },
            { P::OutMix, 0.5f } },
          { M::Gate, M::ReverseSwell }, {}, 0x3333u },
        { "Sidechain Haze", C::RhythmicSpace,
          { { P::CoreDecay, 7.0f }, { P::OutDuck, 0.8f }, { P::OutDuckThresh, -30.0f }, { P::OutDuckRelease, 400.0f },
            { P::MtapMix, 0.3f }, { P::MtapSync, 1 }, { P::MtapDiv, 16 }, { P::OutMix, 0.55f } },
          { M::MultiTap }, {} },
        { "Tape Rhythm", C::RhythmicSpace,
          { { P::MtapSync, 1 }, { P::MtapDiv, 16 }, { P::MtapTaps, 6.0f }, { P::MtapShape, -0.4f }, { P::MtapDiffusion, 0.4f },
            { P::MtapFeedback, 0.3f }, { P::MtapMix, 0.4f }, { P::CoreDecay, 3.0f }, { P::OutMix, 0.45f } },
          { M::MultiTap }, {} },

        // ================================================================ Subtle
        { "Just Air", C::Subtle,
          { { P::CoreDecay, 1.8f }, { P::CoreSize, 0.4f }, { P::OutMix, 0.15f }, { P::CoreDampHF, 8000.0f } }, {}, {} },
        { "Small Glow", C::Subtle,
          { { P::CoreDecay, 2.5f }, { P::CoreSize, 0.5f }, { P::CoreBloom, 0.3f }, { P::OutMix, 0.18f }, { P::OutHPF, 150.0f } },
          {}, {} },
        { "Daily Plate", C::Subtle,
          { { P::CoreDecay, 2.0f }, { P::CoreSize, 0.55f }, { P::CoreDiffEarly, 0.85f }, { P::OutMix, 0.2f } }, {}, {} },
        { "Soft Width", C::Subtle,
          { { P::StereoWidth, 1.4f }, { P::StereoHaas, 6.0f }, { P::StereoBassMono, 150.0f }, { P::CoreDecay, 1.5f },
            { P::OutMix, 0.15f } },
          { M::Stereo }, {} },
        { "Warm Tail", C::Subtle,
          { { P::CoreColor, 2 }, { P::CoreDecay, 3.0f }, { P::OutMix, 0.2f }, { P::OutTilt, -1.5f } }, {}, {} },
        { "Ghost Shimmer", C::Subtle,
          { { P::CoreShimmer, 0.15f }, { P::CoreDecay, 3.0f }, { P::OutMix, 0.15f } }, {}, {} },
        { "Gentle Drift", C::Subtle,
          { { P::CoreDecay, 2.5f }, { P::CoreModDepth, 0.5f }, { P::CoreModRate, 0.2f }, { P::OutMix, 0.18f },
            { P::MfxMix, 0.2f }, { P::MfxDepth, 0.3f } },
          { M::ModFX }, {} },
    };

    return bank;
}

} // namespace aetherfall
