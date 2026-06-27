#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
float value(juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (auto* parameter = state.getRawParameterValue(id))
        return parameter->load();

    DBG("LadderVoice processor missing parameter: " << id);
    return 0.0f;
}

juce::String fixedText(float valueToFormat, int decimals, const char* suffix = "")
{
    return juce::String(valueToFormat, decimals) + suffix;
}

juce::String timeText(float seconds)
{
    if (seconds < 1.0f)
        return juce::String(juce::roundToInt(seconds * 1000.0f)) + " ms";
    return juce::String(seconds, 2) + " s";
}

float def(SynthCore::ParamId id)
{
    return SynthCore::initPatchValue(id);
}
}

LadderVoiceAudioProcessor::LadderVoiceAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LadderVoiceAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto boolParam = [&params](const char* id, const char* name, bool def) {
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, def));
    };
    auto floatParam = [&params](const char* id, const char* name, juce::NormalisableRange<float> range, float def,
                                juce::AudioParameterFloatAttributes attributes = {}) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, range, def, attributes));
    };
    auto choiceParam = [&params](const char* id, const char* name, juce::StringArray choices, int def) {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, choices, def));
    };

    choiceParam("playMode", "Play Mode", {"MONO", "POLY 4"}, juce::roundToInt(def(SynthCore::ParamId::PlayMode)));
    boolParam("osc1Enabled", "Osc 1 Enabled", def(SynthCore::ParamId::Osc1Enabled) >= 0.5f);
    boolParam("osc2Enabled", "Osc 2 Enabled", def(SynthCore::ParamId::Osc2Enabled) >= 0.5f);
    boolParam("osc3Enabled", "Osc 3 Enabled", def(SynthCore::ParamId::Osc3Enabled) >= 0.5f);
    auto twoDecimals = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
        [](float v, int) { return fixedText(v, 2); });

    auto detuneText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) {
            return (v > 0.0f ? juce::String("+") : juce::String()) + juce::String(v, 2) + " st";
        })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto s = t.trim().endsWithIgnoreCase("st")
                ? t.trim().dropLastCharacters(2).trim() : t.trim();
            return (float)s.getDoubleValue();
        });

    auto bendText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + " st"; })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto s = t.trim().endsWithIgnoreCase("st")
                ? t.trim().dropLastCharacters(2).trim() : t.trim();
            return (float)s.getDoubleValue();
        });

    auto timeAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return timeText(v); })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto text = t.trim();
            if (text.endsWithIgnoreCase("ms"))
                return (float)(text.dropLastCharacters(2).trim().getDoubleValue() / 1000.0);
            if (text.endsWithIgnoreCase("s"))
                return (float)text.dropLastCharacters(1).trim().getDoubleValue();
            const double v = text.getDoubleValue();
            return (float)(v > 10.0 ? v / 1000.0 : v);
        });

    auto hzText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) {
            return juce::String(juce::roundToInt(v)) + " Hz";
        })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto text = t.trim();
            return (float)(text.endsWithIgnoreCase("hz")
                ? text.dropLastCharacters(2).trim().getDoubleValue()
                : text.getDoubleValue());
        });

    juce::NormalisableRange<float> unitRange(0.0f, 1.0f, 0.01f);
    juce::NormalisableRange<float> driveRange(0.0f, 3.0f, 0.01f);
    juce::NormalisableRange<float> attackRange(0.001f, 10.0f);
    attackRange.setSkewForCentre(0.05f);
    attackRange.interval = 0.001f;
    juce::NormalisableRange<float> decayRange(0.005f, 10.0f);
    decayRange.setSkewForCentre(0.35f);
    decayRange.interval = 0.001f;
    juce::NormalisableRange<float> releaseRange(0.005f, 10.0f);
    releaseRange.setSkewForCentre(0.30f);
    releaseRange.interval = 0.001f;
    juce::NormalisableRange<float> glideRange(0.0f, 5.0f);
    glideRange.setSkewForCentre(0.25f);
    glideRange.interval = 0.001f;

    floatParam("osc1Level", "Osc 1 Level", unitRange, def(SynthCore::ParamId::Osc1Level), twoDecimals);
    floatParam("osc2Level", "Osc 2 Level", unitRange, def(SynthCore::ParamId::Osc2Level), twoDecimals);
    floatParam("osc3Level", "Osc 3 Level", unitRange, def(SynthCore::ParamId::Osc3Level), twoDecimals);
    choiceParam("osc1Waveform", "Osc 1 Waveform", {"Tri", "Tri-Saw", "Saw", "Rev Saw", "Square", "Wide", "Narrow"}, juce::roundToInt(def(SynthCore::ParamId::Osc1Waveform)));
    choiceParam("osc2Waveform", "Osc 2 Waveform", {"Tri", "Tri-Saw", "Saw", "Rev Saw", "Square", "Wide", "Narrow"}, juce::roundToInt(def(SynthCore::ParamId::Osc2Waveform)));
    choiceParam("osc3Waveform", "Osc 3 Waveform", {"Tri", "Tri-Saw", "Saw", "Rev Saw", "Square", "Wide", "Narrow"}, juce::roundToInt(def(SynthCore::ParamId::Osc3Waveform)));
    choiceParam("osc1Range", "Osc 1 Range", {"LO", "32'", "16'", "8'", "4'", "2'"}, juce::roundToInt(def(SynthCore::ParamId::Osc1Range)));
    choiceParam("osc2Range", "Osc 2 Range", {"LO", "32'", "16'", "8'", "4'", "2'"}, juce::roundToInt(def(SynthCore::ParamId::Osc2Range)));
    choiceParam("osc3Range", "Osc 3 Range", {"LO", "32'", "16'", "8'", "4'", "2'"}, juce::roundToInt(def(SynthCore::ParamId::Osc3Range)));
    floatParam("osc2Detune", "Osc 2 Detune", juce::NormalisableRange<float>(-7.0f, 7.0f, 0.01f), def(SynthCore::ParamId::Osc2Detune), detuneText);
    floatParam("osc3Detune", "Osc 3 Detune", juce::NormalisableRange<float>(-7.0f, 7.0f, 0.01f), def(SynthCore::ParamId::Osc3Detune), detuneText);
    boolParam("osc3KeyboardTracking", "Osc 3 Keyboard Control", def(SynthCore::ParamId::Osc3KeyboardTracking) >= 0.5f);

    floatParam("mixerDrive", "Mixer Drive", driveRange, def(SynthCore::ParamId::MixerDrive), twoDecimals);
    floatParam("loudnessAttack", "Loudness Attack", attackRange, def(SynthCore::ParamId::AmpAttack), timeAttr);
    floatParam("loudnessDecay", "Loudness Decay", decayRange, def(SynthCore::ParamId::AmpDecay), timeAttr);
    floatParam("loudnessSustain", "Loudness Sustain", unitRange, def(SynthCore::ParamId::AmpSustain), twoDecimals);
    floatParam("loudnessRelease", "Loudness Release", releaseRange, def(SynthCore::ParamId::AmpRelease), timeAttr);
    floatParam("masterVolume", "Master Volume", unitRange, def(SynthCore::ParamId::MasterVolume), twoDecimals);
    floatParam("outputDrive", "Output Drive", driveRange, 0.0f, twoDecimals);
    boolParam("glideEnabled", "Glide Enabled", def(SynthCore::ParamId::GlideEnabled) >= 0.5f);
    floatParam("glideTime", "Glide Time", glideRange, def(SynthCore::ParamId::GlideTime), timeAttr);
    floatParam("pitchBendRange", "Pitch Bend Range", juce::NormalisableRange<float>(0.0f, 12.0f, 0.01f), def(SynthCore::ParamId::PitchBendRange), bendText);

    juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f);
    cutoffRange.setSkewForCentre(1000.0f);
    floatParam("filterCutoff", "Filter Cutoff", cutoffRange, def(SynthCore::ParamId::FilterCutoff), hzText);
    floatParam("filterResonance", "Filter Emphasis", unitRange, def(SynthCore::ParamId::FilterResonance), twoDecimals);
    floatParam("filterContour", "Filter Contour", unitRange, def(SynthCore::ParamId::FilterEnvAmount), twoDecimals);
    floatParam("filterAttack", "Filter Attack", attackRange, def(SynthCore::ParamId::FilterAttack), timeAttr);
    floatParam("filterDecay", "Filter Decay", decayRange, def(SynthCore::ParamId::FilterDecay), timeAttr);
    floatParam("filterSustain", "Filter Sustain", unitRange, def(SynthCore::ParamId::FilterSustain), twoDecimals);
    floatParam("filterRelease", "Filter Release", releaseRange, def(SynthCore::ParamId::FilterRelease), timeAttr);

    boolParam("noiseEnabled", "Noise Enabled", def(SynthCore::ParamId::NoiseEnabled) >= 0.5f);
    floatParam("noiseLevel", "Noise Level", unitRange, def(SynthCore::ParamId::NoiseLevel), twoDecimals);
    choiceParam("noiseMode", "Noise Mode", {"White", "Pink"}, juce::roundToInt(def(SynthCore::ParamId::NoiseMode)));
    boolParam("lfoEnabled", "LFO Enabled", def(SynthCore::ParamId::LfoEnabled) >= 0.5f);

    juce::NormalisableRange<float> lfoRateRange(0.01f, 20.0f);
    lfoRateRange.setSkewForCentre(2.0f);
    lfoRateRange.interval = 0.01f;
    auto lfoHzAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + " Hz"; })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto s = t.trim();
            return (float)(s.endsWithIgnoreCase("hz") ? s.dropLastCharacters(2).trim().getDoubleValue() : s.getDoubleValue());
        });
    floatParam("lfoRate", "LFO Rate", lfoRateRange, def(SynthCore::ParamId::LfoRate), lfoHzAttr);
    floatParam("lfoAmount", "LFO Amount", unitRange, def(SynthCore::ParamId::LfoAmount), twoDecimals);
    choiceParam("lfoDestination", "LFO Destination", {"Pitch", "Filter", "Pulse Width"}, juce::roundToInt(def(SynthCore::ParamId::LfoDestination)));
    floatParam("modWheelAmount", "Mod Wheel Amt", unitRange, def(SynthCore::ParamId::ModWheelAmount), twoDecimals);

    floatParam("filterDrive", "Filter Drive", driveRange, def(SynthCore::ParamId::FilterDrive), twoDecimals);
    choiceParam("filterKeyboardTracking", "Filter KB Tracking", {"Off", "Half", "Full"}, juce::roundToInt(def(SynthCore::ParamId::FilterKeyboardTracking) * 2.0f));
    boolParam("legato",   "Legato",   def(SynthCore::ParamId::Legato) >= 0.5f);
    boolParam("retrigger","Retrigger", def(SynthCore::ParamId::Retrigger) >= 0.5f);
    choiceParam("notePriority", "Note Priority", {"Low", "Last", "High"}, juce::roundToInt(def(SynthCore::ParamId::NotePriority)));
    floatParam("osc1PulseWidth", "Osc 1 Pulse Width",
               juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), def(SynthCore::ParamId::Osc1PulseWidth), twoDecimals);
    floatParam("analogDrift", "Analog Drift", unitRange, def(SynthCore::ParamId::AnalogDrift), twoDecimals);

    auto centsAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) {
            const int c = juce::roundToInt(v);
            return (c > 0 ? juce::String("+") : juce::String()) + juce::String(c) + " c";
        })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto s = t.trim().endsWithIgnoreCase("c") ? t.trim().dropLastCharacters(1).trim() : t.trim();
            return (float)s.getDoubleValue();
        });
    floatParam("fineTune", "Fine Tune", juce::NormalisableRange<float>(-50.0f, 50.0f, 1.0f), def(SynthCore::ParamId::FineTune), centsAttr);

    return {params.begin(), params.end()};
}

void LadderVoiceAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.prepare(sampleRate, samplesPerBlock);
    pushParametersToSynth();
}

void LadderVoiceAudioProcessor::releaseResources()
{
}

bool LadderVoiceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LadderVoiceAudioProcessor::pushParametersToSynth()
{
    synth.setParameter(SynthCore::ParamId::Osc1Enabled, value(parameters, "osc1Enabled"));
    synth.setPlayMode(juce::roundToInt(value(parameters, "playMode")));
    synth.setParameter(SynthCore::ParamId::Osc2Enabled, value(parameters, "osc2Enabled"));
    synth.setParameter(SynthCore::ParamId::Osc3Enabled, value(parameters, "osc3Enabled"));
    synth.setParameter(SynthCore::ParamId::Osc1Level, value(parameters, "osc1Level"));
    synth.setParameter(SynthCore::ParamId::Osc2Level, value(parameters, "osc2Level"));
    synth.setParameter(SynthCore::ParamId::Osc3Level, value(parameters, "osc3Level"));
    synth.setParameter(SynthCore::ParamId::Osc1Waveform, value(parameters, "osc1Waveform"));
    synth.setParameter(SynthCore::ParamId::Osc2Waveform, value(parameters, "osc2Waveform"));
    synth.setParameter(SynthCore::ParamId::Osc3Waveform, value(parameters, "osc3Waveform"));
    synth.setParameter(SynthCore::ParamId::Osc1Range, value(parameters, "osc1Range"));
    synth.setParameter(SynthCore::ParamId::Osc2Range, value(parameters, "osc2Range"));
    synth.setParameter(SynthCore::ParamId::Osc3Range, value(parameters, "osc3Range"));
    synth.setParameter(SynthCore::ParamId::Osc2Detune, value(parameters, "osc2Detune"));
    synth.setParameter(SynthCore::ParamId::Osc3Detune, value(parameters, "osc3Detune"));
    synth.setParameter(SynthCore::ParamId::Osc3KeyboardTracking, value(parameters, "osc3KeyboardTracking"));
    synth.setParameter(SynthCore::ParamId::MixerDrive, value(parameters, "mixerDrive"));
    synth.setParameter(SynthCore::ParamId::AmpAttack, value(parameters, "loudnessAttack"));
    synth.setParameter(SynthCore::ParamId::AmpDecay, value(parameters, "loudnessDecay"));
    synth.setParameter(SynthCore::ParamId::AmpSustain, value(parameters, "loudnessSustain"));
    synth.setParameter(SynthCore::ParamId::AmpRelease, value(parameters, "loudnessRelease"));
    synth.setParameter(SynthCore::ParamId::MasterVolume, value(parameters, "masterVolume"));
    synth.setOutputDrive(value(parameters, "outputDrive"));
    synth.setParameter(SynthCore::ParamId::GlideEnabled, value(parameters, "glideEnabled"));
    synth.setParameter(SynthCore::ParamId::GlideTime, value(parameters, "glideTime"));
    pitchBendRange = value(parameters, "pitchBendRange");
    synth.setParameter(SynthCore::ParamId::PitchBendRange, static_cast<float>(pitchBendRange));
    synth.setParameter(SynthCore::ParamId::FilterCutoff, value(parameters, "filterCutoff"));
    synth.setParameter(SynthCore::ParamId::FilterResonance, value(parameters, "filterResonance"));
    synth.setParameter(SynthCore::ParamId::FilterEnvAmount, value(parameters, "filterContour"));
    synth.setParameter(SynthCore::ParamId::FilterAttack, value(parameters, "filterAttack"));
    synth.setParameter(SynthCore::ParamId::FilterDecay, value(parameters, "filterDecay"));
    synth.setParameter(SynthCore::ParamId::FilterSustain, value(parameters, "filterSustain"));
    synth.setParameter(SynthCore::ParamId::FilterRelease, value(parameters, "filterRelease"));
    const auto noiseLevel = value(parameters, "noiseLevel");
    synth.setParameter(SynthCore::ParamId::NoiseEnabled, noiseLevel > 0.0001f ? 1.0f : 0.0f);
    synth.setParameter(SynthCore::ParamId::NoiseLevel,   noiseLevel);
    synth.setParameter(SynthCore::ParamId::NoiseMode,    value(parameters, "noiseMode"));
    synth.setParameter(SynthCore::ParamId::FilterDrive,  value(parameters, "filterDrive"));
    // KB tracking choice: 0=Off(0.0), 1=Half(0.5), 2=Full(1.0)
    synth.setParameter(SynthCore::ParamId::FilterKeyboardTracking,
                       value(parameters, "filterKeyboardTracking") * 0.5f);
    synth.setParameter(SynthCore::ParamId::Legato,    value(parameters, "legato"));
    synth.setParameter(SynthCore::ParamId::Retrigger, value(parameters, "retrigger"));
    synth.setParameter(SynthCore::ParamId::NotePriority,   value(parameters, "notePriority"));
    synth.setParameter(SynthCore::ParamId::Osc1PulseWidth, value(parameters, "osc1PulseWidth"));
    synth.setParameter(SynthCore::ParamId::AnalogDrift,    value(parameters, "analogDrift"));
    const auto lfoAmount = value(parameters, "lfoAmount");
    const auto modWheelAmount = value(parameters, "modWheelAmount");
    synth.setParameter(SynthCore::ParamId::LfoEnabled,     (lfoAmount > 0.0001f || modWheelAmount > 0.0001f) ? 1.0f : 0.0f);
    synth.setParameter(SynthCore::ParamId::LfoRate,        value(parameters, "lfoRate"));
    synth.setParameter(SynthCore::ParamId::LfoAmount,      lfoAmount);
    synth.setParameter(SynthCore::ParamId::LfoDestination, value(parameters, "lfoDestination"));
    synth.setParameter(SynthCore::ParamId::ModWheelAmount, modWheelAmount);
    synth.setParameter(SynthCore::ParamId::FineTune, value(parameters, "fineTune"));
}

void LadderVoiceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    pushParametersToSynth();

    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            synth.noteOn(message.getNoteNumber(), message.getVelocity() * 127.0f);
        } else if (message.isNoteOff()) {
            synth.noteOff(message.getNoteNumber());
        } else if (message.isPitchWheel()) {
            const auto normalized = (static_cast<double>(message.getPitchWheelValue()) - 8192.0) / 8192.0;
            synth.setPitchBend(normalized * pitchBendRange);
        } else if (message.isController() && message.getControllerNumber() == 1) {
            synth.setModWheel(message.getControllerValue() / 127.0);
        }
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    for (int sample = 0; sample < numSamples; ++sample) {
        const auto out = synth.processSample();
        for (int channel = 0; channel < numChannels; ++channel) {
            buffer.setSample(channel, sample, out);
        }
    }
}

juce::AudioProcessorEditor* LadderVoiceAudioProcessor::createEditor()
{
    return new LadderVoiceAudioProcessorEditor(*this);
}

void LadderVoiceAudioProcessor::resetToDefaults()
{
    applyInitPatchToAPVTS();
}

void LadderVoiceAudioProcessor::applyInitPatchToAPVTS()
{
    for (auto* param : getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
    }
    pushParametersToSynth();
}

void LadderVoiceAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

void LadderVoiceAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LadderVoiceAudioProcessor();
}
