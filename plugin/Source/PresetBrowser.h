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

    // Mouse events for hover tracking
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    LadderVoiceAudioProcessor& processor;
    std::function<void(const juce::String&)> onPresetLoaded;

    juce::ListBox listBox;
    juce::TextButton loadBtn  {"Load"};
    juce::TextButton saveBtn  {"Save As..."};
    juce::TextButton deleteBtn{"Delete"};

    // Rebuilt each time the browser opens / user saves. isHeader marks a
    // collapsible section row (category or "USER PRESETS"); its children are
    // simply omitted from `entries` while collapsed, so getNumRows() shrinks
    // and grows as sections open/close.
    struct Entry { juce::String name; bool isUser; int sourceIndex; bool isHeader = false; };
    std::vector<Entry> entries;

    // Collapsed/expanded state per section header name, preserved across
    // rebuilds (Save/Delete) so opening/closing a library sticks until the
    // user changes it again. Populated with sensible defaults the first time
    // rebuildEntries() runs.
    std::map<juce::String, bool> collapsed;
    bool collapsedInitialised = false;

    int hoveredRow = -1; // for subtle hover highlight

    void rebuildEntries();
    void toggleSection(const juce::String& header);
    void loadSelected();
    void saveAs();
    void deleteSelected();
    void updateDeleteState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
