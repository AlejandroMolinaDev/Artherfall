#include "PresetBrowser.h"
#include "PluginProcessor.h"
#include "Presets/PresetManager.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"

namespace aetherfall
{

PresetBrowser::PresetBrowser (AetherfallProcessor& p)
    : processor (p), manager (p.getPresetManager())
{
    search.setTextToShowWhenEmpty ("Search presets...", palette::textDim);
    search.setColour (juce::TextEditor::backgroundColourId, palette::panelRaised);
    search.setColour (juce::TextEditor::outlineColourId, palette::outline);
    search.setColour (juce::TextEditor::textColourId, palette::text);
    search.onTextChange = [this] { refresh(); };
    search.setJustification (juce::Justification::centredLeft);
    search.setIndents (10, 0);
    addAndMakeVisible (search);

    // Filters: All, Favourites, User, then the factory categories.
    const std::pair<juce::String, int> filters[] = { { "All", -1 }, { juce::CharPointer_UTF8 ("\xe2\x98\x85 Favourites"), -2 }, { "User", -3 } };
    for (const auto& [label, f] : filters)
    {
        categoryButtons.push_back (std::make_unique<juce::TextButton> (label));
        categoryButtons.back()->onClick = [this, f2 = f] { setFilter (f2); };
    }
    for (int c = 0; c < numFactoryCategories; ++c)
    {
        categoryButtons.push_back (std::make_unique<juce::TextButton> (categoryName ((PresetCategory) c)));
        categoryButtons.back()->onClick = [this, c] { setFilter (c); };
    }
    for (auto& b : categoryButtons)
        addAndMakeVisible (*b);

    saveButton.onClick = [this] { saveAs(); };
    deleteButton.onClick = [this]
    {
        const int row = list.getSelectedRow();
        if (row >= 0 && row < (int) visible.size() && manager.deleteUserPreset (visible[(size_t) row]))
            refresh();
    };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    for (auto* b : { &saveButton, &deleteButton, &closeButton })
        addAndMakeVisible (*b);

    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, palette::panel);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);

    manager.rescan();
    setFilter (-1);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::setFilter (int f)
{
    categoryFilter = f;
    const int filters[] = { -1, -2, -3 };
    for (size_t i = 0; i < categoryButtons.size(); ++i)
    {
        const int value = i < 3 ? filters[i] : (int) i - 3;
        categoryButtons[i]->setToggleState (value == f, juce::dontSendNotification);
    }
    refresh();
}

void PresetBrowser::refresh()
{
    visible = manager.filter (search.getText(), categoryFilter);
    list.updateContent();
    const int current = manager.getCurrentIndex();
    for (size_t i = 0; i < visible.size(); ++i)
        if (visible[i] == current)
            list.selectRow ((int) i, true, true);
    repaint();
}

int PresetBrowser::getNumRows() { return (int) visible.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) visible.size())
        return;
    const auto& e = manager.getEntries()[(size_t) visible[(size_t) row]];
    const auto accent = categoryAccent (e.category);

    if (selected)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.fillRect (0, 0, width, height);
    }
    g.setColour (accent);
    g.fillEllipse (10.0f, (float) height * 0.5f - 3.0f, 6.0f, 6.0f);

    g.setFont (AetherLookAndFeel::font (14.0f, selected));
    g.setColour (palette::text);
    g.drawText (e.name, 26, 0, width / 2, height, juce::Justification::centredLeft);

    g.setFont (AetherLookAndFeel::font (12.0f));
    g.setColour (palette::textDim);
    g.drawText (juce::String (categoryName (e.category)) + (e.factory ? "" : "  (user)"), width / 2, 0, width / 2 - 44,
                height, juce::Justification::centredRight);

    g.setColour (manager.isFavourite (e) ? palette::warm : palette::textDim.withAlpha (0.35f));
    g.setFont (AetherLookAndFeel::font (16.0f));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x98\x85"), width - 36, 0, 28, height, juce::Justification::centred);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) visible.size())
        return;
    // Clicking the star toggles the favourite.
    if (e.x > list.getWidth() - 44)
    {
        manager.toggleFavourite (manager.getEntries()[(size_t) visible[(size_t) row]]);
        refresh();
        return;
    }
    manager.load (visible[(size_t) row]);
    repaint();
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) visible.size())
    {
        manager.load (visible[(size_t) row]);
        if (onClose) onClose();
    }
}

void PresetBrowser::returnKeyPressed (int row)
{
    if (row >= 0 && row < (int) visible.size())
        manager.load (visible[(size_t) row]);
}

void PresetBrowser::saveAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name and category of the new user preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", processor.getPresetName(), "Name");
    juce::StringArray cats;
    for (int c = 0; c < (int) PresetCategory::Count; ++c)
        cats.add (categoryName ((PresetCategory) c));
    w->addComboBox ("category", cats, "Category");
    if (auto* box = w->getComboBoxComponent ("category"))
        box->setSelectedItemIndex ((int) processor.getPresetCategory(), juce::dontSendNotification);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBrowser> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = w->getTextEditorContents ("name");
        const int cat = w->getComboBoxComponent ("category") != nullptr ? w->getComboBoxComponent ("category")->getSelectedItemIndex() : 9;
        if (safe->manager.saveUserPreset (name, (PresetCategory) juce::jmax (0, cat)))
            safe->refresh();
    }), true);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (palette::background.withAlpha (0.92f));
    const auto r = getLocalBounds().reduced (12).toFloat();
    g.setColour (palette::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (24);
    auto top = r.removeFromTop (32);
    closeButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (8);
    deleteButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (8);
    saveButton.setBounds (top.removeFromRight (100));
    top.removeFromRight (16);
    search.setBounds (top);
    r.removeFromTop (12);

    auto left = r.removeFromLeft (180);
    for (auto& b : categoryButtons)
    {
        b->setBounds (left.removeFromTop (30).reduced (0, 2));
        left.removeFromTop (2);
    }
    r.removeFromLeft (12);
    list.setBounds (r);
}

} // namespace aetherfall
