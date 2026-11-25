#include "RingFreq.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

namespace
{
    constexpr float coeffA[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    constexpr float coeffB[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
}

void RingFreq::Hilbert::process (float x, float& re, float& im) noexcept
{
    // Each section: y[n] = c^2 * (x[n] + y[n-2]) - x[n-2]
    float ia = x, ib = x;
    for (size_t k = 0; k < 4; ++k)
    {
        auto& s = a[k];
        const float c2 = coeffA[k] * coeffA[k];
        const float y = c2 * (ia + s.y2) - s.x2;
        s.x2 = s.x1; s.x1 = ia; s.y2 = s.y1; s.y1 = flushDenormal (y);
        ia = y;

        auto& t = b[k];
        const float d2 = coeffB[k] * coeffB[k];
        const float yb = d2 * (ib + t.y2) - t.x2;
        t.x2 = t.x1; t.x1 = ib; t.y2 = t.y1; t.y1 = flushDenormal (yb);
        ib = yb;
    }
    re = ia;
    im = bDelay;      // branch B is delayed by one sample
    bDelay = ib;
}

void RingFreq::prepareModule()
{
    feedback.prepare (sampleRate, 0.03);
    stereo.prepare (sampleRate, 0.03);
    reset();
}

void RingFreq::reset()
{
    for (auto& h : hilbert) h.reset();
    phase = { 0.0, 0.0 };
    lastOut = { 0.0f, 0.0f };
    primed = false;
}

void RingFreq::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const bool shiftMode = ctx.getChoice (PID::RingMode) == 1;
    const double targetLog = std::log (clamp ((double) ctx.get (PID::RingFreq), 0.1, 20000.0));
    const double targetShift = clamp ((double) ctx.get (PID::RingShift), -5000.0, 5000.0);

    if (! primed)
    {
        logFreq = targetLog;
        shiftHz = targetShift;
        feedback.setCurrentAndTarget (ctx.get (PID::RingFeedback));
        stereo.setCurrentAndTarget (ctx.get (PID::RingStereo));
        primed = true;
    }
    feedback.setTarget (clamp (ctx.get (PID::RingFeedback), 0.0f, 0.9f));
    stereo.setTarget (clamp (ctx.get (PID::RingStereo), 0.0f, 1.0f));

    const double glide = std::exp (-1.0 / (0.01 * sampleRate));
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        logFreq = targetLog + glide * (logFreq - targetLog);
        shiftHz = targetShift + glide * (shiftHz - targetShift);
        const float st = stereo.next();
        const float fb = feedback.next();

        if (! shiftMode)
        {
            const double inc = std::exp (logFreq) / sampleRate;
            phase[0] += inc;
            if (phase[0] >= 1.0) phase[0] -= 1.0;
            const double offset = 0.25 * st;   // R carrier up to 90 degrees ahead
            x[0][i] *= (float) std::sin (twoPi * phase[0]);
            x[1][i] *= (float) std::sin (twoPi * (phase[0] + offset));
        }
        else
        {
            for (int c = 0; c < 2; ++c)
            {
                // R morphs from the same shift (stereo 0) to the opposite one (stereo 1).
                const double hz = c == 0 ? shiftHz : shiftHz * (1.0 - 2.0 * st);
                auto& ph = phase[(size_t) c];
                ph += hz / sampleRate;
                ph -= std::floor (ph);

                float re = 0.0f, im = 0.0f;
                const float in = x[c][i] + fb * lastOut[(size_t) c];
                hilbert[(size_t) c].process (in, re, im);

                const float cs = (float) std::cos (twoPi * ph);
                const float sn = (float) std::sin (twoPi * ph);
                // (re, im) is the analytic signal; this sign gives an UPWARD shift for hz > 0.
                float y = re * cs + im * sn;
                y = std::abs (y) > 1.5f ? std::copysign (1.5f + fastTanh (std::abs (y) - 1.5f), y) : y;
                lastOut[(size_t) c] = y;
                x[c][i] = y;
            }
        }
    }
}

} // namespace aetherfall
