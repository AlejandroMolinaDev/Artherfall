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
        c.phaseAcc.assign ((size_t) maxSize / 2 + 1, 0.0f);
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
        std::fill (c.phaseAcc.begin(), c.phaseAcc.end(), 0.0f);
    }
    inWrite = hopCounter = 0;
    sampleCounter = 0;
    wasFrozen = false;
}

void SpectralFreeze::processFrame (Channel& c, const ModContext& ctx)
{
    const int n = size;
    const int bins = n / 2 + 1;
    const auto& win = windows[size == 2048 ? 0 : 1];
    auto& fft = size == 2048 ? *fft11 : *fft12;

    // Latest n input samples in chronological order, windowed.
    for (int i = 0; i < n; ++i)
        fftData[(size_t) i] = c.inRing[(size_t) ((inWrite - n + i + maxSize) % maxSize)] * win[(size_t) i];
    std::fill (fftData.begin() + n, fftData.begin() + 2 * n, 0.0f);

    fft.performRealOnlyForwardTransform (fftData.data(), true);

    for (int k = 0; k < bins; ++k)
    {
        const float re = fftData[(size_t) (2 * k)], im = fftData[(size_t) (2 * k + 1)];
        mag[(size_t) k] = std::sqrt (re * re + im * im);
        phase[(size_t) k] = std::atan2 (im, re);
    }

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
        std::copy (c.smearMag.begin(), c.smearMag.begin() + bins, c.frozenMag.begin());

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

    // --- Tilt around 1 kHz (+-6 dB per decade at full tilt).
    const float kRef = 1000.0f * (float) n / (float) sampleRate;
    double eBase = 0.0, eOut = 0.0;

    for (int k = 0; k < bins; ++k)
    {
        float m = work[(size_t) k];
        if (std::abs (tilt) > 1.0e-3f)
            m *= std::pow (std::max (1.0f, (float) k) / kRef, 0.3f * tilt);

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
        const float s = (float) std::sqrt (4.0 * eBase / eOut);
        for (int k = 0; k < bins; ++k)
            work[(size_t) k] *= s;
    }
    std::copy (work.begin(), work.begin() + bins, c.prevOut.begin());

    // --- Phases: input phase while live; advancing phase while frozen.
    for (int k = 0; k < bins; ++k)
    {
        c.phaseAcc[(size_t) k] += (float) (twoPi * (double) k * hop / n);
        if (c.phaseAcc[(size_t) k] > (float) twoPi * 64.0f)
            c.phaseAcc[(size_t) k] = std::fmod (c.phaseAcc[(size_t) k], (float) twoPi);

        float ph = frozen ? c.phaseAcc[(size_t) k] : phase[(size_t) k];
        if (phaseRand > 1.0e-3f)
            ph += phaseRand * (float) pi * rng.nextBipolar();
        if (k == 0 || k == bins - 1)
            ph = 0.0f;

        fftData[(size_t) (2 * k)]     = work[(size_t) k] * std::cos (ph);
        fftData[(size_t) (2 * k + 1)] = work[(size_t) k] * std::sin (ph);
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

    const int newSize = ctx.getChoice (PID::SpecFFT) == 0 ? 2048 : 4096;
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
