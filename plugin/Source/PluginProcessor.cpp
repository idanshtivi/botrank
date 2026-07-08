#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../synth_core/Include/RtEventLog.h"
#include "../../synth_core/Include/NoteOnAudioCapture.h"
#include <cstdio>
#include <chrono>
#if LADDERVOICE_ENABLE_POLY_TRACE
#include <cmath>
#endif

namespace {
// Printed once at startup so it's immediately obvious which binary is
// running — the poly-crackle investigation traced back to preset-testing
// workflow accidentally launching a Debug/traced build, which alone is
// enough to miss the audio callback's real-time deadline under polyphony.
void printStartupBanner()
{
#if JUCE_DEBUG
    const char* config = "Debug";
#else
    const char* config = "Release";
#endif
#if LADDERVOICE_ENABLE_POLY_TRACE
    const char* tracing = "ON";
#else
    const char* tracing = "OFF";
#endif
    juce::String banner;
    banner << "==================================\n"
           << "Ladder Voice\n"
           << "Configuration: " << config << "\n"
           << "Tracing: " << tracing << "\n"
           << "Build date: " << __DATE__ << "\n"
           << "Build time: " << __TIME__ << "\n"
           << "==================================";
    juce::Logger::writeToLog(banner);
    // Also to stdout: this project is routinely launched from a terminal
    // during development, where the debugger's log output isn't visible.
    std::fputs((banner + "\n").toRawUTF8(), stdout);
    std::fflush(stdout);
}

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
    printStartupBanner();
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

    auto percentText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) {
            return juce::String(juce::roundToInt(v * 100.0f)) + "%";
        })
        .withValueFromStringFunction([](const juce::String& t) -> float {
            const auto text = t.trim();
            const auto numeric = text.endsWithChar('%') ? text.dropLastCharacters(1).trim() : text;
            const auto value = static_cast<float>(numeric.getDoubleValue());
            return value > 1.0f ? value / 100.0f : value;
        });

    juce::NormalisableRange<float> unitRange(0.0f, 1.0f, 0.01f);
    juce::NormalisableRange<float> levelRange(0.0f, 1.0f, 0.001f);
    juce::NormalisableRange<float> driveRange(0.0f, 3.0f, 0.01f);
    // True logarithmic mapping for attack: value = start * (end/start)^normalised.
    // The old skewed range (setSkewForCentre(0.05f), skew≈0.13) used exponent 7.67 on the
    // normalised value. At min (normalised=0) even a 20px drag gives a normalised delta
    // that pow(n, 7.67) maps to < float32 epsilon — the stored value is bit-identical to
    // 0.001 every tick, so currentNorm resets to 0.0 and the knob never escapes 1ms.
    // With log mapping, normalised delta of 0.000111 (1-pixel slow drag) immediately
    // produces value ≈ 0.001001 — 8000× above float epsilon — so the knob moves instantly.
    auto attackRange = juce::NormalisableRange<float>(
        0.001f, 10.0f,
        [](float s, float e, float n) { return s * std::pow(e / s, n); },
        [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); });
    // Decay/Release had the exact same stuck-knob bug as the old Attack range
    // (setSkewForCentre with a small skew factor -> a huge effective exponent
    // on the normalised value, so drags starting near the range minimum
    // produce a value delta below float32 epsilon and the knob doesn't
    // visibly move until enough velocity has accumulated to jump past the
    // dead zone). Same true-logarithmic mapping fix as attackRange, just
    // with Decay/Release's own 0.005s floor.
    auto decayRange = juce::NormalisableRange<float>(
        0.005f, 10.0f,
        [](float s, float e, float n) { return s * std::pow(e / s, n); },
        [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); });
    decayRange.interval = 0.001f;
    auto releaseRange = juce::NormalisableRange<float>(
        0.005f, 10.0f,
        [](float s, float e, float n) { return s * std::pow(e / s, n); },
        [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); });
    releaseRange.interval = 0.001f;
    juce::NormalisableRange<float> glideRange(0.0f, 5.0f);
    glideRange.setSkewForCentre(0.25f);
    glideRange.interval = 0.001f;

    floatParam("osc1Level", "Osc 1 Level", levelRange, def(SynthCore::ParamId::Osc1Level), twoDecimals);
    floatParam("osc2Level", "Osc 2 Level", levelRange, def(SynthCore::ParamId::Osc2Level), twoDecimals);
    floatParam("osc3Level", "Osc 3 Level", levelRange, def(SynthCore::ParamId::Osc3Level), twoDecimals);
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
    floatParam("noiseLevel", "Noise Level", levelRange, def(SynthCore::ParamId::NoiseLevel), twoDecimals);
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
               juce::NormalisableRange<float>(0.10f, 0.90f, 0.01f), def(SynthCore::ParamId::Osc1PulseWidth), percentText);
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

    _rtBlockIndex = 0;
    _rtSessionSample = 0;
    _rtParamCacheValid = false;
    SynthCore::RtEventLog::instance().start();
    if (SynthCore::RtEventLog::instance().isActive()) {
        juce::String banner;
        banner << "RtEventLog active. Patch: " << presetManager.getCurrentPresetName()
               << "  sampleRate=" << sampleRate << "  blockSize=" << samplesPerBlock;
        std::fputs((banner + "\n").toRawUTF8(), stdout);
        std::fflush(stdout);
    }

    SynthCore::NoteOnAudioCapture::instance().start(sampleRate);
    if (SynthCore::NoteOnAudioCapture::instance().isActive()) {
        juce::String banner;
        banner << "NoteOnAudioCapture active. Patch: " << presetManager.getCurrentPresetName()
               << "  sampleRate=" << sampleRate;
        std::fputs((banner + "\n").toRawUTF8(), stdout);
        std::fflush(stdout);
    }
