#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>
#include "PluginProcessor.h"

// =============================================================================
//  Parameter audit: EVERY parameter must audibly change the output.
//
//  For each parameter we build the context in which it is supposed to act
//  (its module in the chain, the mode that enables it, a modulation route for
//  modulator parameters...), render the same input twice with two different
//  values and require the outputs to differ. A failure means a "dead"
//  parameter (or a wrong context in this table).
// =============================================================================

using namespace aetherfall;

namespace
{
    struct Context
    {
        std::vector<ModuleType> chain;
        std::vector<std::pair<PID, float>> extra;   // other parameters to set first
        std::vector<std::pair<int, ModSlot>> mods;  // matrix routes
        bool coreOn = false;                        // keep the ATMOS CORE in the path
        float a = 0.0f, b = 0.0f;                   // the two values compared
        bool customValues = false;
    };

    void setParam (AetherfallProcessor& p, PID pid, float real)
    {
        auto* prm = p.getValueTreeState().getParameter (juce::String (std::string (info (pid).id)));
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
    }

    /** Deterministic stereo input: a pitched tone (changes note at 0.75 s, the
        first note is outside A minor), decorrelated noise, and regular bursts
        (transients). */
    float input (int n, int ch)
    {
        const double t = n / 48000.0;
        const double f = t < 0.75 ? 233.08 : 349.23;
        double s = 0.12 * std::sin (2.0 * 3.141592653589793 * f * t) + 0.04 * std::sin (2.0 * 3.141592653589793 * 2 * f * t);
        auto x = (std::uint32_t) n * 2654435761u + (std::uint32_t) ch * 7919u + 1u;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        s += 0.03 * ((double) (x & 0xffff) / 32768.0 - 1.0);
        const double burstPhase = std::fmod (t, 0.37);
        if (burstPhase < 0.03)
            s += 0.4 * (1.0 - burstPhase / 0.03) * ((double) ((x >> 8) & 0xff) / 128.0 - 1.0);
        return (float) (ch == 0 ? s : 0.85 * s);
    }

    std::vector<float> render (const Context& c, PID pid, float value, int seconds10)
    {
        AetherfallProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 256);

        setParam (p, PID::OutMix, 1.0f);
        setParam (p, PID::CoreMix, c.coreOn ? 1.0f : 0.0f);
        auto order = chain::emptyOrder();
        for (size_t i = 0; i < c.chain.size(); ++i)
            order[i] = c.chain[i];
        p.setChainOrder (order);
        for (const auto& [slot, s] : c.mods)
            p.setModSlot (slot, s);
        for (const auto& [ep, v] : c.extra)
            setParam (p, ep, v);
        setParam (p, pid, value);

        p.prepareToPlay (48000.0, 256);

