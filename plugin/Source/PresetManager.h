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
    juce::File getUserPresetFile(int i) const { return userPresetFiles[(size_t)i]; }

    void applyPreset(const PresetData& p, juce::AudioProcessorValueTreeState& apvts) const;
    bool loadUserPreset(int index, juce::AudioProcessorValueTreeState& apvts);
    bool loadPresetFromFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts) const;

    juce::File saveUserPreset(const juce::String& name, const juce::String& category,
                              juce::AudioProcessorValueTreeState& apvts);
    bool deleteUserPreset(int index);

    const juce::String& getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

    // Combined navigation: factory presets come first, then user presets
    int getTotalPresetCount() const { return getNumFactoryPresets() + getNumUserPresets(); }
    void loadPresetAtIndex(int combinedIndex, juce::AudioProcessorValueTreeState& apvts);
    juce::String getPresetNameAtIndex(int combinedIndex) const;

private:
    std::vector<PresetData> factoryPresets;
    std::vector<juce::File> userPresetFiles;
    juce::String currentPresetName { "Init" };
    int currentPresetIndex = 0;

    void buildFactoryPresets();
    static juce::String sanitizeFilename(const juce::String& name);
    static PresetData parsePresetXml(const juce::XmlElement& xml);
    static std::unique_ptr<juce::XmlElement> presetToXml(const juce::String& name,
                                                          const juce::String& category,
                                                          juce::AudioProcessorValueTreeState& apvts);
    friend class LadderVoiceAudioProcessor;
    int currentCombinedIndex = 0;
};
