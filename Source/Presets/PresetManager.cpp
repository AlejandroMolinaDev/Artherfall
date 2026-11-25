#include "PresetManager.h"
#include "PluginProcessor.h"
#include "DSP/Chain/ChainOrder.h"
#include "Parameters/ModRoutingState.h"

namespace aetherfall
{

namespace
{
    const juce::Identifier presetRootTag { "AETHERFALL_PRESET" };
    const juce::Identifier uiNode { "UI" };
    constexpr int presetFormatVersion = 1;

    /** Sets the stored (denormalised) value of a parameter in an APVTS state tree. */
    void setParamValue (juce::ValueTree& state, PID pid, float value)
    {
        const juce::String id (std::string (info (pid).id));
        for (auto child : state)
            if (child.hasType ("PARAM") && child.getProperty ("id").toString() == id)
            {
                const auto& inf = info (pid);
                child.setProperty ("value", juce::jlimit (inf.rangeMin(), inf.rangeMax(), value), nullptr);
                return;
            }
    }
}

PresetManager::PresetManager (AetherfallProcessor& p)
    : processor (p), userFolder (getDefaultUserFolder())
{
}

juce::File PresetManager::getDefaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("AETHERFALL").getChildFile ("Presets");
}

void PresetManager::setUserFolder (const juce::File& folder)
{
    userFolder = folder;
    scanned = false;
}

void PresetManager::rescan()
{
    entries.clear();
    const auto& bank = factoryPresets();
    for (size_t i = 0; i < bank.size(); ++i)
        entries.push_back ({ bank[i].name, bank[i].category, true, (int) i, {} });

    if (userFolder.isDirectory())
    {
        auto files = userFolder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + fileExtension);
        files.sort();
        for (const auto& f : files)
        {
            juce::String category = "User";
            if (auto xml = juce::XmlDocument::parse (f))
                if (xml->hasTagName (presetRootTag.toString()))
                    category = xml->getStringAttribute ("category", "User");
            entries.push_back ({ f.getFileNameWithoutExtension(), categoryFromName (category), false, -1, f });
        }
    }

    loadFavourites();
    scanned = true;
}

const std::vector<PresetManager::Entry>& PresetManager::getEntries()
{
    if (! scanned)
        rescan();
    return entries;
}

juce::ValueTree PresetManager::buildFactoryState (const FactoryPreset& preset) const
{
    auto state = processor.getValueTreeState().copyState();

    // Every parameter back to its default...
    for (const auto& p : paramInfos)
        setParamValue (state, p.pid, p.defaultValue);

    // ...and every state node back to its default content.
    for (const auto* id : { &AetherfallProcessor::chainNodeId, &AetherfallProcessor::gateNodeId, &modstate::matrixNode,
                            &modstate::macrosNode, &modstate::seqNode, &modstate::lfoShapesNode })
        state.removeChild (state.getChildWithName (*id), nullptr);
    modstate::ensureNodes (state);

    for (const auto& [pid, value] : preset.values)
        setParamValue (state, pid, value);

    auto order = chain::emptyOrder();
    for (size_t i = 0; i < preset.chain.size() && i < order.size(); ++i)
        order[i] = preset.chain[i];
    juce::ValueTree chainNode (AetherfallProcessor::chainNodeId);
    chainNode.setProperty (AetherfallProcessor::chainOrderProp, juce::String (chain::toString (chain::sanitize (order))), nullptr);
    state.appendChild (chainNode, nullptr);

    if (preset.gatePattern != 0)
    {
        juce::ValueTree gate (AetherfallProcessor::gateNodeId);
        gate.setProperty (AetherfallProcessor::gatePatternProp, (int) preset.gatePattern, nullptr);
        state.appendChild (gate, nullptr);
    }

    for (size_t s = 0; s < preset.mods.size() && s < (size_t) numModSlots; ++s)
    {
        const auto& m = preset.mods[s];
        modstate::writeSlot (state, (int) s, { m.source, (std::int16_t) m.dest, m.curve, ModSource::None });
        setParamValue (state, modAmountParam ((int) s), m.amount);
    }

    return state;
}

