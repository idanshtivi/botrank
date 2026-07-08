#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetManager.h"
#include <map>

// Forward declaration
class LadderVoiceAudioProcessor;

// Popup preset browser launched from the preset name label.
// Shown via juce::CallOutBox::launchAsynchronously.
class PresetBrowser final : public juce::Component,
                            public juce::ListBoxModel
{
public:
    // onPresetLoaded is called on the message thread after a preset is applied.
    PresetBrowser(LadderVoiceAudioProcessor& proc,
                  std::function<void(const juce::String& name)> onPresetLoaded);
    ~PresetBrowser() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;

    // Mouse events for hover tracking + category sidebar clicks
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    // Ctrl+F focuses the search field (bubbles up from whichever child
    // currently has keyboard focus, e.g. the list box, if unhandled there).
    bool keyPressed(const juce::KeyPress&) override;

private:
    LadderVoiceAudioProcessor& processor;
    std::function<void(const juce::String&)> onPresetLoaded;

    juce::ListBox listBox;
    juce::TextEditor searchBox;
    juce::TextButton loadBtn  {"Load"};
    juce::TextButton saveBtn  {"Save As..."};
    juce::TextButton deleteBtn{"Delete"};

    // Rebuilt each time the browser opens / user saves. isHeader marks a
    // collapsible section row (used only in the "All" category, to group
    // factory presets by sound type); its children are simply omitted from
    // `entries` while collapsed, so getNumRows() shrinks and grows as
    // sections open/close.
    struct Entry { juce::String name; bool isUser; int sourceIndex; bool isHeader = false; };
    std::vector<Entry> entries;

    // Collapsed/expanded state per section header name (only used while
    // browsing "All"), preserved across rebuilds so opening/closing a
    // section sticks until the user changes it again.
    std::map<juce::String, bool> collapsed;
    bool collapsedInitialised = false;

    // Category sidebar (like Arturia/u-he/Serum): "All" plus every distinct
    // sound-type category actually present in the factory bank (discovered
    // from PresetData::category, not a separate hardcoded taxonomy), plus
    // "User" if any user presets exist. Selecting one filters the list on
    // the right to just that category.
    juce::StringArray categoryNames;
    juce::String selectedCategory { "All" };
    std::vector<juce::Rectangle<int>> categoryTabRects;
    int hoveredCategoryIndex = -1;
    // Index into categoryNames/categoryTabRects of the first "LAB - ..."
    // session-folder tab, or -1 if none are present. Used to draw an extra
    // gap + divider line separating the shipped Factory taxonomy from
    // ad-hoc LAB preset-development folders.
    int firstLabTabIndex = -1;

    int hoveredRow = -1; // for subtle hover highlight

    // While non-empty, search matches against every factory + user preset's
    // name or category (case-insensitive substring), ignoring the currently
    // selected category tab — it's a global search, same as clicking "All"
    // would show, just filtered further. Clearing it restores whatever
    // category was selected before.
    bool isSearching() const { return searchBox.getText().isNotEmpty(); }
    bool matchesSearch(const juce::String& name, const juce::String& category) const;

    void rebuildCategoryTabs();
    void rebuildEntries();
    void toggleSection(const juce::String& header);
    void loadSelected();
    void saveAs();
    void deleteSelected();
    void updateDeleteState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
