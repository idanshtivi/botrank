#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

struct PresetData {
    juce::String name;
    juce::String category;
    std::vector<std::pair<juce::String, float>> params;
};

class PresetManager {
public:
    PresetManager();

    int getNumFactoryPresets() const { return (int)factoryPresets.size(); }
    const PresetData& getFactoryPreset(int i) const { return factoryPresets[(size_t)i]; }

    juce::File getUserPresetFolder() const;
    void refreshUserPresets();
    int getNumUserPresets() const { return (int)userPresetFiles.size(); }
    juce::String getUserPresetName(int i) const;
    // Reads just the "category" attribute out of a user preset file, without
    // touching APVTS. Defaults to "User" if the file predates the category
    // field or the attribute is otherwise missing/blank — old presets with
    // no category are automatically treated as "User".
    juce::String getUserPresetCategory(int i) const;
    juce::File getUserPresetFile(int i) const { return userPresetFiles[(size_t)i]; }

    void applyPreset(const PresetData& p, juce::AudioProcessorValueTreeState& apvts) const;
    bool loadUserPreset(int index, juce::AudioProcessorValueTreeState& apvts);
    bool loadPresetFromFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts) const;

    // True if a user preset with this exact name already exists in this
    // exact category (the two together determine the on-disk filename, so
    // the same name in a different category is a different preset, not a
    // collision).
    bool userPresetExists(const juce::String& name, const juce::String& category) const;

    juce::File saveUserPreset(const juce::String& name, const juce::String& category,
                              juce::AudioProcessorValueTreeState& apvts);
    bool deleteUserPreset(int index);

    const juce::String& getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

    // Remembered for the lifetime of the plugin instance (not just one
    // popup open) so re-saving into the same category doesn't require
    // reselecting it every time the Save dialog is opened.
    const juce::String& getLastSaveCategory() const { return lastSaveCategory; }
    void setLastSaveCategory(const juce::String& c) { lastSaveCategory = c; }

    // Combined navigation: factory presets come first, then user presets
    int getTotalPresetCount() const { return getNumFactoryPresets() + getNumUserPresets(); }
    void loadPresetAtIndex(int combinedIndex, juce::AudioProcessorValueTreeState& apvts);
    juce::String getPresetNameAtIndex(int combinedIndex) const;

private:
    std::vector<PresetData> factoryPresets;
    std::vector<juce::File> userPresetFiles;
    juce::String currentPresetName { "Init" };
    juce::String lastSaveCategory { "User" };
    int currentPresetIndex = 0;

    void buildFactoryPresets();
    static juce::String sanitizeFilename(const juce::String& name);
    // Category folded into the filename so "Bass/My Lead" and "Pad/My Lead"
    // are different files, not a collision.
    juce::File userPresetFile(const juce::String& name, const juce::String& category) const;
    static PresetData parsePresetXml(const juce::XmlElement& xml);
    static std::unique_ptr<juce::XmlElement> presetToXml(const juce::String& name,
                                                          const juce::String& category,
                                                          juce::AudioProcessorValueTreeState& apvts);
    friend class LadderVoiceAudioProcessor;
    int currentCombinedIndex = 0;
};
