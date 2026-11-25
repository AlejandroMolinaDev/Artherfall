#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

// =============================================================================
//  DAW-style scenarios run against the real plug-in binary (VST3).
//  They mirror the manual QA checklist of the README for FL Studio:
//  scan, instantiate, duplicate, hot sample-rate / buffer changes, offline
//  render, stopped transport, automation, bypass, latency, mono input,
//  several instances, load / unload, editor open / close.
// =============================================================================

namespace
{
    juce::String pluginPath()
    {
        auto env = juce::SystemStats::getEnvironmentVariable ("AETHERFALL_VST3_PATH", {});
       #ifdef AETHERFALL_VST3_PATH
        if (env.isEmpty()) env = AETHERFALL_VST3_PATH;
       #endif
        return env;
    }

    juce::PluginDescription describe()
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile (types, pluginPath());
        REQUIRE (types.size() == 1);
        return *types[0];
    }

    std::unique_ptr<juce::AudioPluginInstance> instantiate (double sr, int block)
    {
        juce::VST3PluginFormat format;
        juce::String error;
        auto inst = format.createInstanceFromDescription (describe(), sr, block, error);
        INFO ("error: " << error);
        REQUIRE (inst != nullptr);
        inst->setPlayConfigDetails (2, 2, sr, block);
        inst->prepareToPlay (sr, block);
        return inst;
    }

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    juce::AudioProcessorParameter* paramByName (juce::AudioPluginInstance& inst, const juce::String& name)
    {
        for (auto* p : inst.getParameters())
            if (p->getName (64) == name)
                return p;
        return nullptr;
    }

    struct Stats { bool finite = true; float peak = 0.0f; double energy = 0.0; };

    /** Processes `numSamples` with the given block-size pattern; input from gen(n, ch). */
    Stats run (juce::AudioPluginInstance& inst, int numSamples, const std::vector<int>& blocks,
               const std::function<float (int, int)>& gen, std::vector<float>* outLeft = nullptr,
               const std::function<void (int)>& beforeBlock = {})
    {
        Stats st;
        juce::MidiBuffer midi;
        int pos = 0, b = 0;
        while (pos < numSamples)
        {
            const int n = std::min (blocks[(size_t) (b++ % (int) blocks.size())], numSamples - pos);
            juce::AudioBuffer<float> buf (2, n);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    buf.setSample (ch, i, gen (pos + i, ch));
            if (beforeBlock) beforeBlock (pos);
            inst.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    const float y = buf.getSample (ch, i);
                    st.finite = st.finite && std::isfinite (y);
                    st.peak = std::max (st.peak, std::abs (y));
                    st.energy += (double) y * y;
                    if (outLeft != nullptr && ch == 0) outLeft->push_back (y);
                }
            pos += n;
        }
        return st;
    }

    float noise (int n, int ch)
    {
        // Deterministic noise (same for every instance / run).
        auto x = (std::uint32_t) (n * 2654435761u + (std::uint32_t) ch * 97u + 12345u);
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return 0.3f * ((float) (x & 0xffff) / 32768.0f - 1.0f);
    }
}

TEST_CASE ("Scan: the VST3 is found and described like FL Studio's plug-in manager sees it", "[host]")
{
    REQUIRE (pluginPath().isNotEmpty());
    const auto d = describe();
    CHECK (d.name == "AETHERFALL");
    CHECK (d.manufacturerName == "Aetherfall Audio");
    CHECK_FALSE (d.isInstrument);
    CHECK (d.pluginFormatName == "VST3");
    INFO ("category: " << d.category);
    CHECK (d.category.containsIgnoreCase ("Fx"));
}

