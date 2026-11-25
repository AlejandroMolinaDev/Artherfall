#include "SpectralFreeze.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void SpectralFreeze::prepareModule()
{
    fft11 = std::make_unique<juce::dsp::FFT> (11);
    fft12 = std::make_unique<juce::dsp::FFT> (12);

    for (int w = 0; w < 2; ++w)
    {
        const int n = 2048 << w;
        auto& win = windows[(size_t) w];
        win.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPi * i / n));   // periodic Hann
    }

    // Phase advance of every bin during one hop (hop = n / 4): 2*pi*k*hop/n = pi*k/2.
    for (int w = 0; w < 2; ++w)
    {
        const int n = 2048 << w, bins = n / 2 + 1;
        rotRe[(size_t) w].resize ((size_t) bins);
        rotIm[(size_t) w].resize ((size_t) bins);
        for (int k = 0; k < bins; ++k)
        {
            const double a = twoPi * (double) k * (n / 4) / n;
            rotRe[(size_t) w][(size_t) k] = (float) std::cos (a);
            rotIm[(size_t) w][(size_t) k] = (float) std::sin (a);
        }
    }
    tiltGain.assign ((size_t) maxSize / 2 + 1, 1.0f);

    fftData.assign ((size_t) maxSize * 2, 0.0f);
    mag.assign ((size_t) maxSize / 2 + 1, 0.0f);
    phase.assign ((size_t) maxSize / 2 + 1, 0.0f);
    work.assign ((size_t) maxSize / 2 + 1, 0.0f);

    const int outSize = nextPowerOfTwo (2 * maxSize + 8);
    outMask = outSize - 1;

    for (auto& c : ch)
    {
        c.inRing.assign ((size_t) maxSize, 0.0f);
        c.outRing.assign ((size_t) outSize, 0.0f);
        c.smearMag.assign ((size_t) maxSize / 2 + 1, 0.0f);
        c.frozenMag.assign ((size_t) maxSize / 2 + 1, 0.0f);
        c.prevOut.assign ((size_t) maxSize / 2 + 1, 0.0f);
        c.frozenRe.assign ((size_t) maxSize / 2 + 1, 1.0f);
        c.frozenIm.assign ((size_t) maxSize / 2 + 1, 0.0f);
    }
    reset();
}

void SpectralFreeze::reset()
{
    for (auto& c : ch)
    {
        std::fill (c.inRing.begin(), c.inRing.end(), 0.0f);
        std::fill (c.outRing.begin(), c.outRing.end(), 0.0f);
        std::fill (c.smearMag.begin(), c.smearMag.end(), 0.0f);
        std::fill (c.frozenMag.begin(), c.frozenMag.end(), 0.0f);
        std::fill (c.prevOut.begin(), c.prevOut.end(), 0.0f);
        std::fill (c.frozenRe.begin(), c.frozenRe.end(), 1.0f);
        std::fill (c.frozenIm.begin(), c.frozenIm.end(), 0.0f);
    }
    inWrite = hopCounter = 0;
    sampleCounter = 0;
    wasFrozen = false;
}

