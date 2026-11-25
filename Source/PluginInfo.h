#pragma once

// -----------------------------------------------------------------------------
// Product identity. Kept independent from the JucePlugin_* macros so the same
// sources compile in the plug-in targets and in the headless test runner.
// -----------------------------------------------------------------------------
namespace aetherfall::product
{
    inline constexpr const char* name = "AETHERFALL";
    inline constexpr const char* company = "Aetherfall Audio";

   #ifdef AETHERFALL_VERSION_STRING
    inline constexpr const char* version = AETHERFALL_VERSION_STRING;
   #else
    inline constexpr const char* version = "0.0.0";
   #endif

    /** Version of the saved-state format. Bump when the state layout changes
        in a way that needs migration code in PluginProcessor::setStateInformation. */
    inline constexpr int stateVersion = 1;
}
