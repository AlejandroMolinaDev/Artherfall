#include "AtmosphereView.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"
#include "DSP/Utils/DspMath.h"
#include "PluginProcessor.h"

namespace aetherfall
{

AtmosphereView::AtmosphereView (AetherfallProcessor& p, const Meters& m)
    : processor (p), meters (m), accent (palette::accentDefault)
{
    history.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    scratch.assign ((size_t) fftSize, 0.0f);
    bands.fill (-100.0f);
    for (auto& particle : particles)
        particle = { rng.nextFloat(), rng.nextFloat(), (rng.nextFloat() - 0.5f) * 0.002f, -0.0005f - rng.nextFloat() * 0.0015f,
                     1.0f + rng.nextFloat() * 2.5f, rng.nextFloat() };
    startTimerHz (30);
}

AtmosphereView::~AtmosphereView() { stopTimer(); }

void AtmosphereView::visibilityChanged()
{
    if (isShowing()) startTimerHz (30); else stopTimer();
}

void AtmosphereView::updateSpectrum()
{
    // Pull the newest wet samples into a sliding window of fftSize.
    const int got = processor.readSpectrumSamples (scratch.data(), fftSize);
    if (got > 0)
    {
        std::move (history.begin() + got, history.end(), history.begin());
        std::copy (scratch.begin(), scratch.begin() + got, history.end() - got);
    }

    std::copy (history.begin(), history.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // 96 log-spaced bands from 30 Hz to 18 kHz; fast rise, slow fall.
    const double sr = 48000.0;
    for (int b = 0; b < numBands; ++b)
    {
        const double f0 = 30.0 * std::pow (600.0, (double) b / numBands);
        const double f1 = 30.0 * std::pow (600.0, (double) (b + 1) / numBands);
        const int k0 = juce::jlimit (1, fftSize / 2, (int) (f0 / sr * fftSize));
        const int k1 = juce::jlimit (k0 + 1, fftSize / 2 + 1, (int) (f1 / sr * fftSize) + 1);
        float mag = 0.0f;
        for (int k = k0; k < k1; ++k)
            mag = std::max (mag, fftData[(size_t) k]);
        const float db = dsp::gainToDb (mag / (float) fftSize * 4.0f + 1.0e-9f);
        bands[(size_t) b] = db > bands[(size_t) b] ? db : bands[(size_t) b] + (db - bands[(size_t) b]) * 0.15f;
    }
}

void AtmosphereView::timerCallback()
{
    updateSpectrum();

    const float rms = 0.5f * (meters.outRmsL.load() + meters.outRmsR.load());
    const float target = dsp::clamp ((dsp::gainToDb (rms + 1.0e-6f) + 54.0f) / 54.0f, 0.0f, 1.0f);
    level += (target - level) * (target > level ? 0.3f : 0.06f);
    phase += 0.004f + 0.01f * level;

    const float speed = 0.6f + 2.5f * level;
    for (auto& p : particles)
    {
        p.x += p.vx * speed + 0.0006f * std::sin (phase * 3.0f + p.y * 9.0f);
        p.y += p.vy * speed;
        p.life -= 0.004f;
        if (p.y < -0.05f || p.life <= 0.0f || p.x < -0.05f || p.x > 1.05f)
            p = { rng.nextFloat(), 1.02f, (rng.nextFloat() - 0.5f) * 0.002f, -0.0005f - rng.nextFloat() * 0.0015f,
                  1.0f + rng.nextFloat() * 2.5f, 0.6f + 0.4f * rng.nextFloat() };
    }
    repaint();
}

void AtmosphereView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (palette::panel);
    g.fillRoundedRectangle (r, 10.0f);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);

        // Fog: a few large soft blobs drifting slowly, denser with level.
        for (int i = 0; i < 4; ++i)
        {
            const float t = phase * (0.5f + 0.2f * (float) i) + (float) i * 1.7f;
            const float cx = r.getX() + r.getWidth() * (0.5f + 0.32f * std::sin (t));
            const float cy = r.getY() + r.getHeight() * (0.62f + 0.18f * std::cos (t * 0.8f));
            const float rad = r.getWidth() * (0.35f + 0.1f * (float) i * 0.5f);
            juce::ColourGradient fog (accent.withAlpha (0.05f + 0.16f * level), cx, cy,
                                      accent.withAlpha (0.0f), cx + rad, cy, true);
            g.setGradientFill (fog);
            g.fillEllipse (cx - rad, cy - rad * 0.6f, rad * 2.0f, rad * 1.2f);
        }

        // Spectrum of the wet signal (faint curve along the bottom).
        {
            juce::Path spec;
            const float base = r.getBottom();
            const float h = r.getHeight() * 0.45f;
            spec.startNewSubPath (r.getX(), base);
            for (int b = 0; b < numBands; ++b)
            {
                const float x = r.getX() + r.getWidth() * (float) b / (numBands - 1);
                const float y = base - h * dsp::clamp ((bands[(size_t) b] + 90.0f) / 90.0f, 0.0f, 1.0f);
                spec.lineTo (x, y);
            }
            spec.lineTo (r.getRight(), base);
            spec.closeSubPath();
            g.setColour (accent.withAlpha (0.10f));
            g.fillPath (spec);
            g.setColour (accent.withAlpha (0.35f));
            g.strokePath (spec, juce::PathStrokeType (1.2f));
        }

        // Particles (grains).
        for (const auto& p : particles)
        {
            const float a = juce::jlimit (0.0f, 1.0f, p.life) * (0.25f + 0.6f * level);
            g.setColour (palette::text.interpolatedWith (accent, 0.5f).withAlpha (a));
            const float s = p.size * (0.8f + 0.8f * level);
            g.fillEllipse (r.getX() + p.x * r.getWidth() - s * 0.5f, r.getY() + p.y * r.getHeight() - s * 0.5f, s, s);
        }
    }

    g.setColour (palette::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

} // namespace aetherfall