        std::vector<float> out;
        const int total = 4800 * seconds10;
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += 256)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    buf.setSample (ch, i, input (pos + i, ch));
            p.processBlock (buf, midi);
            for (int i = 0; i < 256; ++i)
            {
                out.push_back (buf.getSample (0, i));
                out.push_back (buf.getSample (1, i));
            }
        }
        return out;
    }

    ModuleType moduleOf (PID pid)
    {
        for (const auto& m : moduleInfos)
            if ((int) pid >= (int) m.on && (int) pid <= (int) m.lastParam)
                return m.type;
        return ModuleType::None;
    }

    Context contextFor (PID pid)
    {
        Context c;
        const auto& inf = info (pid);
        c.a = inf.rangeMin();
        c.b = inf.rangeMax();

        auto routeTo = [&c] (ModSource s) { c.mods.push_back ({ 0, { s, (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None } }); c.extra.push_back ({ PID::Mod1Amount, -0.8f }); };

        // ---------------------------------------------------------------- modules
        if (const auto mod = moduleOf (pid); mod != ModuleType::None)
        {
            const auto& mi = moduleInfo (mod);
            c.chain = { mod };
            if (pid != mi.mix)
                c.extra.push_back ({ mi.mix, 1.0f });

            if (pid == mi.solo)
            {
                // Solo only matters with a second module: soloing M bypasses the filter.
                c.chain = { mod, mod == ModuleType::Filter ? ModuleType::Saturator : ModuleType::Filter };
                c.extra.push_back ({ PID::FiltCutoff, 300.0f });
                c.extra.push_back ({ PID::SatDrive, 1.0f });
            }
            if (pid == mi.parallel)
            {
                const auto other = mod == ModuleType::Filter ? ModuleType::Saturator : ModuleType::Filter;
                c.chain = { other, mod };
                c.extra.push_back ({ PID::FiltCutoff, 300.0f });
                c.extra.push_back ({ PID::SatDrive, 1.0f });
            }
        }

        // Core parameters need the ATMOS CORE in the path.
        if ((int) pid >= (int) PID::CoreMix && (int) pid <= (int) PID::CoreWidth)
            c.coreOn = true;

        switch (pid)
        {
            // ------------------------------------------------------------ global
            case PID::GlobKey:      c.chain = { ModuleType::Resonator }; c.extra.push_back ({ PID::ResoMix, 1.0f }); c.a = 0; c.b = 7; break;
            case PID::GlobScale:    c.chain = { ModuleType::Drone }; c.extra.push_back ({ PID::DroneMix, 1.0f }); c.a = 0; c.b = 1; break;
            case PID::GlobQuality:  c.coreOn = true; c.a = 0; c.b = 2; break;
            case PID::GlobModDepth: routeTo (ModSource::Lfo1); c.extra.push_back ({ PID::Lfo1Shape, 4 }); c.extra.push_back ({ PID::Lfo1Rate, 3.0f }); break;
            case PID::Bypass:       c.coreOn = true; break;

            // ------------------------------------------------------------ core
            case PID::CorePreDiv:    c.extra.push_back ({ PID::CorePreSync, 1 }); break;
            case PID::CoreShimA:     c.extra.push_back ({ PID::CoreShimmer, 0.9f }); c.extra.push_back ({ PID::CoreShimBlend, 0.0f }); break;
            case PID::CoreShimB:     c.extra.push_back ({ PID::CoreShimmer, 0.9f }); c.extra.push_back ({ PID::CoreShimBlend, 1.0f }); break;
            case PID::CoreShimBlend: c.extra.push_back ({ PID::CoreShimmer, 0.9f }); break;

            // ------------------------------------------------------------ output
            case PID::OutGlueThresh: case PID::OutGlueRatio: case PID::OutGlueAttack:
            case PID::OutGlueRelease: case PID::OutGlueMakeup:
                c.extra.push_back ({ PID::OutGlueOn, 1 }); c.extra.push_back ({ PID::OutGlueThresh, -40.0f });
                c.extra.push_back ({ PID::OutGlueRatio, 6.0f }); c.extra.push_back ({ PID::OutLimitOn, 0 });
                break;
            case PID::OutDuckThresh: case PID::OutDuckAttack: case PID::OutDuckRelease:
                c.extra.push_back ({ PID::OutDuck, 1.0f }); c.extra.push_back ({ PID::OutMix, 0.6f }); c.coreOn = true; break;
            case PID::OutDuck:       c.extra.push_back ({ PID::OutMix, 0.6f }); c.coreOn = true; break;
            case PID::OutClipDrive:  c.extra.push_back ({ PID::OutClipOn, 1 }); c.extra.push_back ({ PID::OutLimitOn, 0 }); break;
            case PID::OutClipOn:     c.extra.push_back ({ PID::OutClipDrive, 12.0f }); c.extra.push_back ({ PID::OutLimitOn, 0 }); break;
            case PID::OutLimitOn:    c.extra.push_back ({ PID::OutGain, 12.0f }); c.extra.push_back ({ PID::OutClipOn, 0 }); break;
            case PID::OutCeiling:    c.extra.push_back ({ PID::OutGain, 12.0f }); c.extra.push_back ({ PID::OutClipOn, 0 }); break;
            case PID::OutMix:        c.coreOn = true; break;

            // ------------------------------------------------------------ macros
            case PID::MacroHaze: case PID::MacroDrift: case PID::MacroGrit: case PID::MacroBloom:
                c.coreOn = true;
                c.chain = { ModuleType::Granular, ModuleType::Saturator, ModuleType::Drone };
                break;

            // ------------------------------------------------------------ module specifics
            case PID::GranRandMode:  c.extra.push_back ({ PID::GranPitchRand, 12.0f }); break;
            case PID::SpecShift:     c.extra.push_back ({ PID::SpecShimmer, 1.0f }); break;
            case PID::ShdlyTime:     c.extra.push_back ({ PID::ShdlySync, 0 }); break;
            case PID::TapeTime:      c.extra.push_back ({ PID::TapeSync, 0 }); break;
            case PID::MtapDiv:       c.extra.push_back ({ PID::MtapSync, 1 }); break;
            case PID::FiltVowel:     c.extra.push_back ({ PID::FiltMode, 4 }); break;
            case PID::MfxDiv:        c.extra.push_back ({ PID::MfxSync, 1 }); break;
            case PID::RingShift:     c.extra.push_back ({ PID::RingMode, 1 }); break;
            case PID::RingFeedback:  c.extra.push_back ({ PID::RingMode, 1 }); break;
            case PID::RingFreq:      c.extra.push_back ({ PID::RingMode, 0 }); break;
            case PID::StereoRate: case PID::StereoSync:
                c.extra.push_back ({ PID::StereoAutoPan, 0.8f }); break;
            case PID::StereoDiv:     c.extra.push_back ({ PID::StereoAutoPan, 0.8f }); c.extra.push_back ({ PID::StereoSync, 1 }); break;
            case PID::RevLength:     c.extra.push_back ({ PID::RevSync, 0 }); break;
            case PID::RevSync:       c.extra.push_back ({ PID::RevLength, 400.0f }); break;   // != the synced 1/2 (1 s)
            case PID::DroneFollow:   break;
            case PID::LofiRate:      c.extra.push_back ({ PID::LofiWobble, 1.0f }); break;
            default: break;
        }

        // ---------------------------------------------------------------- modulators
        const int p = (int) pid;
        if (p >= (int) PID::Lfo1Shape && p <= (int) PID::Lfo8Smooth)
        {
            const int k = (p - (int) PID::Lfo1Shape) / lfoParamStride, f = (p - (int) PID::Lfo1Shape) % lfoParamStride;
            c.mods.push_back ({ 0, { (ModSource) ((int) ModSource::Lfo1 + k), (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None } });
            c.extra.push_back ({ PID::Mod1Amount, -0.8f });
            c.extra.push_back ({ lfoParam (k, 1), 2.0f });           // 2 Hz so a cycle fits
            if (f == 3) c.extra.push_back ({ lfoParam (k, 2), 1 });   // division needs sync
            if (f == 7) c.extra.push_back ({ lfoParam (k, 0), 4 });   // smoothing shows on a square
            if (f == 5) { c.a = 0; c.b = 2; }                         // Free vs Transient retrigger
            if (f == 1) { c.a = 0.05f; c.b = 7.0f; }
            if (f == 4) { c.a = 0.0f; c.b = 0.5f; }                   // phase 0 and 1 are the same
        }
        if (p >= (int) PID::Env1Attack && p <= (int) PID::Env4Gain)
        {
            const int k = (p - (int) PID::Env1Attack) / envParamStride;
            c.mods.push_back ({ 0, { (ModSource) ((int) ModSource::Env1 + k), (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None } });
            c.extra.push_back ({ PID::Mod1Amount, -0.8f });
            c.coreOn = true;   // the "Wet" source needs a wet signal that differs from the input
        }
        if (p >= (int) PID::Seq1Div && p <= (int) PID::Seq2Polarity)
        {
            const int k = (p - (int) PID::Seq1Div) / seqParamStride;
            c.mods.push_back ({ 0, { (ModSource) ((int) ModSource::Seq1 + k), (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None } });
            c.extra.push_back ({ PID::Mod1Amount, -0.8f });
        }
        if (p >= (int) PID::Chaos1Mode && p <= (int) PID::Chaos2Smooth)
        {
            const int k = (p - (int) PID::Chaos1Mode) / chaosParamStride;
            c.mods.push_back ({ 0, { (ModSource) ((int) ModSource::Chaos1 + k), (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None } });
            c.extra.push_back ({ PID::Mod1Amount, -0.8f });
            c.extra.push_back ({ chaosParam (k, 1), 5.0f });
        }
        if (p >= (int) PID::Mod1Amount && p <= (int) PID::Mod32Amount)
        {
            const int slot = p - (int) PID::Mod1Amount;
            c.mods = { { slot, { ModSource::Lfo1, (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None } } };
            c.extra = { { PID::Lfo1Shape, 4 }, { PID::Lfo1Rate, 3.0f } };
        }

        return c;
    }

    double relativeDifference (const std::vector<float>& x, const std::vector<float>& y)
    {
        double diff = 0.0, ex = 0.0, ey = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double d = (double) x[i] - y[i];
            diff += d * d;
            ex += (double) x[i] * x[i];
            ey += (double) y[i] * y[i];
        }
        return std::sqrt (diff / std::max (1.0e-20, 0.5 * (ex + ey)));
    }
}

TEST_CASE ("Audit: every parameter audibly changes the output in its context", "[audit]")
{
    std::vector<std::string> dead, unstable;
    int checked = 0;

    for (const auto& inf : paramInfos)
    {
        const auto c = contextFor (inf.pid);
        const auto outA = render (c, inf.pid, c.a, 20);   // 2 s
        const auto outB = render (c, inf.pid, c.b, 20);
        const double rel = relativeDifference (outA, outB);
        ++checked;
        if (! (rel > 1.0e-3) || ! std::isfinite (rel))
            dead.push_back (std::string (inf.id) + " (rel diff " + std::to_string (rel) + ")");

        // Extreme values must stay finite and (with the safety limiter on) under 0 dBFS.
        bool limiterOn = true;
        for (const auto& [ep, v] : c.extra)
            if (ep == PID::OutLimitOn) limiterOn = v >= 0.5f;
        if (inf.pid == PID::OutLimitOn) limiterOn = false;
        for (const auto* out : { &outA, &outB })
        {
            float peak = 0.0f;
            bool finite = true;
            for (float v : *out) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            if (! finite || (limiterOn && peak > 1.0001f))
                unstable.push_back (std::string (inf.id) + " (peak " + std::to_string (peak) + (finite ? ")" : ", NaN/Inf)"));
        }
    }

    for (const auto& u : unstable)
        UNSCOPED_INFO ("unstable at an extreme value: " << u);
    CHECK (unstable.empty());

    for (const auto& d : dead)
        UNSCOPED_INFO ("no audible effect: " << d);
    INFO (dead.size() << " of " << checked << " parameters without effect");
    CHECK (dead.empty());
}
