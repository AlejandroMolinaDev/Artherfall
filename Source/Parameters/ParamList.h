#pragma once

// =============================================================================
//  AETHERFALL — master parameter list (X-macro)
// =============================================================================
//
//  Every host-visible parameter is declared exactly once here. The list
//  generates:
//    - the `PID` enum (ParamIDs.h) used by the DSP to index parameter values,
//    - the constexpr `ParamInfo` table (ParamInfo.h) with ranges/defaults,
//    - the APVTS layout (ParamLayout.cpp).
//
//  Columns of X(...):
//    Enum, "string_id", "Display Name", Kind, min, max, default, skew, interval,
//    Unit, ChoiceSet, Flags
//
//  - Kind:     Float | Bool | Choice
//  - skew:     > 0  -> value placed at the middle of the knob (skew for centre)
//              0    -> linear
//              with the SymSkew flag: symmetric skew factor (bipolar ranges)
//  - interval: 0 = continuous, 1 = integer steps
//  - Flags:    NoAuto (not automatable), NoMod (not a modulation destination),
//              SymSkew (see above). Bool/Choice are never modulation targets.
//
//  RULES (state compatibility):
//    * String IDs are what the host/project stores. Never rename them once
//      presets exist (F9). Adding parameters anywhere is fine.
//    * Choice option lists may only grow at the end (ChoiceLists.h).
//    * Names are kept <= 16 characters (FL Studio "Browse parameters").
// =============================================================================

#define AF_FLOAT(X, E, id, nm, mn, mx, df, sk, un)   X(E, id, nm, Float,  mn, mx, df, sk, 0.0f, un,   None, 0)
#define AF_FLOAT_NM(X, E, id, nm, mn, mx, df, sk, un) X(E, id, nm, Float, mn, mx, df, sk, 0.0f, un,   None, ParamFlag::NoMod)
#define AF_FLOAT_SYM(X, E, id, nm, mn, mx, df, sk, un) X(E, id, nm, Float, mn, mx, df, sk, 0.0f, un,  None, ParamFlag::SymSkew)
#define AF_INT(X, E, id, nm, mn, mx, df)              X(E, id, nm, Float,  mn, mx, df, 0.0f, 1.0f, Int, None, 0)
#define AF_BOOL(X, E, id, nm, df)                     X(E, id, nm, Bool,   0.0f, 1.0f, df, 0.0f, 1.0f, None, None, 0)
#define AF_BOOL_NA(X, E, id, nm, df)                  X(E, id, nm, Bool,   0.0f, 1.0f, df, 0.0f, 1.0f, None, None, ParamFlag::NoAuto)
#define AF_CHOICE(X, E, id, nm, cs, df)               X(E, id, nm, Choice, 0.0f, 0.0f, df, 0.0f, 1.0f, None, cs, 0)

