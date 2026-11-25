// Host-level tests: AETHERFALL is loaded from its built VST3 bundle through
// the VST3 API (exactly like a DAW does) and driven through DAW-style
// scenarios. The bundle path comes from the build (AETHERFALL_VST3_PATH) or
// from the environment variable of the same name.
#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    return Catch::Session().run (argc, argv);
}
