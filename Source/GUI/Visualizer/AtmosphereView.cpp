#include "AtmosphereView.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

AtmosphereView::AtmosphereView (const Meters& m) : meters (m), accent (palette::accentDefault)
{
    for (auto& p : particles)
        p = { rng.nextFloat(), rng.nextFloat(), (rng.nextFloat() - 0.5f) * 0.002f, -0.0005f - rng.nextFloat() * 0.0015f,
              1.0f + rng.nextFloat() * 2.5f, rng.nextFloat() };
    startTimerHz (30);
}

AtmosphereView::~AtmosphereView() { stopTimer(); }

void AtmosphereView::visibilityChanged()
{
    if (isShowing()) startTimerHz (30); else stopTimer();
}

void AtmosphereView::timerCallback()
{
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