TEST_CASE ("Instantiate and process: finite, audible, never above 0 dBFS", "[host]")
{
    auto inst = instantiate (44100.0, 512);
    const auto st = run (*inst, 44100 * 3, { 512 }, noise);
    CHECK (st.finite);
    CHECK (st.energy > 1.0);
    CHECK (st.peak <= 1.0f);
    CHECK (inst->getParameters().size() > 400);
    CHECK (inst->getTotalNumInputChannels() == 2);
    CHECK (inst->getTotalNumOutputChannels() == 2);

    // Parameter names are short enough for FL's "Browse parameters".
    for (auto* p : inst->getParameters())
        CHECK (p->getName (64).length() <= 16);
}

TEST_CASE ("Duplicating an instance (state copy) gives an identical twin", "[host]")
{
    auto a = instantiate (48000.0, 256);
    // Make A non-default.
    paramByName (*a, "Core Decay")->setValueNotifyingHost (0.8f);
    paramByName (*a, "Dry/Wet")->setValueNotifyingHost (0.7f);
    paramByName (*a, "Core Color")->setValueNotifyingHost (1.0f);
    run (*a, 4800, { 256 }, noise);   // let the parameter changes reach the processor

    juce::MemoryBlock state;
    a->getStateInformation (state);

    auto b = instantiate (48000.0, 256);
    b->setStateInformation (state.getData(), (int) state.getSize());
    a->reset();
    b->reset();

    std::vector<float> outA, outB;
    run (*a, 48000, { 256 }, noise, &outA);
    run (*b, 48000, { 256 }, noise, &outB);
    REQUIRE (outA.size() == outB.size());
    float maxDiff = 0.0f;
    for (size_t i = 0; i < outA.size(); ++i)
        maxDiff = std::max (maxDiff, std::abs (outA[i] - outB[i]));
    INFO ("max difference between original and duplicate: " << maxDiff);
    CHECK (maxDiff < 1.0e-4f);

    for (auto* name : { "Core Decay", "Dry/Wet", "Core Color" })
        CHECK_THAT (paramByName (*b, name)->getValue(), Catch::Matchers::WithinAbs (paramByName (*a, name)->getValue(), 1.0e-5));
}

TEST_CASE ("Hot sample-rate and buffer-size changes", "[host]")
{
    auto inst = instantiate (44100.0, 512);
    const std::pair<double, int> configs[] = { { 96000.0, 128 }, { 48000.0, 1024 }, { 192000.0, 64 },
                                               { 22050.0, 2048 }, { 88200.0, 480 }, { 44100.0, 512 } };
    for (const auto& [sr, block] : configs)
    {
        inst->releaseResources();
        inst->setPlayConfigDetails (2, 2, sr, block);
        inst->prepareToPlay (sr, block);
        // Variable blocks up to the announced maximum (the VST3 spec forbids larger
        // blocks; the processor itself is tested with oversized blocks in the unit tests).
        const auto st = run (*inst, (int) (sr * 0.5), { block, 1, 7, block / 2 + 3, block - 1 }, noise);
        INFO ("sr " << sr << " block " << block);
        CHECK (st.finite);
        CHECK (st.peak <= 1.0f);
    }
}

TEST_CASE ("Offline render (non-realtime) is fast and clean", "[host]")
{
    auto inst = instantiate (48000.0, 512);
    inst->setNonRealtime (true);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const auto st = run (*inst, 48000 * 30, { 512 }, noise);
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    WARN ("30 s rendered offline in " << ms << " ms (" << 30000.0 / ms << "x realtime)");
    CHECK (st.finite);
    CHECK (ms < 30000.0);   // faster than real time
}

TEST_CASE ("Stopped transport: the tail keeps ringing after the input stops", "[host]")
{
    auto inst = instantiate (48000.0, 512);   // no play head = transport stopped
    paramByName (*inst, "Dry/Wet")->setValueNotifyingHost (1.0f);
    std::vector<float> out;
    run (*inst, 48000 * 3, { 512 }, [] (int n, int ch) { return n < 48000 ? noise (n, ch) : 0.0f; }, &out);
    double tail = 0.0;
    for (size_t i = 48000 + 24000; i < out.size(); ++i)
        tail += (double) out[i] * out[i];
    INFO ("tail energy 0.5..2 s after the input stopped: " << tail);
    CHECK (tail > 1.0e-3);
    CHECK (inst->getTailLengthSeconds() > 1.0);
}

