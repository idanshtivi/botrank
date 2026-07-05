#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../synth_core/Include/SynthEngine.h"
#include "PresetManager.h"

class LadderVoiceAudioProcessor final : public juce::AudioProcessor {
public:
    LadderVoiceAudioProcessor();
    ~LadderVoiceAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    // Host-triggered reset (transport stop/reposition, etc.) — must clear any
    // stuck/active notes and realtime MIDI state (pitch bend, mod wheel,
    // sustain pedal) without disturbing the current patch's parameter values.
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ladder Voice"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void resetToDefaults();
    void applyInitPatchToAPVTS();

    juce::AudioProcessorValueTreeState parameters;
    PresetManager presetManager;

private:
    SynthCore::SynthEngine synth;
    double pitchBendRange = 2.0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushParametersToSynth();

#if LADDERVOICE_ENABLE_POLY_TRACE
    uint64_t _traceBlockIndex    = 0;
    uint64_t _traceSessionSample = 0;
    uint32_t _traceCrackleIndex  = 0;
    float    _tracePrevSample    = 0.0f;
    uint64_t _traceLastNoteSample = 0;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LadderVoiceAudioProcessor)
};