#if LADDERVOICE_ENABLE_POLY_TRACE
    _traceBlockIndex    = 0;
    _traceSessionSample = 0;
    _traceCrackleIndex  = 0;
    _tracePrevSample    = 0.0f;
    _traceLastNoteSample = 0;
    auto& logger = SynthCore::PolyTraceLogger::instance();
    logger.start();
    if (logger.isActive()) {
        // Raw output capture next to the CSV trace files: lets a reported
        // click be inspected in the actual waveform, not inferred from the
        // delta-threshold crackle detector (which fires on ordinary
        // sawtooth/square edges just as much as on real glitches).
        const auto wavFile = juce::File::getCurrentWorkingDirectory()
                                 .getChildFile(logger.sessionDirectory())
                                 .getChildFile("output.wav");
        wavFile.getParentDirectory().createDirectory();
        if (auto stream = std::make_unique<juce::FileOutputStream>(wavFile)) {
            if (stream->openedOk()) {
                juce::WavAudioFormat wavFormat;
                _traceWavWriter.reset(wavFormat.createWriterFor(
                    stream.release(), sampleRate, 1u, 32, {}, 0));
            }
        }
    }
#endif
}

void LadderVoiceAudioProcessor::releaseResources()
{
#if LADDERVOICE_ENABLE_POLY_TRACE
    _traceWavWriter.reset();
    SynthCore::PolyTraceLogger::instance().stop();
#endif
    SynthCore::RtEventLog::instance().stop();
    SynthCore::NoteOnAudioCapture::instance().stop();
}

void LadderVoiceAudioProcessor::reset()
{
    // SynthEngine::reset() clears all active/held voices, pitch bend, mod
    // wheel, and sustain-pedal state (equivalent to a power-cycle) while
    // re-applying the current parameter values, so the loaded patch itself
    // is untouched — only realtime playback/MIDI state is cleared.
    synth.reset();
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
    synth.setParameter(SynthCore::ParamId::AnalogDrift,    0.0f); // always off; old saved state must not restore hidden drift
    const auto lfoAmount = value(parameters, "lfoAmount");
    const auto modWheelAmount = value(parameters, "modWheelAmount");
    synth.setParameter(SynthCore::ParamId::LfoEnabled,     (lfoAmount > 0.0001f || modWheelAmount > 0.0001f) ? 1.0f : 0.0f);
    synth.setParameter(SynthCore::ParamId::LfoRate,        value(parameters, "lfoRate"));
    synth.setParameter(SynthCore::ParamId::LfoAmount,      lfoAmount);
    synth.setParameter(SynthCore::ParamId::LfoDestination, value(parameters, "lfoDestination"));
    synth.setParameter(SynthCore::ParamId::ModWheelAmount, modWheelAmount);
    synth.setParameter(SynthCore::ParamId::FineTune, value(parameters, "fineTune"));
}