TEST_CASE ("Automation stress: random values on random parameters every block", "[host]")
{
    auto inst = instantiate (48000.0, 256);
    auto& params = inst->getParameters();
    juce::Random rng (7);
    // Everything is fair game except the bypass and the safety limiter (with the
    // limiter switched off, exceeding 0 dBFS would be legitimate).
    const juce::StringArray keep { "Limiter On", "Ceiling", "Bypass" };
    std::vector<juce::AudioProcessorParameter*> targets;
    for (auto* p : params)
        if (p != inst->getBypassParameter() && ! keep.contains (p->getName (64)))
            targets.push_back (p);

    const auto st = run (*inst, 48000 * 10, { 256, 64, 200, 1 }, noise, nullptr, [&] (int)
    {
        for (int k = 0; k < 12; ++k)
            targets[(size_t) rng.nextInt ((int) targets.size())]->setValue (rng.nextFloat());
    });
    CHECK (st.finite);
    CHECK (st.peak <= 1.0f);
}

TEST_CASE ("Host bypass returns the dry signal without clicks", "[host]")
{
    auto inst = instantiate (48000.0, 256);
    auto* bypass = inst->getBypassParameter();
    REQUIRE (bypass != nullptr);
    bypass->setValueNotifyingHost (1.0f);

    float maxErr = 0.0f, maxStep = 0.0f, prev = 0.0f;
    juce::MidiBuffer midi;
    for (int b = 0; b < 200; ++b)
    {
        juce::AudioBuffer<float> buf (2, 256), in (2, 256);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                in.setSample (ch, i, 0.5f * std::sin ((float) (b * 256 + i) * 0.03f));
        buf.makeCopyOf (in);
        inst->processBlock (buf, midi);
        for (int i = 0; i < 256; ++i)
        {
            if (b > 10) maxErr = std::max (maxErr, std::abs (buf.getSample (0, i) - in.getSample (0, i)));
            maxStep = std::max (maxStep, std::abs (buf.getSample (0, i) - prev));
            prev = buf.getSample (0, i);
        }
    }
    CHECK (maxErr < 1.0e-4f);
    CHECK (maxStep < 0.05f);   // the 220 Hz-ish sine moves ~0.015 per sample; a click would be far more
}

TEST_CASE ("Project reload with the Spectral module: latency reported and dry stays aligned", "[host]")
{
    auto inst = instantiate (48000.0, 512);
    paramByName (*inst, "Core Mix")->setValueNotifyingHost (0.0f);
    paramByName (*inst, "Dry/Wet")->setValueNotifyingHost (0.0f);   // dry only (delayed by latency)
    run (*inst, 2048, { 512 }, noise);

    // Edit the saved project state like a DAW would restore it: put Spectral in slot 1.
    juce::MemoryBlock state;
    inst->getStateInformation (state);
    auto hostXml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (hostXml != nullptr);
    auto* comp = hostXml->getChildByName ("IComponent");
    REQUIRE (comp != nullptr);
    juce::MemoryBlock compState;
    REQUIRE (compState.fromBase64Encoding (comp->getAllSubText()));

    auto pluginXml = juce::AudioProcessor::getXmlFromBinary (compState.getData(), (int) compState.getSize());
    REQUIRE (pluginXml != nullptr);
    auto* chain = pluginXml->getChildByName ("CHAIN");
    REQUIRE (chain != nullptr);
    chain->setAttribute ("order", "spectral");

    // Rebuild: our chunk followed by whatever the VST3 wrapper appended after it.
    juce::MemoryBlock newPlugin;
    juce::AudioProcessor::copyXmlToBinary (*pluginXml, newPlugin);
    const auto oldSize = 8 + (int) juce::ByteOrder::littleEndianInt (static_cast<const char*> (compState.getData()) + 4);
    if ((int) compState.getSize() > oldSize)
        newPlugin.append (static_cast<const char*> (compState.getData()) + oldSize, compState.getSize() - (size_t) oldSize);
    comp->deleteAllTextElements();
    comp->addTextElement (newPlugin.toBase64Encoding());

    auto reloaded = instantiate (48000.0, 512);
    juce::MemoryBlock newState;
    juce::AudioProcessor::copyXmlToBinary (*hostXml, newState);
    reloaded->setStateInformation (newState.getData(), (int) newState.getSize());

    std::vector<float> out;
    auto sine = [] (int n, int) { return 0.4f * std::sin ((float) n * 0.0431f); };
    run (*reloaded, 48000, { 512 }, sine, &out);
    pump (200);   // latency change notifications travel through the message thread

    INFO ("reported latency " << reloaded->getLatencySamples());
    CHECK (reloaded->getLatencySamples() == 4096);
    float maxErr = 0.0f;
    for (size_t i = 24000; i < out.size(); ++i)
        maxErr = std::max (maxErr, std::abs (out[i] - sine ((int) i - 4096, 0)));
    CHECK (maxErr < 1.0e-3f);
}

