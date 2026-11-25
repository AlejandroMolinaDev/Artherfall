// Custom Catch2 main: initialises JUCE (MessageManager etc.) once for the
// whole run so processors and value trees behave like inside a host.
#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    return Catch::Session().run (argc, argv);
}
