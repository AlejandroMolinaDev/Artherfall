#pragma once

#include <set>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>
#include "FactoryPresets.h"

namespace aetherfall
{

class AetherfallProcessor;

/**
    Preset browser model (message thread).

    - Factory presets come from FactoryPresets (data, read-only, no disk I/O).
    - User presets are XML files (*.afpreset) in Documents/AETHERFALL/Presets:
        <AETHERFALL_PRESET name=".." category=".." version="1"> <AETHERFALL ...full state...> </...>
    - Favourites are stored in favourites.xml in the same folder.
    - The loaded preset (name + category) is kept in the plug-in state (PRESET
      node), so it is restored with the DAW project.
*/
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        PresetCategory category = PresetCategory::User;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;

        juce::String key() const { return factory ? "factory:" + name : "user:" + file.getFileName(); }
    };

    explicit PresetManager (AetherfallProcessor&);

    static juce::File getDefaultUserFolder();
    void setUserFolder (const juce::File& folder);   // tests / custom location
    juce::File getUserFolder() const { return userFolder; }

    void rescan();
    const std::vector<Entry>& getEntries();

    bool load (int index);
    bool loadNext();
    bool loadPrevious();

    /** Index of the current preset in getEntries() (-1 = not found / modified). */
    int getCurrentIndex();

    bool saveUserPreset (const juce::String& name, PresetCategory category);
    bool deleteUserPreset (int index);

    bool isFavourite (const Entry&) const;
    void toggleFavourite (const Entry&);

    /** Filter: -1 = all, -2 = favourites, -3 = user, otherwise a category index. */
    std::vector<int> filter (const juce::String& search, int categoryFilter);

    /** Builds the complete state of a factory preset from the default state. */
    juce::ValueTree buildFactoryState (const FactoryPreset&) const;

    static constexpr const char* fileExtension = ".afpreset";

private:
    void loadFavourites();
    void saveFavourites() const;

    AetherfallProcessor& processor;
    juce::File userFolder;
    std::vector<Entry> entries;
    std::set<juce::String> favourites;
    bool scanned = false;
};

} // namespace aetherfall