// Parameters shared by every chain module: on/off, dry-wet, solo, parallel.
// They belong to the module (not to the slot) so automation follows the module
// when it is dragged to another slot.
#define AF_MODULE_COMMON(X, E, idp, tag, defMix) \
    AF_BOOL    (X, E##On,       idp "_on",   tag " On",       1.0f) \
    AF_FLOAT   (X, E##Mix,      idp "_mix",  tag " Mix",      0.0f, 1.0f, defMix, 0.0f, Pct) \
    AF_BOOL_NA (X, E##Solo,     idp "_solo", tag " Solo",     0.0f) \
    AF_BOOL_NA (X, E##Parallel, idp "_par",  tag " Parallel", 0.0f)

#define AF_LFO(X, N) \
    AF_CHOICE (X, Lfo##N##Shape,    "lfo" #N "_shape",  "LFO" #N " Shape",    LfoShape, 0.0f) \
    AF_FLOAT  (X, Lfo##N##Rate,     "lfo" #N "_rate",   "LFO" #N " Rate",     0.01f, 20.0f, 0.5f, 1.0f, Hz) \
    AF_BOOL   (X, Lfo##N##Sync,     "lfo" #N "_sync",   "LFO" #N " Sync",     0.0f) \
    AF_CHOICE (X, Lfo##N##Div,      "lfo" #N "_div",    "LFO" #N " Div",      NoteDiv, 16.0f) \
    AF_FLOAT  (X, Lfo##N##Phase,    "lfo" #N "_phase",  "LFO" #N " Phase",    0.0f, 1.0f, 0.0f, 0.0f, Phase) \
    AF_CHOICE (X, Lfo##N##Trig,     "lfo" #N "_trig",   "LFO" #N " Retrig",   LfoTrigger, 0.0f) \
    AF_CHOICE (X, Lfo##N##Polarity, "lfo" #N "_pol",    "LFO" #N " Polarity", Polarity, 0.0f) \
    AF_FLOAT  (X, Lfo##N##Smooth,   "lfo" #N "_smooth", "LFO" #N " Smooth",   0.0f, 1.0f, 0.0f, 0.0f, Pct)

#define AF_ENV(X, N) \
    AF_FLOAT  (X, Env##N##Attack,  "env" #N "_atk",  "Env" #N " Attack",  0.1f, 500.0f, 10.0f, 20.0f, ms) \
    AF_FLOAT  (X, Env##N##Release, "env" #N "_rel",  "Env" #N " Release", 5.0f, 5000.0f, 250.0f, 300.0f, ms) \
    AF_CHOICE (X, Env##N##Source,  "env" #N "_src",  "Env" #N " Source",  EnvSource, 0.0f) \
    AF_FLOAT  (X, Env##N##Gain,    "env" #N "_gain", "Env" #N " Gain",    -24.0f, 24.0f, 0.0f, 0.0f, dB)

#define AF_SEQ(X, N) \
    AF_CHOICE (X, Seq##N##Div,      "seq" #N "_div",   "Seq" #N " Div",      NoteDiv, 5.0f) \
    AF_INT    (X, Seq##N##Steps,    "seq" #N "_steps", "Seq" #N " Steps",    2.0f, 32.0f, 16.0f) \
    AF_FLOAT  (X, Seq##N##Glide,    "seq" #N "_glide", "Seq" #N " Glide",    0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_CHOICE (X, Seq##N##Polarity, "seq" #N "_pol",   "Seq" #N " Polarity", Polarity, 1.0f)

#define AF_CHAOS(X, N) \
    AF_CHOICE (X, Chaos##N##Mode,   "chaos" #N "_mode",   "Chaos" #N " Mode",   ChaosMode, 0.0f) \
    AF_FLOAT  (X, Chaos##N##Rate,   "chaos" #N "_rate",   "Chaos" #N " Rate",   0.01f, 10.0f, 0.2f, 0.5f, Hz) \
    AF_FLOAT  (X, Chaos##N##Smooth, "chaos" #N "_smooth", "Chaos" #N " Smooth", 0.0f, 1.0f, 0.5f, 0.0f, Pct)

#define AF_MODSLOT(X, N) \
    AF_FLOAT_NM (X, Mod##N##Amount, "mod" #N "_amt", "Mod" #N " Amount", -1.0f, 1.0f, 0.0f, 0.0f, BiPct)

// -----------------------------------------------------------------------------
#define AF_PARAMS(X) \
    /* ---------------------------------------------------------------- Global */ \
    AF_BOOL    (X, Bypass,        "bypass",        "Bypass",        0.0f) \
    AF_CHOICE  (X, GlobKey,       "glob_key",      "Key",           Key, 9.0f) \
    AF_CHOICE  (X, GlobScale,     "glob_scale",    "Scale",         Scale, 1.0f) \
    AF_CHOICE  (X, GlobQuality,   "glob_quality",  "Quality",       Quality, 1.0f) \
    AF_FLOAT   (X, GlobModDepth,  "glob_moddepth", "Mod Depth",     0.0f, 2.0f, 1.0f, 0.0f, Pct) \
    /* ----------------------------------------------------------- Input stage */ \
    AF_FLOAT   (X, InGain,        "in_gain",       "In Gain",       -24.0f, 24.0f, 0.0f, 0.0f, dB) \
    AF_FLOAT   (X, InHPF,         "in_hpf",        "In HPF",        20.0f, 2000.0f, 20.0f, 200.0f, HzOffLow) \
    AF_FLOAT   (X, InLPF,         "in_lpf",        "In LPF",        1000.0f, 20000.0f, 20000.0f, 5000.0f, HzOffHigh) \
    AF_FLOAT   (X, InDrive,       "in_drive",      "In Drive",      0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, InBassMono,    "in_bassmono",   "In Bass Mono",  0.0f, 500.0f, 0.0f, 120.0f, HzOffLow) \
    /* ------------------------------------------------------------ ATMOS CORE */ \
    AF_FLOAT   (X, CoreMix,       "core_mix",        "Core Mix",        0.0f, 1.0f, 1.0f, 0.0f, Pct) \
    AF_FLOAT   (X, CoreSize,      "core_size",       "Core Size",       0.1f, 1.0f, 0.6f, 0.0f, Pct) \
    AF_FLOAT   (X, CoreDecay,     "core_decay",      "Core Decay",      0.2f, 60.0f, 4.0f, 4.0f, s) \
    AF_BOOL    (X, CoreFreeze,    "core_freeze",     "Core Freeze",     0.0f) \
    AF_FLOAT   (X, CorePreDelay,  "core_predelay",   "Core PreDelay",   0.0f, 500.0f, 20.0f, 80.0f, ms) \
    AF_BOOL    (X, CorePreSync,   "core_presync",    "Core Pre Sync",   0.0f) \
    AF_CHOICE  (X, CorePreDiv,    "core_prediv",     "Core Pre Div",    NoteDiv, 8.0f) \
    AF_FLOAT   (X, CoreDiffEarly, "core_diff_early", "Core Diff Early", 0.0f, 1.0f, 0.7f, 0.0f, Pct) \
    AF_FLOAT   (X, CoreDiffLate,  "core_diff_late",  "Core Diff Late",  0.0f, 1.0f, 0.6f, 0.0f, Pct) \
    AF_FLOAT   (X, CoreModDepth,  "core_mod_depth",  "Core Mod Depth",  0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, CoreModRate,   "core_mod_rate",   "Core Mod Rate",   0.05f, 5.0f, 0.4f, 0.5f, Hz) \
    AF_FLOAT   (X, CoreDampHF,    "core_damp_hf",    "Core Damp HF",    1000.0f, 20000.0f, 7000.0f, 5000.0f, Hz) \
    AF_FLOAT   (X, CoreDampLF,    "core_damp_lf",    "Core Damp LF",    20.0f, 1000.0f, 100.0f, 150.0f, Hz) \
    AF_FLOAT   (X, CoreShimmer,   "core_shimmer",    "Core Shimmer",    0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_CHOICE  (X, CoreShimA,     "core_shim_a",     "Core Shim Int A", ShimInterval, 0.0f) \
    AF_CHOICE  (X, CoreShimB,     "core_shim_b",     "Core Shim Int B", ShimInterval, 1.0f) \
    AF_FLOAT   (X, CoreShimBlend, "core_shim_blend", "Core Shim Blend", 0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, CoreBloom,     "core_bloom",      "Core Bloom",      0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_CHOICE  (X, CoreColor,     "core_color",      "Core Color",      ColorMode, 0.0f) \
    AF_FLOAT   (X, CoreWidth,     "core_width",      "Core Width",      0.0f, 1.0f, 1.0f, 0.0f, Pct) \
    /* ---------------------------------------------------------- Output stage */ \
    AF_FLOAT   (X, OutTilt,        "out_tilt",        "Out Tilt",     -6.0f, 6.0f, 0.0f, 0.0f, dB) \
    AF_FLOAT   (X, OutHPF,         "out_hpf",         "Out HPF",      20.0f, 2000.0f, 20.0f, 200.0f, HzOffLow) \
    AF_FLOAT   (X, OutLPF,         "out_lpf",         "Out LPF",      1000.0f, 20000.0f, 20000.0f, 5000.0f, HzOffHigh) \
    AF_FLOAT   (X, OutWidth,       "out_width",       "Width",        0.0f, 2.0f, 1.0f, 0.0f, Pct) \
    AF_BOOL    (X, OutGlueOn,      "out_glue_on",     "Glue On",      0.0f) \
    AF_FLOAT   (X, OutGlueThresh,  "out_glue_thr",    "Glue Thresh",  -40.0f, 0.0f, -18.0f, 0.0f, dB) \
    AF_FLOAT   (X, OutGlueRatio,   "out_glue_ratio",  "Glue Ratio",   1.0f, 10.0f, 2.0f, 3.0f, Ratio) \
    AF_FLOAT   (X, OutGlueAttack,  "out_glue_atk",    "Glue Attack",  0.1f, 100.0f, 10.0f, 10.0f, ms) \
    AF_FLOAT   (X, OutGlueRelease, "out_glue_rel",    "Glue Release", 10.0f, 1000.0f, 200.0f, 150.0f, ms) \
    AF_FLOAT   (X, OutGlueMakeup,  "out_glue_makeup", "Glue Makeup",  0.0f, 24.0f, 0.0f, 0.0f, dB) \
    AF_FLOAT   (X, OutDuck,        "out_duck",        "Ducking",      0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, OutDuckThresh,  "out_duck_thr",    "Duck Thresh",  -60.0f, 0.0f, -30.0f, 0.0f, dB) \
    AF_FLOAT   (X, OutDuckAttack,  "out_duck_atk",    "Duck Attack",  1.0f, 200.0f, 10.0f, 20.0f, ms) \
    AF_FLOAT   (X, OutDuckRelease, "out_duck_rel",    "Duck Release", 20.0f, 2000.0f, 300.0f, 250.0f, ms) \
    AF_BOOL    (X, OutClipOn,      "out_clip_on",     "Clip On",      1.0f) \
    AF_FLOAT   (X, OutClipDrive,   "out_clip_drive",  "Clip Drive",   0.0f, 24.0f, 0.0f, 0.0f, dB) \
    AF_BOOL    (X, OutLimitOn,     "out_limit_on",    "Limiter On",   1.0f) \
    AF_FLOAT   (X, OutCeiling,     "out_ceiling",     "Ceiling",      -12.0f, 0.0f, -0.3f, 0.0f, dB) \
    AF_FLOAT   (X, OutMix,         "out_mix",         "Dry/Wet",      0.0f, 1.0f, 0.35f, 0.0f, Pct) \
    AF_FLOAT   (X, OutGain,        "out_gain",        "Output",       -24.0f, 12.0f, 0.0f, 0.0f, dB) \
    /* ---------------------------------------------------------------- Macros */ \
    AF_FLOAT_NM(X, MacroHaze,  "macro_haze",  "HAZE",  0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT_NM(X, MacroDrift, "macro_drift", "DRIFT", 0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT_NM(X, MacroGrit,  "macro_grit",  "GRIT",  0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT_NM(X, MacroBloom, "macro_bloom", "BLOOM", 0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    /* ====================================================== Chain modules == */ \
    /* 1. Granular Cloud */ \
    AF_MODULE_COMMON(X, Gran, "gran", "Gran", 0.5f) \
    AF_FLOAT   (X, GranSize,      "gran_size",     "Gran Size",     10.0f, 500.0f, 120.0f, 100.0f, ms) \
    AF_FLOAT   (X, GranDensity,   "gran_density",  "Gran Density",  1.0f, 100.0f, 20.0f, 15.0f, PerSec) \
    AF_FLOAT   (X, GranPosition,  "gran_pos",      "Gran Position", 0.0f, 1.0f, 0.1f, 0.0f, Pct) \
    AF_FLOAT   (X, GranSpray,     "gran_spray",    "Gran Spray",    0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_FLOAT   (X, GranPitch,     "gran_pitch",    "Gran Pitch",    -24.0f, 24.0f, 0.0f, 0.0f, st) \
    AF_FLOAT   (X, GranPitchRand, "gran_pitch_rnd","Gran Pitch Rnd",0.0f, 12.0f, 0.0f, 0.0f, st) \
    AF_CHOICE  (X, GranRandMode,  "gran_rnd_mode", "Gran Rnd Mode", GranRandMode, 0.0f) \
    AF_FLOAT   (X, GranReverse,   "gran_reverse",  "Gran Reverse",  0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_CHOICE  (X, GranWindow,    "gran_window",   "Gran Window",   GranWindow, 0.0f) \
    AF_BOOL    (X, GranFreeze,    "gran_freeze",   "Gran Freeze",   0.0f) \
    AF_FLOAT   (X, GranFeedback,  "gran_fb",       "Gran Feedback", 0.0f, 0.9f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, GranWidth,     "gran_width",    "Gran Width",    0.0f, 1.0f, 0.7f, 0.0f, Pct) \
    /* 2. Spectral Freeze / Smear */ \
    AF_MODULE_COMMON(X, Spec, "spec", "Spec", 0.5f) \
    AF_CHOICE  (X, SpecFFT,       "spec_fft",      "Spec FFT Size", FFTSize, 1.0f) \
    AF_BOOL    (X, SpecFreeze,    "spec_freeze",   "Spec Freeze",   0.0f) \
    AF_FLOAT   (X, SpecSmear,     "spec_smear",    "Spec Smear",    0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, SpecBlur,      "spec_blur",     "Spec Blur",     0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_FLOAT   (X, SpecShift,     "spec_shift",    "Spec Shift",    -24.0f, 24.0f, 12.0f, 0.0f, st) \
    AF_FLOAT   (X, SpecShimmer,   "spec_shimmer",  "Spec Shimmer",  0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, SpecPhaseRand, "spec_phase",    "Spec Phase Rnd",0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, SpecTilt,      "spec_tilt",     "Spec Tilt",     -1.0f, 1.0f, 0.0f, 0.0f, BiPct) \
    /* 3. Shimmer Delay */ \
    AF_MODULE_COMMON(X, Shdly, "shdly", "ShDly", 0.4f) \
    AF_FLOAT   (X, ShdlyTime,     "shdly_time",    "ShDly Time",    10.0f, 2000.0f, 450.0f, 300.0f, ms) \
    AF_BOOL    (X, ShdlySync,     "shdly_sync",    "ShDly Sync",    1.0f) \
    AF_CHOICE  (X, ShdlyDiv,      "shdly_div",     "ShDly Div",     NoteDiv, 9.0f) \
    AF_FLOAT   (X, ShdlyFeedback, "shdly_fb",      "ShDly Feedback",0.0f, 0.98f, 0.5f, 0.0f, Pct) \
    X(ShdlyPitch, "shdly_pitch", "ShDly Pitch", Float, -24.0f, 24.0f, 12.0f, 0.0f, 1.0f, st, None, 0) \
    AF_FLOAT   (X, ShdlyShimmer,  "shdly_shimmer", "ShDly Shimmer", 0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_BOOL    (X, ShdlyPingPong, "shdly_pingpong","ShDly PingPong",1.0f) \
    AF_FLOAT   (X, ShdlyLPF,      "shdly_lpf",     "ShDly LPF",     500.0f, 20000.0f, 7000.0f, 4000.0f, Hz) \
    AF_FLOAT   (X, ShdlyHPF,      "shdly_hpf",     "ShDly HPF",     20.0f, 2000.0f, 150.0f, 200.0f, Hz) \
    /* 4. Tape Delay */ \
    AF_MODULE_COMMON(X, Tape, "tape", "Tape", 0.35f) \
    AF_FLOAT   (X, TapeTime,      "tape_time",     "Tape Time",     10.0f, 2000.0f, 375.0f, 300.0f, ms) \
    AF_BOOL    (X, TapeSync,      "tape_sync",     "Tape Sync",     1.0f) \
    AF_CHOICE  (X, TapeDiv,       "tape_div",      "Tape Div",      NoteDiv, 11.0f) \
    AF_FLOAT   (X, TapeFeedback,  "tape_fb",       "Tape Feedback", 0.0f, 1.05f, 0.45f, 0.0f, Pct) \
    AF_FLOAT   (X, TapeWow,       "tape_wow",      "Tape Wow",      0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, TapeFlutter,   "tape_flutter",  "Tape Flutter",  0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_FLOAT   (X, TapeSat,       "tape_sat",      "Tape Sat",      0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, TapeAge,       "tape_age",      "Tape Age",      0.0f, 1.0f, 0.4f, 0.0f, Pct) \
    AF_FLOAT   (X, TapeSpread,    "tape_spread",   "Tape Spread",   0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    /* 5. Multi-Tap Diffuse Delay */ \
    AF_MODULE_COMMON(X, Mtap, "mtap", "MTap", 0.4f) \
    AF_FLOAT   (X, MtapTime,      "mtap_time",     "MTap Time",     20.0f, 2000.0f, 600.0f, 400.0f, ms) \
    AF_BOOL    (X, MtapSync,      "mtap_sync",     "MTap Sync",     0.0f) \
    AF_CHOICE  (X, MtapDiv,       "mtap_div",      "MTap Div",      NoteDiv, 14.0f) \
    AF_FLOAT   (X, MtapFeedback,  "mtap_fb",       "MTap Feedback", 0.0f, 0.95f, 0.3f, 0.0f, Pct) \
    AF_INT     (X, MtapTaps,      "mtap_taps",     "MTap Taps",     2.0f, 8.0f, 8.0f) \
    AF_FLOAT   (X, MtapShape,     "mtap_shape",    "MTap Shape",    -1.0f, 1.0f, 0.0f, 0.0f, BiPct) \
    AF_FLOAT   (X, MtapDiffusion, "mtap_diff",     "MTap Diffusion",0.0f, 1.0f, 0.6f, 0.0f, Pct) \
    AF_FLOAT   (X, MtapWidth,     "mtap_width",    "MTap Width",    0.0f, 1.0f, 0.8f, 0.0f, Pct) \
    AF_FLOAT   (X, MtapTone,      "mtap_tone",     "MTap Tone",     500.0f, 20000.0f, 8000.0f, 4000.0f, Hz) \
    /* 6. Drone Generator */ \
    AF_MODULE_COMMON(X, Drone, "drone", "Drone", 0.4f) \
    AF_CHOICE  (X, DroneOctave,   "drone_oct",     "Drone Octave",  DroneOctave, 1.0f) \
    AF_CHOICE  (X, DroneVoicing,  "drone_voicing", "Drone Voicing", DroneVoicing, 1.0f) \
    AF_FLOAT   (X, DroneWave,     "drone_wave",    "Drone Wave",    0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, DroneCutoff,   "drone_cutoff",  "Drone Cutoff",  50.0f, 10000.0f, 1200.0f, 1000.0f, Hz) \
    AF_FLOAT   (X, DroneRes,      "drone_res",     "Drone Reso",    0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_FLOAT   (X, DroneNoise,    "drone_noise",   "Drone Noise",   0.0f, 1.0f, 0.1f, 0.0f, Pct) \
    AF_FLOAT   (X, DroneGlide,    "drone_glide",   "Drone Glide",   10.0f, 5000.0f, 800.0f, 500.0f, ms) \
    AF_FLOAT   (X, DroneDetune,   "drone_detune",  "Drone Detune",  0.0f, 50.0f, 12.0f, 0.0f, ct) \
    AF_FLOAT   (X, DroneFollow,   "drone_follow",  "Drone Follow",  0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_BOOL    (X, DroneQuantize, "drone_quant",   "Drone Quantize",1.0f) \
    AF_BOOL    (X, DroneHold,     "drone_hold",    "Drone Hold",    0.0f) \
    /* 7. Resonator Bank */ \
    AF_MODULE_COMMON(X, Reso, "reso", "Reso", 0.4f) \
    AF_CHOICE  (X, ResoChord,     "reso_chord",    "Reso Chord",    ResoChord, 1.0f) \
    AF_CHOICE  (X, ResoOctave,    "reso_oct",      "Reso Octave",   Octave5, 2.0f) \
    AF_INT     (X, ResoVoices,    "reso_voices",   "Reso Voices",   4.0f, 8.0f, 6.0f) \
    AF_FLOAT   (X, ResoDecay,     "reso_decay",    "Reso Decay",    0.05f, 10.0f, 1.5f, 1.5f, s) \
    AF_FLOAT   (X, ResoBright,    "reso_bright",   "Reso Bright",   0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, ResoDetune,    "reso_detune",   "Reso Detune",   0.0f, 1.0f, 0.1f, 0.0f, Pct) \
    AF_FLOAT   (X, ResoSpread,    "reso_spread",   "Reso Spread",   0.0f, 1.0f, 0.6f, 0.0f, Pct) \
    AF_FLOAT   (X, ResoDrive,     "reso_drive",    "Reso Drive",    0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    /* 8. Comb / Karplus Smear */ \
    AF_MODULE_COMMON(X, Comb, "comb", "Comb", 0.4f) \
    AF_FLOAT   (X, CombFreq,      "comb_freq",     "Comb Freq",     20.0f, 2000.0f, 110.0f, 200.0f, Hz) \
    AF_FLOAT   (X, CombFeedback,  "comb_fb",       "Comb Feedback", -0.99f, 0.99f, 0.85f, 0.0f, BiPct) \
    AF_FLOAT   (X, CombDamp,      "comb_damp",     "Comb Damp",     0.0f, 1.0f, 0.4f, 0.0f, Pct) \
    AF_FLOAT   (X, CombModDepth,  "comb_mod_depth","Comb Mod Depth",0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, CombModRate,   "comb_mod_rate", "Comb Mod Rate", 0.01f, 10.0f, 0.3f, 0.5f, Hz) \
    AF_FLOAT   (X, CombSpread,    "comb_spread",   "Comb Spread",   0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, CombSmear,     "comb_smear",    "Comb Smear",    0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_BOOL    (X, CombTrack,     "comb_track",    "Comb Track",    0.0f) \
    /* 9. Filter Sweep */ \
    AF_MODULE_COMMON(X, Filt, "filt", "Filt", 1.0f) \
    AF_CHOICE  (X, FiltMode,      "filt_mode",     "Filt Mode",     FilterMode, 0.0f) \
    AF_CHOICE  (X, FiltSlope,     "filt_slope",    "Filt Slope",    FilterSlope, 1.0f) \
    AF_CHOICE  (X, FiltType,      "filt_type",     "Filt Type",     FilterType, 0.0f) \
    AF_FLOAT   (X, FiltCutoff,    "filt_cutoff",   "Filt Cutoff",   20.0f, 20000.0f, 2000.0f, 1000.0f, Hz) \
    AF_FLOAT   (X, FiltRes,       "filt_res",      "Filt Reso",     0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, FiltDrive,     "filt_drive",    "Filt Drive",    0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, FiltVowel,     "filt_vowel",    "Filt Vowel",    0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, FiltStereo,    "filt_stereo",   "Filt Stereo",   0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    /* 10. Phaser / Flanger / Chorus */ \
    AF_MODULE_COMMON(X, Mfx, "mfx", "ModFX", 0.5f) \
    AF_CHOICE  (X, MfxMode,       "mfx_mode",      "ModFX Mode",    ModFXMode, 2.0f) \
    AF_FLOAT   (X, MfxRate,       "mfx_rate",      "ModFX Rate",    0.01f, 10.0f, 0.3f, 0.5f, Hz) \
    AF_BOOL    (X, MfxSync,       "mfx_sync",      "ModFX Sync",    0.0f) \
    AF_CHOICE  (X, MfxDiv,        "mfx_div",       "ModFX Div",     NoteDiv, 16.0f) \
    AF_FLOAT   (X, MfxDepth,      "mfx_depth",     "ModFX Depth",   0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, MfxFeedback,   "mfx_fb",        "ModFX Feedback",-0.95f, 0.95f, 0.3f, 0.0f, BiPct) \
    AF_INT     (X, MfxStages,     "mfx_stages",    "ModFX Stages",  2.0f, 12.0f, 6.0f) \
    AF_FLOAT   (X, MfxCenter,     "mfx_center",    "ModFX Center",  0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, MfxWidth,      "mfx_width",     "ModFX Width",   0.0f, 1.0f, 0.7f, 0.0f, Pct) \
    /* 11. Saturator */ \
    AF_MODULE_COMMON(X, Sat, "sat", "Sat", 1.0f) \
    AF_CHOICE  (X, SatMode,       "sat_mode",      "Sat Mode",      SatMode, 0.0f) \
    AF_FLOAT   (X, SatDrive,      "sat_drive",     "Sat Drive",     0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, SatTone,       "sat_tone",      "Sat Tone",      -1.0f, 1.0f, 0.0f, 0.0f, BiPct) \
    AF_FLOAT   (X, SatBias,       "sat_bias",      "Sat Bias",      0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, SatOutput,     "sat_out",       "Sat Output",    -24.0f, 12.0f, 0.0f, 0.0f, dB) \
    AF_CHOICE  (X, SatOversample, "sat_os",        "Sat Oversample",Oversampling, 1.0f) \
    AF_BOOL    (X, SatAutoGain,   "sat_autogain",  "Sat Auto Gain", 1.0f) \
    /* 12. Bitcrush / Decimate */ \
    AF_MODULE_COMMON(X, Crush, "crush", "Crush", 0.5f) \
    AF_FLOAT   (X, CrushBits,     "crush_bits",    "Crush Bits",    2.0f, 16.0f, 10.0f, 0.0f, Bits) \
    AF_FLOAT   (X, CrushRate,     "crush_rate",    "Crush Rate",    500.0f, 48000.0f, 16000.0f, 8000.0f, Hz) \
    AF_FLOAT   (X, CrushSmooth,   "crush_smooth",  "Crush Smooth",  0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, CrushJitter,   "crush_jitter",  "Crush Jitter",  0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    /* 13. Ring Mod / Freq Shifter */ \
    AF_MODULE_COMMON(X, Ring, "ring", "Ring", 0.5f) \
    AF_CHOICE  (X, RingMode,      "ring_mode",     "Ring Mode",     RingMode, 0.0f) \
    AF_FLOAT   (X, RingFreq,      "ring_freq",     "Ring Freq",     1.0f, 5000.0f, 220.0f, 300.0f, Hz) \
    AF_FLOAT_SYM(X, RingShift,    "ring_shift",    "Ring Shift",    -1000.0f, 1000.0f, 5.0f, 0.35f, Hz) \
    AF_FLOAT   (X, RingFeedback,  "ring_fb",       "Ring Feedback", 0.0f, 0.9f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, RingStereo,    "ring_stereo",   "Ring Stereo",   0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    /* 14. Stereo Engine */ \
    AF_MODULE_COMMON(X, Stereo, "stereo", "Stereo", 1.0f) \
    AF_FLOAT   (X, StereoWidth,   "stereo_width",  "Stereo Width",  0.0f, 2.0f, 1.2f, 0.0f, Pct) \
    AF_FLOAT   (X, StereoHaas,    "stereo_haas",   "Stereo Haas",   0.0f, 30.0f, 0.0f, 0.0f, ms) \
    AF_FLOAT   (X, StereoRate,    "stereo_rate",   "Stereo Rate",   0.01f, 10.0f, 0.2f, 0.5f, Hz) \
    AF_BOOL    (X, StereoSync,    "stereo_sync",   "Stereo Sync",   0.0f) \
    AF_CHOICE  (X, StereoDiv,     "stereo_div",    "Stereo Div",    NoteDiv, 17.0f) \
    AF_FLOAT   (X, StereoAutoPan, "stereo_pan",    "Stereo AutoPan",0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, StereoOrbit,   "stereo_orbit",  "Stereo Orbit",  0.0f, 1.0f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, StereoBassMono,"stereo_bassmono","Stereo BassMono",0.0f, 500.0f, 120.0f, 120.0f, HzOffLow) \
    /* 15. Gate / Trance Gate */ \
    AF_MODULE_COMMON(X, Gate, "gate", "Gate", 1.0f) \
    AF_CHOICE  (X, GateDiv,       "gate_div",      "Gate Div",      GateDiv, 2.0f) \
    AF_INT     (X, GateSteps,     "gate_steps",    "Gate Steps",    1.0f, 16.0f, 16.0f) \
    AF_FLOAT   (X, GateSwing,     "gate_swing",    "Gate Swing",    0.0f, 0.75f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, GateSmooth,    "gate_smooth",   "Gate Smooth",   0.0f, 1.0f, 0.2f, 0.0f, Pct) \
    AF_FLOAT   (X, GateDepth,     "gate_depth",    "Gate Depth",    0.0f, 1.0f, 1.0f, 0.0f, Pct) \
    /* 16. Reverse Swell */ \
    AF_MODULE_COMMON(X, Rev, "rev", "RevSw", 0.5f) \
    AF_CHOICE  (X, RevMode,       "rev_mode",      "RevSw Mode",    RevMode, 0.0f) \
    AF_FLOAT   (X, RevLength,     "rev_length",    "RevSw Length",  100.0f, 4000.0f, 1000.0f, 800.0f, ms) \
    AF_BOOL    (X, RevSync,       "rev_sync",      "RevSw Sync",    1.0f) \
    AF_CHOICE  (X, RevDiv,        "rev_div",       "RevSw Div",     NoteDiv, 14.0f) \
    AF_FLOAT   (X, RevFeedback,   "rev_fb",        "RevSw Feedback",0.0f, 0.9f, 0.0f, 0.0f, Pct) \
    AF_FLOAT   (X, RevXfade,      "rev_xfade",     "RevSw Xfade",   0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    /* 17. Texture Layer */ \
    AF_MODULE_COMMON(X, Tex, "tex", "Tex", 1.0f) \
    AF_CHOICE  (X, TexType,       "tex_type",      "Tex Type",      TexType, 0.0f) \
    AF_FLOAT   (X, TexLevel,      "tex_level",     "Tex Level",     -60.0f, 0.0f, -24.0f, 0.0f, dB) \
    AF_FLOAT   (X, TexFollow,     "tex_follow",    "Tex Follow",    0.0f, 1.0f, 0.6f, 0.0f, Pct) \
    AF_FLOAT   (X, TexTone,       "tex_tone",      "Tex Tone",      0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, TexDensity,    "tex_density",   "Tex Density",   0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, TexWidth,      "tex_width",     "Tex Width",     0.0f, 1.0f, 0.8f, 0.0f, Pct) \
    /* 18. Lo-Fi Tape Machine */ \
    AF_MODULE_COMMON(X, Lofi, "lofi", "LoFi", 1.0f) \
    AF_FLOAT   (X, LofiSat,       "lofi_sat",      "LoFi Sat",      0.0f, 1.0f, 0.4f, 0.0f, Pct) \
    AF_FLOAT   (X, LofiComp,      "lofi_comp",     "LoFi Comp",     0.0f, 1.0f, 0.4f, 0.0f, Pct) \
    AF_FLOAT   (X, LofiTone,      "lofi_tone",     "LoFi Tone",     0.0f, 1.0f, 0.5f, 0.0f, Pct) \
    AF_FLOAT   (X, LofiWobble,    "lofi_wobble",   "LoFi Wobble",   0.0f, 1.0f, 0.3f, 0.0f, Pct) \
    AF_FLOAT   (X, LofiRate,      "lofi_rate",     "LoFi Rate",     0.1f, 5.0f, 0.6f, 1.0f, Hz) \
    AF_FLOAT   (X, LofiNoise,     "lofi_noise",    "LoFi Noise",    0.0f, 1.0f, 0.1f, 0.0f, Pct) \
    /* ========================================================= Modulation == */ \
    AF_LFO(X, 1) AF_LFO(X, 2) AF_LFO(X, 3) AF_LFO(X, 4) \
    AF_LFO(X, 5) AF_LFO(X, 6) AF_LFO(X, 7) AF_LFO(X, 8) \
    AF_ENV(X, 1) AF_ENV(X, 2) AF_ENV(X, 3) AF_ENV(X, 4) \
    AF_SEQ(X, 1) AF_SEQ(X, 2) \
    AF_CHAOS(X, 1) AF_CHAOS(X, 2) \
    AF_MODSLOT(X, 1)  AF_MODSLOT(X, 2)  AF_MODSLOT(X, 3)  AF_MODSLOT(X, 4) \
    AF_MODSLOT(X, 5)  AF_MODSLOT(X, 6)  AF_MODSLOT(X, 7)  AF_MODSLOT(X, 8) \
    AF_MODSLOT(X, 9)  AF_MODSLOT(X, 10) AF_MODSLOT(X, 11) AF_MODSLOT(X, 12) \
    AF_MODSLOT(X, 13) AF_MODSLOT(X, 14) AF_MODSLOT(X, 15) AF_MODSLOT(X, 16) \
    AF_MODSLOT(X, 17) AF_MODSLOT(X, 18) AF_MODSLOT(X, 19) AF_MODSLOT(X, 20) \
    AF_MODSLOT(X, 21) AF_MODSLOT(X, 22) AF_MODSLOT(X, 23) AF_MODSLOT(X, 24) \
    AF_MODSLOT(X, 25) AF_MODSLOT(X, 26) AF_MODSLOT(X, 27) AF_MODSLOT(X, 28) \
    AF_MODSLOT(X, 29) AF_MODSLOT(X, 30) AF_MODSLOT(X, 31) AF_MODSLOT(X, 32)
