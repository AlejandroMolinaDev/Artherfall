#include "PitchTracker.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

void PitchTracker::prepare (double sampleRate)
{
    inputRate = sampleRate;
    decimation = std::max (1, (int) std::lround (sampleRate / 12000.0));
    rate = sampleRate / decimation;

    const float fc = (float) (rate * 0.4);
    aa1.setLowPass (fc, 0.5412f, (float) sampleRate);
    aa2.setLowPass (fc, 1.3066f, (float) sampleRate);

    ring.assign ((size_t) windowSize * 2, 0.0f);
    frame.assign ((size_t) windowSize, 0.0f);
    diff.assign ((size_t) windowSize / 2 + 1, 0.0f);

    tauMin = std::max (2, (int) (rate / 1500.0));
    tauMax = std::min (windowSize / 2, (int) (rate / 45.0));
    reset();
}

void PitchTracker::reset()
{
    aa1.reset();
    aa2.reset();
    std::fill (ring.begin(), ring.end(), 0.0f);
    writeIndex = hopCounter = decimCounter = 0;
    frequency = confidence = 0.0f;
}

void PitchTracker::push (float x) noexcept
{
    const float filtered = aa2.process (aa1.process (x));
    if (++decimCounter < decimation)
        return;
    decimCounter = 0;

    ring[(size_t) writeIndex] = filtered;
    writeIndex = (writeIndex + 1) % (int) ring.size();

    if (++hopCounter >= hopSize)
    {
        hopCounter = 0;
        analyse();
    }
}

void PitchTracker::analyse() noexcept
{
    // Copy the latest window in chronological order.
    const int n = windowSize;
    const int size = (int) ring.size();
    double energy = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float v = ring[(size_t) ((writeIndex - n + i + size) % size)];
        frame[(size_t) i] = v;
        energy += (double) v * v;
    }

    // Silence / very quiet: unvoiced.
    if (energy / n < 1.0e-6)   // ~ -60 dBFS RMS
    {
        frequency = 0.0f;
        confidence = 0.0f;
        return;
    }

    // Difference function and cumulative mean normalisation.
    const int w = n / 2;
    diff[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double d = 0.0;
        for (int j = 0; j < w; ++j)
        {
            const float delta = frame[(size_t) j] - frame[(size_t) (j + tau)];
            d += (double) delta * delta;
        }
        running += d;
        diff[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
    }

    // First dip below the threshold (then walk down to the local minimum).
    constexpr float threshold = 0.15f;
    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (diff[(size_t) tau] < threshold)
        {
            while (tau + 1 <= tauMax && diff[(size_t) (tau + 1)] < diff[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        // No clear period: keep the last frequency but lower the confidence.
        confidence *= 0.5f;
        if (confidence < 0.1f)
            frequency = 0.0f;
        return;
    }

    // Parabolic interpolation around the minimum.
    double refined = best;
    if (best > 1 && best < tauMax)
    {
        const double a = diff[(size_t) (best - 1)], b = diff[(size_t) best], c = diff[(size_t) (best + 1)];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1.0e-12)
            refined += 0.5 * (a - c) / den;
    }

    frequency = (float) (rate / refined);
    confidence = dsp::clamp (1.0f - diff[(size_t) best], 0.0f, 1.0f);
}

} // namespace aetherfall
