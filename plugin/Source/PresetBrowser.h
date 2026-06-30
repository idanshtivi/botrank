#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetManager.h"

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

    // Rebuilt each time the browser opens / user saves
    struct Entry { juce::String name; bool isUser; int sourceIndex; };
    std::vector<Entry> entries;

    int hoveredRow = -1; // for subtle hover highlight

    void rebuildEntries();
    void loadSelected();
    void saveAs();
    void deleteSelected();
    void updateDeleteState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