bool PresetManager::load (int index)
{
    const auto& list = getEntries();
    if (index < 0 || index >= (int) list.size())
        return false;

    const auto& e = list[(size_t) index];
    juce::ValueTree state;

    if (e.factory)
    {
        state = buildFactoryState (factoryPresets()[(size_t) e.factoryIndex]);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr || ! xml->hasTagName (presetRootTag.toString()))
            return false;
        auto* stateXml = xml->getChildByName (processor.getValueTreeState().state.getType().toString());
        if (stateXml == nullptr)
            return false;
        state = juce::ValueTree::fromXml (*stateXml);
        // Keep the current window size / tab.
        state.removeChild (state.getChildWithName (uiNode), nullptr);
        if (auto ui = processor.getValueTreeState().state.getChildWithName (uiNode); ui.isValid())
            state.appendChild (ui.createCopy(), nullptr);
    }

    processor.loadPresetState (state, e.name, e.category);
    return true;
}

int PresetManager::getCurrentIndex()
{
    const auto name = processor.getPresetName();
    const auto cat = processor.getPresetCategory();
    const auto& list = getEntries();
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name == name && list[i].category == cat)
            return (int) i;
    return -1;
}

bool PresetManager::loadNext()
{
    const int n = (int) getEntries().size();
    return n > 0 && load ((getCurrentIndex() + 1 + n) % n);
}

bool PresetManager::loadPrevious()
{
    const int n = (int) getEntries().size();
    const int cur = getCurrentIndex();
    return n > 0 && load (((cur < 0 ? 0 : cur) - 1 + n) % n);
}

bool PresetManager::saveUserPreset (const juce::String& rawName, PresetCategory category)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty() || ! userFolder.createDirectory())
        return false;

    auto state = processor.getValueTreeState().copyState();
    state.removeChild (state.getChildWithName (uiNode), nullptr);
    if (auto xmlState = state.createXml())
    {
        juce::XmlElement root (presetRootTag.toString());
        root.setAttribute ("name", name);
        root.setAttribute ("category", categoryName (category));
        root.setAttribute ("version", presetFormatVersion);
        root.addChildElement (xmlState.release());

        const auto file = userFolder.getChildFile (name + fileExtension);
        if (! root.writeTo (file))
            return false;

        processor.setPresetInfo (name, category);
        rescan();
        return true;
    }
    return false;
}

bool PresetManager::deleteUserPreset (int index)
{
    const auto& list = getEntries();
    if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory)
        return false;
    const bool ok = list[(size_t) index].file.deleteFile();
    rescan();
    return ok;
}

bool PresetManager::isFavourite (const Entry& e) const { return favourites.count (e.key()) > 0; }

void PresetManager::toggleFavourite (const Entry& e)
{
    if (! favourites.erase (e.key()))
        favourites.insert (e.key());
    saveFavourites();
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    if (auto xml = juce::XmlDocument::parse (userFolder.getChildFile ("favourites.xml")))
        for (auto* f : xml->getChildIterator())
            favourites.insert (f->getStringAttribute ("key"));
}

void PresetManager::saveFavourites() const
{
    if (! userFolder.createDirectory())
        return;
    juce::XmlElement root ("FAVOURITES");
    for (const auto& k : favourites)
        root.createNewChildElement ("F")->setAttribute ("key", k);
    root.writeTo (userFolder.getChildFile ("favourites.xml"));
}

std::vector<int> PresetManager::filter (const juce::String& search, int categoryFilter)
{
    std::vector<int> out;
    const auto& list = getEntries();
    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& e = list[i];
        if (categoryFilter == -2 && ! isFavourite (e)) continue;
        if (categoryFilter == -3 && e.factory) continue;
        if (categoryFilter >= 0 && (int) e.category != categoryFilter) continue;
        if (search.isNotEmpty() && ! e.name.containsIgnoreCase (search)
            && ! juce::String (categoryName (e.category)).containsIgnoreCase (search))
            continue;
        out.push_back ((int) i);
    }
    return out;
}

} // namespace aetherfall