void LadderVoiceAudioProcessor::_rtLogChangedParams()
{
    // Reads back whatever pushParametersToSynth() just wrote (synth._params
    // via getParameter()) and logs only the values that changed since the
    // previous block -- cheap (kParamCount float compares), no allocation,
    // and does not affect what value was actually applied to the engine.
    auto& rtLog = SynthCore::RtEventLog::instance();
    for (int i = 0; i < SynthCore::kParamCount; ++i) {
        const auto id = static_cast<SynthCore::ParamId>(i);
        const float v = synth.getParameter(id);
        if (!_rtParamCacheValid || _rtLastParamValues[static_cast<size_t>(i)] != v) {
            if (_rtParamCacheValid) {
                rtLog.log(SynthCore::RtEventType::ParamChange, 0, -1, -1, 0,
                           SynthCore::RtAllocReason::NotApplicable,
                           static_cast<int16_t>(i), v);
            }
            _rtLastParamValues[static_cast<size_t>(i)] = v;
        }
    }
    _rtParamCacheValid = true;
}

void LadderVoiceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto& rtLog = SynthCore::RtEventLog::instance();
    const auto tBlockStart = std::chrono::high_resolution_clock::now();

    pushParametersToSynth();
    if (rtLog.isActive())
        _rtLogChangedParams();

    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    int currentSample = 0;

    if (rtLog.isActive()) {
        rtLog.setBlockContext(_rtBlockIndex, _rtSessionSample);
        int active = 0, held = 0, releasing = 0;
        synth.getVoiceCounts(active, held, releasing);
        // Block event: activeVoices/heldVoices/releasingVoices are packed into
        // the note/velocity/paramId fields (Block events don't otherwise use
        // them) to avoid growing the event struct for a throwaway diagnostic.
        rtLog.log(SynthCore::RtEventType::Block, 0, -1,
                   static_cast<int16_t>(active), static_cast<uint8_t>(held),
                   SynthCore::RtAllocReason::NotApplicable,
                   static_cast<int16_t>(releasing), 0.0f,
                   static_cast<uint32_t>(numSamples), static_cast<float>(getSampleRate()));
    }

#if LADDERVOICE_ENABLE_POLY_TRACE
    auto& logger = SynthCore::PolyTraceLogger::instance();
    const double sr = getSampleRate();
    POLY_TRACE_SET_CTX(_traceBlockIndex, _traceSessionSample,
                       static_cast<uint32_t>(numSamples),
                       static_cast<float>(sr));
