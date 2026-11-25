#pragma once

#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

namespace aetherfall
{

class AetherfallProcessor;
class PresetManager;

/** Overlay preset browser: categories, favourites, search, save / delete. */
class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (AetherfallProcessor&);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    std::function<void()> onClose;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;

    void setFilter (int categoryFilter);
    void saveAs();

    AetherfallProcessor& processor;
    PresetManager& manager;

    juce::TextEditor search;
    juce::TextButton saveButton { "Save As..." }, deleteButton { "Delete" }, closeButton { "Close" };
    std::vector<std::unique_ptr<juce::TextButton>> categoryButtons;
    juce::ListBox list { "presets", this };

    std::vector<int> visible;
    int categoryFilter = -1;
};

} // namespace aetherfall