TEST_CASE ("Mono input track (mono -> stereo)", "[host]")
{
    auto inst = instantiate (48000.0, 512);
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    inst->releaseResources();                    // layouts change only while inactive (VST3)
    REQUIRE (inst->setBusesLayout (layout));
    inst->setPlayConfigDetails (1, 2, 48000.0, 512);
    inst->prepareToPlay (48000.0, 512);
    REQUIRE (inst->getTotalNumInputChannels() == 1);

    juce::MidiBuffer midi;
    double energyR = 0.0;
    for (int b = 0; b < 100; ++b)
    {
        juce::AudioBuffer<float> buf (2, 512);
        buf.clear();
        for (int i = 0; i < 512; ++i)
            buf.setSample (0, i, noise (b * 512 + i, 0));
        inst->processBlock (buf, midi);
        for (int i = 0; i < 512; ++i)
        {
            REQUIRE (std::isfinite (buf.getSample (1, i)));
            energyR += (double) buf.getSample (1, i) * buf.getSample (1, i);
        }
    }
    CHECK (energyR > 0.1);
}

TEST_CASE ("Eight instances at once (a busy FL project)", "[host]")
{
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> insts;
    for (int i = 0; i < 8; ++i)
        insts.push_back (instantiate (48000.0, 256));

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    bool finite = true;
    for (auto& inst : insts)
        finite = finite && run (*inst, 48000 * 2, { 256 }, noise).finite;
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    WARN ("8 instances, 2 s each: " << ms << " ms of CPU = " << 100.0 * ms / 2000.0 / 8.0 << " % of a core per instance");
    CHECK (finite);
}

TEST_CASE ("Load / unload the plug-in repeatedly", "[host]")
{
    for (int i = 0; i < 6; ++i)
    {
        auto inst = instantiate (48000.0, 512);
        run (*inst, 4800, { 512 }, noise);
        inst->releaseResources();
        inst.reset();
        pump (20);
    }
    SUCCEED();
}

TEST_CASE ("Editor opens, resizes and closes (needs a display)", "[host][editor]")
{
    if (juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isEmpty()
        && (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Linux) != 0)
    {
        WARN ("No display: editor test skipped (run under xvfb-run)");
        return;
    }

    auto inst = instantiate (48000.0, 512);
    for (int round = 0; round < 3; ++round)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (inst->createEditorAndMakeActive());
        REQUIRE (editor != nullptr);
        editor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
        editor->setVisible (true);
        pump (300);
        INFO ("editor size " << editor->getWidth() << "x" << editor->getHeight());
        CHECK (editor->getWidth() >= 750);
        CHECK (editor->getHeight() >= 480);
        editor->setSize (1500, 960);
        pump (200);
        run (*inst, 4800, { 512 }, noise);   // audio keeps running with the window open
        editor->setVisible (false);
        editor.reset();
        pump (100);
    }
}