#endif

    auto& noteCapture = SynthCore::NoteOnAudioCapture::instance();

    auto renderUntil = [&](int endSample) {
        endSample = juce::jlimit(0, numSamples, endSample);
        for (; currentSample < endSample; ++currentSample) {
            const auto out = synth.processSample();
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.setSample(channel, currentSample, out);
            // Captures the exact final signal (post-everything) for the
            // live NoteOn-snippet investigation -- observation only.
            if (noteCapture.isActive())
                noteCapture.pushSample(out);
        }
    };

    for (const auto metadata : midiMessages) {
        renderUntil(metadata.samplePosition);

        const auto message = metadata.getMessage();
#if LADDERVOICE_ENABLE_POLY_TRACE
        if (message.isNoteOn() || message.isNoteOff())
            _traceLastNoteSample = _traceSessionSample + static_cast<uint64_t>(currentSample);
#endif
        if (message.isNoteOn() && noteCapture.isActive()) {
            noteCapture.onNoteOn(message.getNoteNumber(),
                                  static_cast<uint8_t>(message.getVelocity() * 127.0f));
        }
        if (rtLog.isActive()) {
            const auto offset = static_cast<uint32_t>(currentSample);
            if (message.isNoteOn()) {
                rtLog.log(SynthCore::RtEventType::NoteOnMsg, offset, -1,
                           static_cast<int16_t>(message.getNoteNumber()),
                           static_cast<uint8_t>(message.getVelocity() * 127.0f));
            } else if (message.isNoteOff()) {
                rtLog.log(SynthCore::RtEventType::NoteOffMsg, offset, -1,
                           static_cast<int16_t>(message.getNoteNumber()));
            } else {
                rtLog.log(SynthCore::RtEventType::MidiOther, offset);
            }
        }
        // Single shared path for all MIDI (note on/off, pitch bend, mod
        // wheel, sustain/all-notes-off/etc.) — SynthEngine::processMidi is
        // the same, already-tested function used everywhere else, so there
        // is exactly one place that decides what each MIDI message means.
        synth.processMidi(message.getRawData(), message.getRawDataSize());
    }

    renderUntil(numSamples);

    if (noteCapture.isActive())
        noteCapture.serviceCompletedCaptures();

    if (rtLog.isActive()) {
        const auto tBlockEnd = std::chrono::high_resolution_clock::now();
        const auto blockTimeUs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(tBlockEnd - tBlockStart).count());
        const double sr = getSampleRate();
        const double blockBudgetUs = sr > 0.0 ? static_cast<double>(numSamples) / sr * 1.0e6 : 0.0;
        const double cpuPct = blockBudgetUs > 0.0 ? blockTimeUs / blockBudgetUs * 100.0 : 0.0;
        if (cpuPct > 60.0) {
            // Lightweight real-time-deadline warning -- an audio-thread stall
            // (denormals, page fault, driver hiccup) can itself sound like a
            // click; this flags any block that got uncomfortably close to
            // (or over) its real-time deadline.
            rtLog.log(SynthCore::RtEventType::CpuWarning, 0, -1, -1, 0,
                       SynthCore::RtAllocReason::NotApplicable, -1,
                       static_cast<float>(cpuPct));
        }
        _rtSessionSample += static_cast<uint64_t>(numSamples);
        ++_rtBlockIndex;
    }