void SpectralFreeze::processFrame (Channel& c, const ModContext& ctx)
{
    const int n = size;
    const int bins = n / 2 + 1;
    const int sizeIdx = size == 2048 ? 0 : 1;
    const auto& win = windows[(size_t) sizeIdx];
    auto& fft = size == 2048 ? *fft11 : *fft12;

    // Latest n input samples in chronological order, windowed.
    for (int i = 0; i < n; ++i)
        fftData[(size_t) i] = c.inRing[(size_t) ((inWrite - n + i + maxSize) % maxSize)] * win[(size_t) i];
    std::fill (fftData.begin() + n, fftData.begin() + 2 * n, 0.0f);

    fft.performRealOnlyForwardTransform (fftData.data(), true);

    // Magnitudes; the phase is kept as a unit phasor (re, im) / |X| in `phase`
    // pairs, so no atan2 / sin / cos per bin is needed.
    for (int k = 0; k < bins; ++k)
        mag[(size_t) k] = std::sqrt (fftData[(size_t) (2 * k)] * fftData[(size_t) (2 * k)]
                                     + fftData[(size_t) (2 * k + 1)] * fftData[(size_t) (2 * k + 1)]);

    const float smear = clamp (ctx.get (PID::SpecSmear), 0.0f, 1.0f);
    const float blur = clamp (ctx.get (PID::SpecBlur), 0.0f, 1.0f);
    const float shimmer = clamp (ctx.get (PID::SpecShimmer), 0.0f, 1.0f);
    const float phaseRand = clamp (ctx.get (PID::SpecPhaseRand), 0.0f, 1.0f);
    const float tilt = clamp (ctx.get (PID::SpecTilt), -1.0f, 1.0f);
    const float ratio = std::exp2 (clamp (ctx.get (PID::SpecShift), -24.0f, 24.0f) / 12.0f);
    const bool frozen = ctx.getBool (PID::SpecFreeze);

    // --- Temporal smear (one-pole per bin, ~1.4 s at full smear).
    const float alpha = 0.985f * std::pow (smear, 0.35f);
    for (int k = 0; k < bins; ++k)
        c.smearMag[(size_t) k] = alpha * c.smearMag[(size_t) k] + (1.0f - alpha) * mag[(size_t) k];

    if (frozen && ! wasFrozen)
    {
        // Capture the spectrum and the current phases.
        std::copy (c.smearMag.begin(), c.smearMag.begin() + bins, c.frozenMag.begin());
        for (int k = 0; k < bins; ++k)
        {
            const float m = mag[(size_t) k];
            c.frozenRe[(size_t) k] = m > 1.0e-12f ? fftData[(size_t) (2 * k)] / m : 1.0f;
            c.frozenIm[(size_t) k] = m > 1.0e-12f ? fftData[(size_t) (2 * k + 1)] / m : 0.0f;
        }
    }

    const auto& base = frozen ? c.frozenMag : c.smearMag;

    // --- Blur across bins (forward + backward one-pole).
    for (int k = 0; k < bins; ++k)
        work[(size_t) k] = base[(size_t) k];
    if (blur > 1.0e-3f)
    {
        const float b = 1.0f - 0.92f * blur;
        for (int k = 1; k < bins; ++k)
            work[(size_t) k] = work[(size_t) (k - 1)] + b * (work[(size_t) k] - work[(size_t) (k - 1)]);
        for (int k = bins - 2; k >= 0; --k)
            work[(size_t) k] = work[(size_t) (k + 1)] + b * (work[(size_t) k] - work[(size_t) (k + 1)]);
    }

    // --- Tilt around 1 kHz (+-6 dB per decade at full tilt); gains cached.
    const bool useTilt = std::abs (tilt) > 1.0e-3f;
    if (useTilt && (std::abs (tilt - cachedTilt) > 1.0e-3f || cachedTiltSize != n))
    {
        const float kRef = 1000.0f * (float) n / (float) sampleRate;
        for (int k = 0; k < bins; ++k)
            tiltGain[(size_t) k] = std::pow (std::max (1.0f, (float) k) / kRef, 0.3f * tilt);
        cachedTilt = tilt;
        cachedTiltSize = n;
    }

    double eBase = 0.0, eOut = 0.0;
    for (int k = 0; k < bins; ++k)
    {
        float m = work[(size_t) k];
        if (useTilt)
            m *= tiltGain[(size_t) k];

        // --- Shimmer: previous output shifted by the ratio, fed back.
        if (shimmer > 1.0e-3f)
        {
            const float src = (float) k / ratio;
            const int i0 = (int) src;
            if (i0 + 1 < bins)
            {
                const float f = src - (float) i0;
                m += 0.7f * shimmer * (c.prevOut[(size_t) i0] + f * (c.prevOut[(size_t) i0 + 1] - c.prevOut[(size_t) i0]));
            }
        }

        eBase += (double) base[(size_t) k] * base[(size_t) k];
        eOut += (double) m * m;
        work[(size_t) k] = m;
    }

    // Keep the shimmer feedback from accumulating beyond ~+6 dB.
    if (eOut > 4.0 * eBase && eOut > 0.0)
    {
        const float sc = (float) std::sqrt (4.0 * eBase / eOut);
        for (int k = 0; k < bins; ++k)
            work[(size_t) k] *= sc;
    }
    std::copy (work.begin(), work.begin() + bins, c.prevOut.begin());

    // --- Phases: the input phasor while live; while frozen every bin keeps
    //     rotating at its own frequency (rotation table), optionally randomised.
    const auto& rre = rotRe[(size_t) sizeIdx];
    const auto& rim = rotIm[(size_t) sizeIdx];
    const bool renormalise = ++framesSinceNormalise >= 64;
    if (renormalise)
        framesSinceNormalise = 0;

    for (int k = 0; k < bins; ++k)
    {
        float ur, ui;
        if (frozen)
        {
            const float r0 = c.frozenRe[(size_t) k], i0 = c.frozenIm[(size_t) k];
            ur = r0 * rre[(size_t) k] - i0 * rim[(size_t) k];
            ui = r0 * rim[(size_t) k] + i0 * rre[(size_t) k];
            if (renormalise)
            {
                const float len = std::sqrt (ur * ur + ui * ui);
                if (len > 1.0e-6f) { ur /= len; ui /= len; }
            }
            c.frozenRe[(size_t) k] = ur;
            c.frozenIm[(size_t) k] = ui;
        }
        else
        {
            const float m = mag[(size_t) k];
            ur = m > 1.0e-12f ? fftData[(size_t) (2 * k)] / m : 1.0f;
            ui = m > 1.0e-12f ? fftData[(size_t) (2 * k + 1)] / m : 0.0f;
        }

        if (phaseRand > 1.0e-3f)
        {
            // Rotate by a random angle in +-pi * phaseRand.
            const double turns = 0.5 * (double) phaseRand * (double) rng.nextBipolar();
            const float cr = cosCycles (turns), sr = sinCycles (turns);
            const float nr = ur * cr - ui * sr;
            ui = ur * sr + ui * cr;
            ur = nr;
        }

        if (k == 0 || k == bins - 1)
        {
            ur = 1.0f;   // DC and Nyquist must stay real
            ui = 0.0f;
        }

        fftData[(size_t) (2 * k)]     = work[(size_t) k] * ur;
        fftData[(size_t) (2 * k + 1)] = work[(size_t) k] * ui;
    }

    fft.performRealOnlyInverseTransform (fftData.data());

    // Synthesis window + overlap-add. Hann x Hann at 75 % overlap sums to 1.5.
    // The frame covering inputs [t-n+1, t] lands at outputs [t-n+1+L, t+L].
    constexpr float olaScale = 1.0f / 1.5f;
    const long long startOut = sampleCounter - n + 1 + latencySamples;
    for (int i = 0; i < n; ++i)
        c.outRing[(size_t) ((startOut + i) & outMask)] += fftData[(size_t) i] * win[(size_t) i] * olaScale;
}

void SpectralFreeze::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    // Eco quality forces the smaller FFT (the reported latency does not change).
    const int newSize = (ctx.getChoice (PID::SpecFFT) == 0 || ctx.getChoice (PID::GlobQuality) == 0) ? 2048 : 4096;
    if (newSize != size)
    {
        size = newSize;
        order = size == 2048 ? 11 : 12;
        hop = size / 4;
        hopCounter = 0;
        for (auto& c : ch)
            std::fill (c.smearMag.begin(), c.smearMag.end(), 0.0f);
    }

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < 2; ++c)
            ch[(size_t) c].inRing[(size_t) inWrite] = x[c][i];
        inWrite = (inWrite + 1) % maxSize;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            for (auto& c : ch)
                processFrame (c, ctx);
            wasFrozen = ctx.getBool (PID::SpecFreeze);
        }

        const auto idxOut = (size_t) (sampleCounter & outMask);
        for (int c = 0; c < 2; ++c)
        {
            auto& ring = ch[(size_t) c].outRing;
            x[c][i] = sanitize (ring[idxOut]);
            ring[idxOut] = 0.0f;
        }
        ++sampleCounter;
    }
}

} // namespace aetherfall
