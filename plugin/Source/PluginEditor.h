#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include "AnalogLookAndFeel.h"
#include "PluginProcessor.h"

class LadderVoiceAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit LadderVoiceAudioProcessorEditor(LadderVoiceAudioProcessor&);
    ~LadderVoiceAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void updatePresetNameDisplay(const juce::String& name);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    LadderVoiceAudioProcessor& processor;
    AnalogLookAndFeel analogLookAndFeel;
    juce::TextButton presetPrevButton;
    juce::TextButton presetNextButton;
    juce::Label presetNameLabel;

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::unordered_map<juce::Component*, juce::Label*> attachedLabels;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::ComboBox> playModeCombo;
    std::unique_ptr<juce::Label> playModeLabel;
    std::unique_ptr<ComboBoxAttachment> playModeAttachment;

    // Preset navigation — tracks index across factory + user combined list
    int currentPresetIndex = 0;

    void navigatePreset(int delta);
    void showPresetBrowser();

    juce::Slider& addKnob(const juce::String& text, const juce::String& parameterId);
    juce::ToggleButton& addToggle(const juce::String& text, const juce::String& parameterId);
    juce::ComboBox& addCombo(const juce::String& text, const juce::String& parameterId, const juce::StringArray& items);
    bool hasParameter(const juce::String& parameterId) const;
    void drawSection(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title);
    void positionControlLabels();
    void updatePulseWidthControlState();
    void fitPresetNameLabelFont();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LadderVoiceAudioProcessorEditor)
};