#if LADDERVOICE_ENABLE_POLY_TRACE
    if (_traceWavWriter != nullptr && numSamples > 0)
        _traceWavWriter->writeFromAudioSampleBuffer(buffer, 0, numSamples);

    if (logger.isActive() && numSamples > 0) {
        // Measure block wall-clock time
        const auto tBlockEnd = std::chrono::high_resolution_clock::now();
        const auto blockTimeUs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(tBlockEnd - tBlockStart).count());
        const float blockBudgetUs = static_cast<float>(numSamples) / static_cast<float>(sr) * 1.0e6f;
        const float cpuPct = blockBudgetUs > 0.0f ? blockTimeUs / blockBudgetUs * 100.0f : 0.0f;

        // Scan output buffer for peak, RMS, and max sample-to-sample delta
        const float* readPtr = buffer.getReadPointer(0);
        float peak = 0.0f, sumSq = 0.0f;
        float maxDelta = 0.0f;
        int   maxDeltaOff = 0;
        float prev = _tracePrevSample;
        for (int i = 0; i < numSamples; ++i) {
            const float s = readPtr[i];
            const float d = std::abs(s - prev);
            if (d > maxDelta) { maxDelta = d; maxDeltaOff = i; }
            const float a = std::abs(s);
            if (a > peak) peak = a;
            sumSq += s * s;
            prev = s;
        }
        _tracePrevSample = prev;
        const float blockRms = std::sqrt(sumSq / static_cast<float>(numSamples));

        // Update rolling average delta (used for crackle ratio detection)
        const float avgDelta = logger.recentAvgDelta();
        logger.updateRecentAvgDelta(maxDelta);

        // Push block trace event
        SynthCore::SnapshotPayload blockSnaps[SynthCore::kPolyVoiceCount];
        synth.fillVoiceSnapshots(blockSnaps, 0);
        uint8_t blockActive = 0, blockHeld = 0, blockReleasing = 0;
        for (int vi = 0; vi < SynthCore::kPolyVoiceCount; ++vi) {
            if (blockSnaps[vi].isActive) {
                ++blockActive;
                if (!blockSnaps[vi].isHeld) ++blockReleasing;
            }
            if (blockSnaps[vi].isHeld) ++blockHeld;
        }

        SynthCore::BlockPayload bp{};
        bp.sessionSample  = _traceSessionSample;
        bp.blockIndex     = _traceBlockIndex;
        bp.blockSize      = static_cast<uint32_t>(numSamples);
        bp.sampleRate     = static_cast<float>(sr);
        bp.playMode       = static_cast<uint8_t>(synth.playModeForTrace());
        bp.activeVoices   = blockActive;
        bp.heldVoices     = blockHeld;
        bp.releasingVoices = blockReleasing;
        bp.finalPeak      = peak;
        bp.finalRms       = blockRms;
        bp.maxDelta       = maxDelta;
        bp.maxDeltaOffset = static_cast<uint16_t>(maxDeltaOff < 65535 ? maxDeltaOff : 65535);
        bp.blockTimeUs    = blockTimeUs;
        bp.cpuPercent     = cpuPct;
        bp.droppedEvents  = logger.droppedCount();
        logger.pushBlock(bp);

        // Crackle detection: absolute threshold OR ratio above recent average
        const bool aboveAbs   = maxDelta > SynthCore::PolyTraceLogger::kAbsoluteThreshold;
        const bool aboveRatio = avgDelta > 0.002f &&
                                maxDelta > avgDelta * SynthCore::PolyTraceLogger::kRatioThreshold;
        if (aboveAbs || aboveRatio) {
            // Capture the two samples straddling the max delta
            float prevAtMax = 0.0f, curAtMax = 0.0f;
            if (maxDeltaOff > 0) {
                prevAtMax = readPtr[maxDeltaOff - 1];
                curAtMax  = readPtr[maxDeltaOff];
            } else if (numSamples > 1) {
                prevAtMax = _tracePrevSample; // approximation
                curAtMax  = readPtr[0];
            }

            // Capture per-voice state snapshot at crackle time
            SynthCore::SnapshotPayload snaps[SynthCore::kPolyVoiceCount];
            synth.fillVoiceSnapshots(snaps, _traceCrackleIndex);

            uint8_t activeCount = 0, heldCount = 0, releasingCount = 0;
            for (int vi = 0; vi < SynthCore::kPolyVoiceCount; ++vi) {
                if (snaps[vi].isActive) {
                    ++activeCount;
                    if (!snaps[vi].isHeld) ++releasingCount;
                }
                if (snaps[vi].isHeld) ++heldCount;
            }

            SynthCore::CracklePayload cp{};
            cp.sessionSample      = _traceSessionSample + static_cast<uint64_t>(maxDeltaOff);
            cp.blockIndex         = _traceBlockIndex;
            cp.sampleOffset       = static_cast<uint32_t>(maxDeltaOff);
            cp.crackleIndex       = _traceCrackleIndex;
            cp.maxDelta           = maxDelta;
            cp.prevSample         = prevAtMax;
            cp.curSample          = curAtMax;
            cp.recentAvgDelta     = avgDelta;
            cp.activeVoices       = activeCount;
            cp.heldVoices         = heldCount;
            cp.releasingVoices    = releasingCount;
            cp.playModeAtCrackle  = static_cast<uint8_t>(synth.playModeForTrace());
            cp.lastNoteEventSample = _traceLastNoteSample;
            cp.droppedEvents      = logger.droppedCount();
            cp.monoVoiceActive    = synth.monoVoiceActiveForTrace() ? 1u : 0u;
            cp.monoLoudnessValue  = static_cast<float>(synth.monoLoudnessValueForTrace());
            cp.monoFilterValue    = static_cast<float>(synth.monoFilterValueForTrace());
            logger.pushCrackle(cp);

            for (int vi = 0; vi < SynthCore::kPolyVoiceCount; ++vi)
                logger.pushSnapshot(snaps[vi]);

            ++_traceCrackleIndex;
        }
    }

    _traceSessionSample += static_cast<uint64_t>(numSamples);
    ++_traceBlockIndex;
#endif
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
    auto state = parameters.copyState();
    state.setProperty("presetName", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void LadderVoiceAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        auto name = tree.getProperty("presetName", "Init").toString();
        presetManager.setCurrentPresetName(name);
        parameters.replaceState(tree);
        // Force analogDrift to 0 — there is no visible UI control for it.
        // Old saved state may contain a non-zero value from a prior build.
        if (auto* p = parameters.getParameter("analogDrift"))
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LadderVoiceAudioProcessor();
}

