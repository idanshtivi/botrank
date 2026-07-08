#include "PresetManager.h"

namespace {
// All APVTS parameter IDs used in presets
static const char* const kAllParamIds[] = {
    "playMode", "osc1Enabled", "osc2Enabled", "osc3Enabled",
    "osc1Level", "osc2Level", "osc3Level",
    "osc1Waveform", "osc2Waveform", "osc3Waveform",
    "osc1Range", "osc2Range", "osc3Range",
    "osc2Detune", "osc3Detune", "osc3KeyboardTracking",
    "mixerDrive", "noiseLevel", "noiseMode",
    "filterCutoff", "filterResonance", "filterContour",
    "filterAttack", "filterDecay", "filterSustain", "filterRelease",
    "filterDrive", "filterKeyboardTracking",
    "loudnessAttack", "loudnessDecay", "loudnessSustain", "loudnessRelease",
    "masterVolume", "glideEnabled", "glideTime",
    "pitchBendRange", "fineTune",
    "legato", "retrigger", "notePriority",
    "osc1PulseWidth",
    "lfoRate", "lfoAmount", "lfoDestination", "modWheelAmount",
    nullptr
};

using P = std::pair<juce::String, float>;
using PL = std::vector<P>;

// Build a full param list from the init defaults, then override with provided overrides.
// Init defaults match initPatchValue() in SynthEngine.h.
//
// NOTE on filterKeyboardTracking: this parameter is registered in
// PluginProcessor.cpp as a 3-position AudioParameterChoice (0=Off, 1=Half,
// 2=Full) — NOT a continuous 0.0-1.0 amount — and gets halved in
// pushParametersToSynth() before reaching the engine. Only 0.0f/1.0f/2.0f
// are valid values here; anything else (e.g. 0.5f) is an invalid choice
// index that silently rounds to the nearest one, not the "amount" its
// numeric value would suggest.
static PL makePreset(std::initializer_list<P> overrides)
{
    PL base = {
        {"playMode",              0.0f},
        {"osc1Enabled",           1.0f}, {"osc2Enabled",  1.0f}, {"osc3Enabled",  0.0f},
        {"osc1Level",             0.65f},{"osc2Level",    0.30f}, {"osc3Level",    0.0f},
        {"osc1Waveform",          2.0f}, {"osc2Waveform", 2.0f},  {"osc3Waveform", 0.0f},
        {"osc1Range",             3.0f}, {"osc2Range",    3.0f},  {"osc3Range",    3.0f},
        {"osc2Detune",            0.0f}, {"osc3Detune",   0.0f},
        {"osc3KeyboardTracking",  1.0f},
        {"mixerDrive",            1.0f},
        {"noiseLevel",            0.0f}, {"noiseMode",    0.0f},
        {"filterCutoff",       6000.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.15f},
        {"filterAttack",       0.005f},  {"filterDecay",  0.30f},  {"filterSustain", 0.0f},  {"filterRelease", 0.20f},
        {"filterDrive",           0.50f},{"filterKeyboardTracking", 0.0f},
        {"loudnessAttack",     0.005f},  {"loudnessDecay",0.25f},  {"loudnessSustain",0.75f}, {"loudnessRelease",0.20f},
        {"masterVolume",          0.60f},
        {"glideEnabled",          0.0f}, {"glideTime",    0.05f},
        {"pitchBendRange",        2.0f}, {"fineTune",     0.0f},
        {"legato",                0.0f}, {"retrigger",    1.0f},   {"notePriority",  0.0f},
        {"osc1PulseWidth",        0.50f},
        {"lfoRate",               2.0f}, {"lfoAmount",    0.0f},   {"lfoDestination",0.0f},  {"modWheelAmount",0.0f},
        {"analogDrift",           0.0f},
    };

    for (auto& ov : overrides) {
        for (auto& b : base) {
            if (b.first == ov.first) { b.second = ov.second; break; }
        }
    }
    return base;
}
} // namespace

PresetManager::PresetManager()
{
    buildFactoryPresets();
}

void PresetManager::buildFactoryPresets()
{
    // 0 – Init (exact engine defaults)
    factoryPresets.push_back({"Init", "Init", makePreset({})});

    // ══════════════════════════════════════════════════════════════════════
    // ANALOG ESSENTIALS (20) — core patch types every analog synth needs,
    // in the spirit of the classic mono/poly analog instruments (fat mono
    // basses and leads, poly brass stacks, simple sustained keys/pads).
    // Original sounds programmed from scratch on this engine — no factory
    // preset from any commercial product was copied.
    // ══════════════════════════════════════════════════════════════════════

    factoryPresets.push_back({"Cold War Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.90f},          // Saw, 16'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.55f}, {"osc2Detune", 0.04f}, // Square, 16'
        {"mixerDrive", 1.7f},
        {"filterCutoff", 400.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.50f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.30f}, {"filterSustain", 0.05f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.55f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.25f}, {"loudnessSustain", 0.10f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Memory Thick Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.50f}, {"osc2Detune", 0.08f},
        {"noiseLevel", 0.05f},
        {"mixerDrive", 1.5f},
        {"filterCutoff", 450.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.35f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.40f}, {"filterSustain", 0.15f}, {"filterRelease", 0.18f},
        {"filterDrive", 0.40f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.32f}, {"loudnessSustain", 0.20f}, {"loudnessRelease", 0.16f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Sequential Punch Bass", "Bass", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 2}, {"osc1Level", 0.90f}, {"osc1PulseWidth", 0.40f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.40f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 550.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.60f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.18f}, {"filterSustain", 0.0f}, {"filterRelease", 0.10f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.16f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.09f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Oberheim Growl Bass", "Bass", makePreset({
        {"osc1Waveform", 5}, {"osc1Range", 2}, {"osc1Level", 1.0f}, {"osc1PulseWidth", 0.30f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.30f},
        {"mixerDrive", 2.0f},
        {"filterCutoff", 380.0f}, {"filterResonance", 0.42f}, {"filterContour", 0.40f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.0f}, {"filterRelease", 0.13f},
        {"filterDrive", 1.0f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.24f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.11f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Jupiter Glide Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 2}, {"osc2Level", 0.40f},
        {"mixerDrive", 0.90f},
        {"filterCutoff", 480.0f}, {"filterResonance", 0.12f}, {"filterContour", 0.20f},
        {"filterAttack", 0.01f}, {"filterDecay", 0.35f}, {"filterSustain", 0.35f}, {"filterRelease", 0.22f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.01f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.50f}, {"loudnessRelease", 0.25f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.09f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Vintage Mono Lead", "Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 1.2f},
        {"filterCutoff", 6500.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.18f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.25f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"pitchBendRange", 2.0f}, {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Sequential Bright Lead", "Lead", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.35f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 1.4f},
        {"filterCutoff", 5800.0f}, {"filterResonance", 0.35f}, {"filterContour", 0.22f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.28f}, {"filterSustain", 0.60f}, {"filterRelease", 0.22f},
        {"filterDrive", 0.50f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.005f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.18f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Oberheim Stack Lead", "Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.09f},
        {"mixerDrive", 1.5f},
        {"filterCutoff", 6000.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.20f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.30f}, {"filterSustain", 0.60f}, {"filterRelease", 0.24f},
        {"filterDrive", 0.60f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.006f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Jupiter Sweet Lead", "Lead", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 1}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 0.70f},
        {"filterCutoff", 3800.0f}, {"filterResonance", 0.12f}, {"filterContour", 0.10f},
        {"filterAttack", 0.04f}, {"filterDecay", 0.28f}, {"filterSustain", 0.80f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.03f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.30f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Memory Unison Lead", "Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.65f}, {"osc2Detune", 0.18f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 6200.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.16f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.26f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.007f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.22f},
        {"pitchBendRange", 5.0f}, {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Classic Analog Keys", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.75f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 0.85f},
        {"filterCutoff", 3200.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.40f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.40f}, {"filterSustain", 0.35f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.50f}, {"loudnessSustain", 0.50f}, {"loudnessRelease", 0.35f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Jupiter String Keys", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 4}, {"osc2Level", 0.35f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 0.70f},
        {"filterCutoff", 2800.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.15f},
        {"filterAttack", 0.35f}, {"filterDecay", 0.40f}, {"filterSustain", 0.80f}, {"filterRelease", 1.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.35f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.1f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Oberheim Brass Keys", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.70f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 2400.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.50f},
        {"filterAttack", 0.07f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.35f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.04f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.30f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Memory Warm Pad", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.13f},
        {"mixerDrive", 0.65f},
        {"filterCutoff", 2600.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.18f},
        {"filterAttack", 0.6f}, {"filterDecay", 0.5f}, {"filterSustain", 0.80f}, {"filterRelease", 1.4f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.65f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.6f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Mini Classic Pad", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.65f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.08f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 2000.0f}, {"filterResonance", 0.07f}, {"filterContour", 0.10f},
        {"filterAttack", 0.8f}, {"filterDecay", 0.5f}, {"filterSustain", 0.82f}, {"filterRelease", 1.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.85f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.7f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Oberheim Horn Stack", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.75f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 2700.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.55f},
        {"filterAttack", 0.06f}, {"filterDecay", 0.28f}, {"filterSustain", 0.65f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.03f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.28f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Jupiter Brass Ensemble", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.68f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.48f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 2200.0f}, {"filterResonance", 0.16f}, {"filterContour", 0.60f},
        {"filterAttack", 0.10f}, {"filterDecay", 0.30f}, {"filterSustain", 0.70f}, {"filterRelease", 0.35f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.06f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.32f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Sequential Poly Brass", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 3000.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.65f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.18f}, {"filterSustain", 0.30f}, {"filterRelease", 0.15f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.20f}, {"loudnessSustain", 0.35f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Memory Horn Section", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.70f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.40f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 2500.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.50f},
        {"filterAttack", 0.08f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.32f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.05f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.30f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Mini Mono Brass", "Brass", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 1.4f},
        {"filterCutoff", 3200.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.55f},
        {"filterAttack", 0.01f}, {"filterDecay", 0.25f}, {"filterSustain", 0.55f}, {"filterRelease", 0.22f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.01f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"masterVolume", 0.55f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // PERFORMANCE (10) — glide, unison, mod-wheel expression, and other
    // live-playable techniques front and center.
    // ══════════════════════════════════════════════════════════════════════

    factoryPresets.push_back({"Expressive Slide Lead", "Performance", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 5200.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.15f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.24f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.22f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.14f},
        {"pitchBendRange", 2.0f}, {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Funk Clav Performance", "Performance", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.40f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 3600.0f}, {"filterResonance", 0.32f}, {"filterContour", 0.60f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.18f}, {"filterSustain", 0.10f}, {"filterRelease", 0.12f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.20f}, {"loudnessSustain", 0.15f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Wheel Vibrato Lead", "Performance", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 6000.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.15f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.68f}, {"filterRelease", 0.24f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.22f},
        {"lfoRate", 5.5f}, {"lfoAmount", 0.0f}, {"lfoDestination", 0.0f}, {"modWheelAmount", 0.35f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Unison Glide Solo", "Performance", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.65f}, {"osc2Detune", 0.16f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 5600.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.16f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.25f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.007f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.22f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.10f},
        {"pitchBendRange", 5.0f}, {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Talking Filter Lead", "Performance", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.30f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 2200.0f}, {"filterResonance", 0.35f}, {"filterContour", 0.20f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.60f}, {"filterRelease", 0.22f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.007f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"lfoRate", 4.0f}, {"lfoAmount", 0.0f}, {"lfoDestination", 1.0f}, {"modWheelAmount", 0.5f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Legato Slide Bass", "Performance", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.40f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 500.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.30f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.35f}, {"filterSustain", 0.25f}, {"filterRelease", 0.18f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.35f}, {"loudnessRelease", 0.16f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.07f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Pitch Bend Screamer", "Performance", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.35f}, {"osc2Detune", 0.08f},
        {"mixerDrive", 1.6f},
        {"filterCutoff", 5000.0f}, {"filterResonance", 0.45f}, {"filterContour", 0.25f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.30f}, {"filterSustain", 0.55f}, {"filterRelease", 0.22f},
        {"filterDrive", 0.80f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.005f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"pitchBendRange", 12.0f}, {"masterVolume", 0.53f},
    })});

    factoryPresets.push_back({"Smooth Portamento Solo", "Performance", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 1}, {"osc2Range", 3}, {"osc2Level", 0.30f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 0.80f},
        {"filterCutoff", 4000.0f}, {"filterResonance", 0.14f}, {"filterContour", 0.10f},
        {"filterAttack", 0.02f}, {"filterDecay", 0.28f}, {"filterSustain", 0.75f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.02f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.30f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.20f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Analog Ribbon Lead", "Performance", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 5500.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.14f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.28f}, {"filterSustain", 0.68f}, {"filterRelease", 0.22f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.006f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.05f},
        {"pitchBendRange", 3.0f}, {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Dynamic Brass Performance", "Performance", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.75f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 2500.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.50f},
        {"filterAttack", 0.05f}, {"filterDecay", 0.28f}, {"filterSustain", 0.65f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.03f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.28f},
        {"lfoRate", 3.0f}, {"lfoAmount", 0.0f}, {"lfoDestination", 1.0f}, {"modWheelAmount", 0.30f},
        {"masterVolume", 0.55f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // CINEMATIC (15) — huge pads, evolving textures, emotional soundscapes,
    // deep drones, expressive analog atmospheres.
    // ══════════════════════════════════════════════════════════════════════

    factoryPresets.push_back({"Vast Analog Horizon", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.14f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 2200.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.15f},
        {"filterAttack", 1.2f}, {"filterDecay", 0.8f}, {"filterSustain", 0.85f}, {"filterRelease", 2.5f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.3f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.8f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Fading Light Drone", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 2}, {"osc2Level", 0.50f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.10f},
        {"filterAttack", 2.0f}, {"filterDecay", 1.0f}, {"filterSustain", 0.90f}, {"filterRelease", 3.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 2.2f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.5f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Distant Memory Pad", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.09f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 1800.0f}, {"filterResonance", 0.12f}, {"filterContour", 0.30f},
        {"filterAttack", 1.0f}, {"filterDecay", 0.7f}, {"filterSustain", 0.80f}, {"filterRelease", 2.2f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.1f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.5f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Cathedral Choir Analog", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 4}, {"osc2Level", 0.40f}, {"osc2Detune", 0.11f},
        {"mixerDrive", 0.55f},
        {"filterCutoff", 2400.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.20f},
        {"filterAttack", 1.4f}, {"filterDecay", 0.7f}, {"filterSustain", 0.85f}, {"filterRelease", 2.8f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.5f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.0f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Slow Awakening Texture", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.08f},
        {"mixerDrive", 0.70f},
        {"filterCutoff", 250.0f}, {"filterResonance", 0.30f}, {"filterContour", 1.0f},
        {"filterAttack", 3.5f}, {"filterDecay", 1.0f}, {"filterSustain", 0.70f}, {"filterRelease", 2.0f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.5f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.2f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Frozen Cathedral Drone", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 2}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 0.45f},
        {"filterCutoff", 1200.0f}, {"filterResonance", 0.06f}, {"filterContour", 0.10f},
        {"filterAttack", 2.5f}, {"filterDecay", 1.0f}, {"filterSustain", 0.90f}, {"filterRelease", 3.5f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 2.6f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 4.0f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Ancient Machine Drone", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 2}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 5}, {"osc2Range", 2}, {"osc2Level", 0.45f}, {"osc1PulseWidth", 0.30f},
        {"mixerDrive", 0.80f},
        {"filterCutoff", 350.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.30f},
        {"filterAttack", 2.0f}, {"filterDecay", 1.2f}, {"filterSustain", 0.75f}, {"filterRelease", 3.0f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 1.8f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.2f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Ocean of Static Pad", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.16f},
        {"noiseLevel", 0.10f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 3500.0f}, {"filterResonance", 0.12f}, {"filterContour", 0.20f},
        {"filterAttack", 1.0f}, {"filterDecay", 0.6f}, {"filterSustain", 0.80f}, {"filterRelease", 2.4f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.1f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.6f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Grief and Glass Atmosphere", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 0.45f},
        {"filterCutoff", 1600.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.12f},
        {"filterAttack", 1.6f}, {"filterDecay", 0.8f}, {"filterSustain", 0.82f}, {"filterRelease", 3.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.7f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.2f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Hopeful Horizon Pad", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 4}, {"osc2Level", 0.40f}, {"osc2Detune", 0.09f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 3200.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.25f},
        {"filterAttack", 0.9f}, {"filterDecay", 0.6f}, {"filterSustain", 0.85f}, {"filterRelease", 2.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.2f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Widescreen Analog Strings", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 4}, {"osc2Level", 0.45f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 0.65f},
        {"filterCutoff", 2900.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.20f},
        {"filterAttack", 0.5f}, {"filterDecay", 0.6f}, {"filterSustain", 0.85f}, {"filterRelease", 1.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.55f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.8f},
        {"masterVolume", 0.53f},
    })});

    factoryPresets.push_back({"Subterranean Drone", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 1}, {"osc1Level", 0.70f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.40f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 220.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.10f},
        {"filterAttack", 2.0f}, {"filterDecay", 1.0f}, {"filterSustain", 0.80f}, {"filterRelease", 3.0f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 2.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.4f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Echoes of Tomorrow", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 0.65f},
        {"filterCutoff", 500.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.90f},
        {"filterAttack", 2.8f}, {"filterDecay", 1.2f}, {"filterSustain", 0.65f}, {"filterRelease", 2.2f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.6f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.4f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Weightless Atmosphere", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 4}, {"osc2Level", 0.35f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 0.40f},
        {"filterCutoff", 3800.0f}, {"filterResonance", 0.06f}, {"filterContour", 0.10f},
        {"filterAttack", 1.3f}, {"filterDecay", 0.6f}, {"filterSustain", 0.85f}, {"filterRelease", 2.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.4f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.9f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Requiem Pad", "Cinematic", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.08f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 1200.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.15f},
        {"filterAttack", 1.8f}, {"filterDecay", 0.9f}, {"filterSustain", 0.80f}, {"filterRelease", 3.2f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 2.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.5f},
        {"masterVolume", 0.50f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // FX (5) — sweeps, risers, and textures that showcase filter behavior
    // rather than sounds meant to be played melodically.
    // ══════════════════════════════════════════════════════════════════════

    factoryPresets.push_back({"Rising Tension Sweep", "FX", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.65f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 150.0f}, {"filterResonance", 0.35f}, {"filterContour", 1.0f},
        {"filterAttack", 3.0f}, {"filterDecay", 0.8f}, {"filterSustain", 0.60f}, {"filterRelease", 0.8f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.02f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.6f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Falling Sky Sweep", "FX", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 6000.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.80f},
        {"filterAttack", 0.01f}, {"filterDecay", 1.5f}, {"filterSustain", 0.10f}, {"filterRelease", 0.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.01f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.5f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Resonant Metal Zap", "FX", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 5}, {"osc2Level", 0.30f},
        {"mixerDrive", 1.2f},
        {"filterCutoff", 1500.0f}, {"filterResonance", 0.60f}, {"filterContour", 0.90f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.30f}, {"filterSustain", 0.0f}, {"filterRelease", 0.20f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.15f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Noise Wind Sweep", "FX", makePreset({
        {"osc1Enabled", 0.0f}, {"osc2Enabled", 0.0f},
        {"noiseLevel", 0.80f}, {"noiseMode", 1.0f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 200.0f}, {"filterResonance", 0.40f}, {"filterContour", 0.80f},
        {"filterAttack", 2.0f}, {"filterDecay", 1.2f}, {"filterSustain", 0.30f}, {"filterRelease", 1.0f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.1f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.2f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Analog Alarm Pulse", "FX", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.30f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 2500.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.30f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.20f}, {"filterSustain", 0.80f}, {"filterRelease", 0.20f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.005f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"lfoRate", 4.0f}, {"lfoAmount", 0.6f}, {"lfoDestination", 1.0f},
        {"masterVolume", 0.55f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // PAD EXPANSION (15) — additive-only premium pad bank. Each preset has
    // a distinct identity (waveform pairing, filter character, envelope
    // scale, and modulation) rather than variations on one template.
    // Existing factory presets/categories above are untouched.
    // ══════════════════════════════════════════════════════════════════════

    factoryPresets.push_back({"Monolith", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.10f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.35f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 0.85f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.35f},
        {"filterAttack", 2.0f}, {"filterDecay", 1.0f}, {"filterSustain", 0.85f}, {"filterRelease", 3.5f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 2.2f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 4.0f},
        {"masterVolume", 0.68f},
    })});

    factoryPresets.push_back({"Cathedral", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 1}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 1}, {"osc2Range", 4}, {"osc2Level", 0.40f}, {"osc2Detune", 0.09f},
        {"mixerDrive", 0.45f},
        {"filterCutoff", 3000.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.15f},
        {"filterAttack", 1.8f}, {"filterDecay", 0.9f}, {"filterSustain", 0.88f}, {"filterRelease", 3.2f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 2.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.4f},
        {"masterVolume", 0.65f},
    })});

    factoryPresets.push_back({"Aurora Bloom", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.58f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.42f}, {"osc2Detune", 0.11f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 2600.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.30f},
        {"filterAttack", 1.0f}, {"filterDecay", 0.6f}, {"filterSustain", 0.82f}, {"filterRelease", 2.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.1f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.2f},
        {"lfoRate", 0.35f}, {"lfoAmount", 0.35f}, {"lfoDestination", 1.0f},
        {"masterVolume", 0.53f},
    })});

    factoryPresets.push_back({"Endless Sky", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 4}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 5}, {"osc2Level", 0.35f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 0.40f},
        {"filterCutoff", 4200.0f}, {"filterResonance", 0.06f}, {"filterContour", 0.12f},
        {"filterAttack", 1.2f}, {"filterDecay", 0.6f}, {"filterSustain", 0.85f}, {"filterRelease", 2.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.3f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.8f},
        {"masterVolume", 0.50f},
    })});

    factoryPresets.push_back({"Frozen Horizon", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.55f}, {"osc1PulseWidth", 0.20f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 5000.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.20f},
        {"filterAttack", 0.8f}, {"filterDecay", 0.5f}, {"filterSustain", 0.80f}, {"filterRelease", 2.2f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.9f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.4f},
        {"masterVolume", 0.54f},
    })});

    factoryPresets.push_back({"Gravity Fields", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 0}, {"osc1Level", 0.65f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.45f},
        {"mixerDrive", 0.55f},
        {"filterCutoff", 260.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.30f},
        {"filterAttack", 2.5f}, {"filterDecay", 1.2f}, {"filterSustain", 0.75f}, {"filterRelease", 3.0f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 2.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.2f},
        {"masterVolume", 0.68f},
    })});

    factoryPresets.push_back({"First Light", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.58f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 1}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 0.55f},
        {"filterCutoff", 1200.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.55f},
        {"filterAttack", 1.6f}, {"filterDecay", 0.8f}, {"filterSustain", 0.78f}, {"filterRelease", 2.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.4f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.2f},
        {"masterVolume", 0.53f},
    })});

    factoryPresets.push_back({"Dreamstate", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.09f},
        {"mixerDrive", 0.45f},
        {"filterCutoff", 2400.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.15f},
        {"filterAttack", 1.5f}, {"filterDecay", 0.7f}, {"filterSustain", 0.85f}, {"filterRelease", 2.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.6f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.8f},
        {"lfoRate", 0.18f}, {"lfoAmount", 0.12f}, {"lfoDestination", 0.0f},
        {"glideEnabled", 1.0f}, {"glideTime", 0.35f},
        {"masterVolume", 0.56f},
    })});

    factoryPresets.push_back({"Celestial Choir", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 1}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.08f},
        {"mixerDrive", 0.55f},
        {"filterCutoff", 1400.0f}, {"filterResonance", 0.35f}, {"filterContour", 0.25f},
        {"filterAttack", 1.3f}, {"filterDecay", 0.7f}, {"filterSustain", 0.80f}, {"filterRelease", 2.4f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.4f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.6f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Distant Suns", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 5}, {"osc1Range", 3}, {"osc1Level", 0.55f}, {"osc1PulseWidth", 0.35f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 5}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.15f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 3000.0f}, {"filterResonance", 0.12f}, {"filterContour", 0.20f},
        {"filterAttack", 1.7f}, {"filterDecay", 0.8f}, {"filterSustain", 0.82f}, {"filterRelease", 3.0f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.8f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.2f},
        {"masterVolume", 0.56f},
    })});

    factoryPresets.push_back({"Echoes of Time", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 3}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 700.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.70f},
        {"filterAttack", 3.0f}, {"filterDecay", 1.5f}, {"filterSustain", 0.55f}, {"filterRelease", 2.5f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.8f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.6f},
        {"lfoRate", 0.25f}, {"lfoAmount", 0.20f}, {"lfoDestination", 1.0f},
        {"masterVolume", 0.56f},
    })});

    factoryPresets.push_back({"Northern Lights", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 5}, {"osc1Range", 3}, {"osc1Level", 0.55f}, {"osc1PulseWidth", 0.40f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 2800.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.25f},
        {"filterAttack", 1.1f}, {"filterDecay", 0.6f}, {"filterSustain", 0.82f}, {"filterRelease", 2.2f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.2f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.4f},
        {"lfoRate", 0.5f}, {"lfoAmount", 0.30f}, {"lfoDestination", 2.0f},
        {"masterVolume", 0.52f},
    })});

    factoryPresets.push_back({"Silent Ocean", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 0}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 2}, {"osc2Level", 0.45f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 0.35f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.05f}, {"filterContour", 0.10f},
        {"filterAttack", 3.5f}, {"filterDecay", 1.5f}, {"filterSustain", 0.88f}, {"filterRelease", 4.0f},
        {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 3.5f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 4.5f},
        {"masterVolume", 0.75f},
    })});

    factoryPresets.push_back({"Ethereal Waves", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 1}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.10f},
        {"noiseLevel", 0.06f}, {"noiseMode", 1.0f},
        {"mixerDrive", 0.45f},
        {"filterCutoff", 3400.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.18f},
        {"filterAttack", 1.4f}, {"filterDecay", 0.7f}, {"filterSustain", 0.83f}, {"filterRelease", 2.6f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.5f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.8f},
        {"lfoRate", 0.3f}, {"lfoAmount", 0.15f}, {"lfoDestination", 1.0f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Infinite Horizon", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.08f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.35f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 0.70f},
        {"filterCutoff", 2000.0f}, {"filterResonance", 0.14f}, {"filterContour", 0.30f},
        {"filterAttack", 1.8f}, {"filterDecay", 0.9f}, {"filterSustain", 0.84f}, {"filterRelease", 3.4f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 2.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 3.6f},
        {"masterVolume", 0.62f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // BIG ANALOG BASSES (15) — additive-only bass expansion. Each preset
    // targets a distinct classic bass archetype (sub, funk, acid, drive,
    // glide, growl, etc.) rather than variations on one template. Existing
    // factory presets/categories above are untouched. All mono, with
    // filterKeyboardTracking=Full so tone stays balanced across >=2 octaves.
    // ══════════════════════════════════════════════════════════════════════

    // 1. Deep sub bass — huge low fundamental, filter kept mostly closed so
    // the sub isn't thinned out by upper harmonics.
    factoryPresets.push_back({"Iron Foundation", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 1}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.45f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 0.70f},
        {"filterCutoff", 300.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.20f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.35f}, {"filterSustain", 0.05f}, {"filterRelease", 0.18f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.10f}, {"loudnessRelease", 0.16f},
        {"masterVolume", 0.62f},
    })});

    // 2. Fat mono bass — classic driven saw+square Moog-style fatness.
    factoryPresets.push_back({"Mammoth Sub", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.88f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.55f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.4f},
        {"filterCutoff", 450.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.45f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.08f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.55f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.12f}, {"loudnessRelease", 0.13f},
        {"masterVolume", 0.58f},
    })});

    // 3. Funk bass — short, percussive, rhythmic pluck in the brighter register.
    factoryPresets.push_back({"Velvet Low End", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.40f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 700.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.55f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.16f}, {"filterSustain", 0.0f}, {"filterRelease", 0.10f},
        {"filterDrive", 0.40f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.15f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.09f},
        {"masterVolume", 0.58f},
    })});

    // 4. Acid-style filter bass — single saw, high resonance/contour, glide
    // slides like a classic 303-style line.
    factoryPresets.push_back({"Rubber Drive", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.95f},
        {"osc2Enabled", 0.0f},
        {"mixerDrive", 1.2f},
        {"filterCutoff", 500.0f}, {"filterResonance", 0.48f}, {"filterContour", 0.85f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.35f}, {"filterSustain", 0.0f}, {"filterRelease", 0.12f},
        {"filterDrive", 0.35f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.10f},
        {"glideEnabled", 1.0f}, {"glideTime", 0.06f}, {"legato", 1.0f},
        {"masterVolume", 0.55f},
    })});

    // 5. Drive bass — heavy mixer/filter saturation for a growling, gritty edge.
    factoryPresets.push_back({"Black Circuit Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.45f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 2.0f},
        {"filterCutoff", 450.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.40f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.30f}, {"filterSustain", 0.10f}, {"filterRelease", 0.15f},
        {"filterDrive", 1.10f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.12f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.52f},
    })});

    // 6. Round vintage bass — smooth triangle pair, minimal drive/resonance.
    factoryPresets.push_back({"Funk Engine", "Bass", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.02f},
        {"mixerDrive", 0.55f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.20f},
        {"filterAttack", 0.010f}, {"filterDecay", 0.35f}, {"filterSustain", 0.20f}, {"filterRelease", 0.20f},
        {"filterDrive", 0.20f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.32f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.18f},
        {"masterVolume", 0.60f},
    })});

    // 7. Punchy short bass — very tight amp/filter envelopes, snappy transient.
    factoryPresets.push_back({"Ladder Punch", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.40f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 600.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.65f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.12f}, {"filterSustain", 0.0f}, {"filterRelease", 0.08f},
        {"filterDrive", 0.45f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.11f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.07f},
        {"masterVolume", 0.58f},
    })});

    // 8. Glide bass — smooth legato slides, sustained tone.
    factoryPresets.push_back({"Deep Voltage", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.40f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 0.90f},
        {"filterCutoff", 550.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.30f},
        {"filterAttack", 0.010f}, {"filterDecay", 0.30f}, {"filterSustain", 0.30f}, {"filterRelease", 0.20f},
        {"filterDrive", 0.35f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.35f}, {"loudnessRelease", 0.18f},
        {"glideEnabled", 1.0f}, {"glideTime", 0.12f}, {"legato", 1.0f},
        {"masterVolume", 0.58f},
    })});

    // 9. Dark cinematic bass — very low cutoff, no keyboard tracking (stays
    // dark across the range), slow evolving envelope.
    factoryPresets.push_back({"Submarine", "Bass", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 1}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 1}, {"osc2Level", 0.30f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 220.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.15f},
        {"filterAttack", 1.2f}, {"filterDecay", 1.0f}, {"filterSustain", 0.60f}, {"filterRelease", 2.0f},
        {"filterDrive", 0.15f}, {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 1.0f}, {"loudnessDecay", 0.8f}, {"loudnessSustain", 0.70f}, {"loudnessRelease", 2.2f},
        {"masterVolume", 0.58f},
    })});

    // 10. Wide detuned bass — larger detune spread for width/thickness, driven.
    factoryPresets.push_back({"Dirty Transistor", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.65f}, {"osc2Detune", 0.15f},
        {"mixerDrive", 1.5f},
        {"filterCutoff", 500.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.35f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.15f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.65f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.18f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.54f},
    })});

    // 11. Rubber bass — bouncy resonant pluck, quick decay from bright to dark.
    factoryPresets.push_back({"Round Machine", "Bass", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.40f},
        {"mixerDrive", 0.80f},
        {"filterCutoff", 650.0f}, {"filterResonance", 0.35f}, {"filterContour", 0.70f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.20f}, {"filterSustain", 0.05f}, {"filterRelease", 0.12f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.19f}, {"loudnessSustain", 0.08f}, {"loudnessRelease", 0.11f},
        {"masterVolume", 0.58f},
    })});

    // 12. Growl bass — aggressive resonant snarl with glide for monstrous slides.
    factoryPresets.push_back({"Glide Monster", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.90f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.55f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 1.8f},
        {"filterCutoff", 500.0f}, {"filterResonance", 0.38f}, {"filterContour", 0.55f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.30f}, {"filterSustain", 0.20f}, {"filterRelease", 0.18f},
        {"filterDrive", 0.90f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.22f}, {"loudnessRelease", 0.16f},
        {"glideEnabled", 1.0f}, {"glideTime", 0.10f}, {"legato", 1.0f},
        {"masterVolume", 0.50f},
    })});

    // 13. Soft warm bass — gentle triangle pair, low resonance, slower attack.
    factoryPresets.push_back({"Heavy Current", "Bass", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 2}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 2}, {"osc2Level", 0.35f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 0.45f},
        {"filterCutoff", 750.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.15f},
        {"filterAttack", 0.020f}, {"filterDecay", 0.35f}, {"filterSustain", 0.35f}, {"filterRelease", 0.25f},
        {"filterDrive", 0.15f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.015f}, {"loudnessDecay", 0.32f}, {"loudnessSustain", 0.40f}, {"loudnessRelease", 0.22f},
        {"masterVolume", 0.60f},
    })});

    // 14. Hard-sync-style bass — this engine has no dedicated sync parameter,
    // so the edge is approximated with a wider osc2 detune + drive to create
    // a harsher, beating, sync-like harmonic character.
    factoryPresets.push_back({"Basement Prophet", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.70f}, {"osc2Detune", 0.18f},
        {"mixerDrive", 1.6f},
        {"filterCutoff", 800.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.50f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.22f}, {"filterSustain", 0.10f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.70f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.20f}, {"loudnessSustain", 0.12f}, {"loudnessRelease", 0.13f},
        {"masterVolume", 0.53f},
    })});

    // 15. Massive performance bass — 3-oscillator stack (saw+square+sub saw)
    // for the biggest low end in the bank, still keyboard-tracked for playability.
    factoryPresets.push_back({"Titan Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.60f}, {"osc2Detune", 0.09f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 1}, {"osc3Level", 0.35f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 1.6f},
        {"filterCutoff", 550.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.45f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.30f}, {"filterSustain", 0.25f}, {"filterRelease", 0.20f},
        {"filterDrive", 0.70f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.28f}, {"loudnessRelease", 0.18f},
        {"masterVolume", 0.50f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // FLAGSHIP — the reference bass. Not a bank addition; a single preset
    // arrived at over 8 measured iterations (spectral/transient analysis,
    // not just clip/NaN safety), meant to be the baseline every future bass
    // preset gets derived from. See conversation history for the full
    // iteration log; summary of the decisions baked in here:
    //   - osc1Range=8' (not 16'): 16' pushed a 2-octave test span down to a
    //     20.6Hz low note — subsonic mud, not a readable bass note.
    //   - osc1+osc2 same-octave detuned unison (not octave-apart layering):
    //     an octave-down osc2 reinforced osc1's harmonic series and added
    //     real weight, but caused ~40ms of phase-interference "swelling" on
    //     the low note before the attack settled. A quiet (0.25 level)
    //     osc3 sub layer instead adds weight without hurting the transient.
    //   - filterContour=0.55: a bigger sweep than the 0.40 first tried,
    //     measurably richer harmonics during the pluck (H2 -8.1dB -> -7.4dB
    //     at the high note) with attack getting faster, not slower.
    //   - mixerDrive/filterDrive settled at a moderate 1.5/0.65 middle
    //     ground: pushing to 1.8/0.80 showed no measurable harmonic-ratio
    //     benefit, so there was no evidence to justify the extra processing.
    //   - masterVolume=0.62 deliberately does NOT use the available
    //     headroom (worst peak measured only 0.26) — matched to the rest of
    //     the Bass bank's loudness so switching presets doesn't jump in
    //     level; "flagship" should come from tone, not being the loudest.
    //   - No glide/legato: situational effects a player enables themselves,
    //     not baked into a general-purpose reference (dedicated glide/acid
    //     basses already exist elsewhere in this category).
    // Verified (offline harness): peak <=0.261, RMS ratio 1.08x across a
    // 3+ octave span (A0-E4), zero clips/NaN anywhere in that range.
    // ══════════════════════════════════════════════════════════════════════

    factoryPresets.push_back({"Flagship", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.05f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.25f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 1.5f},
        {"filterCutoff", 600.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.55f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.30f}, {"filterSustain", 0.18f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.65f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.62f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // LAB - BASS SHOOTOUT (15) — TEMPORARY listening-audition set, not a
    // finished part of the commercial library. Three production-refined
    // versions (A=Conservative, B=Balanced, C=Bold) of each of the five
    // winning concepts from the "LAB - Extreme Bass Exploration" analysis
    // pass (Waveform Clash, Resonance Edge, Dark Vintage, Wide Pulse
    // Extreme, Max Analog Drift). Kept in its own category — not "Bass" —
    // so it doesn't contaminate the shipped library with 15 near-duplicate
    // entries before a winner is picked per concept. Delete this whole
    // block (and, per concept, the two non-chosen versions) once the
    // listening shootout is decided.
    // ══════════════════════════════════════════════════════════════════════

    // --- Concept 1: Waveform Clash (Narrow-pulse + RevSaw) ---
    // Lab used osc1 Narrow@PW0.12 + osc2 RevSaw, no sub, mixerDrive 1.1,
    // filterDrive 0.40 -- harmonically rich but thin/edgy with nothing
    // grounding the low end. Production changes: added a quiet osc3 sub
    // (dropped in the Bold version, where the buzz should dominate
    // unobstructed), backed pulse width off the lab's near-limit 0.12 for
    // A/B (kept for C), and tuned drive/output per version.
    factoryPresets.push_back({"Clash Whisper (A)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.75f}, {"osc1PulseWidth", 0.35f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 3}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.03f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.20f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 0.9f},
        {"filterCutoff", 800.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.30f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.20f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.35f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.60f},
    })});
    factoryPresets.push_back({"Clash Core (B)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.78f}, {"osc1PulseWidth", 0.20f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 3}, {"osc2Range", 3}, {"osc2Level", 0.48f}, {"osc2Detune", 0.04f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.18f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.35f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.20f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.45f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.13f},
        {"masterVolume", 0.58f},
    })});
    factoryPresets.push_back({"Clash Extreme (C)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.80f}, {"osc1PulseWidth", 0.14f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 3}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.4f},
        {"filterCutoff", 1100.0f}, {"filterResonance", 0.35f}, {"filterContour", 0.40f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.25f}, {"filterSustain", 0.18f}, {"filterRelease", 0.12f},
        {"filterDrive", 0.60f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.24f}, {"loudnessSustain", 0.22f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.52f},
    })});

    // --- Concept 2: Resonance Edge ---
    // Lab pushed resonance to 0.92 (untested beyond 3 fixed notes) with an
    // undetuned osc2. Production changes: pulled the ceiling back to 0.85
    // for margin across the whole keyboard/velocity range (not just the
    // lab's 3 test notes), added a small osc2 detune (the lab's exact
    // unison read as slightly sterile/phase-locked), and scaled contour
    // with resonance so the Bold version's peak is actively "ridden" by
    // the envelope rather than just sitting static.
    factoryPresets.push_back({"Resonant Warmth (A)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 0.9f},
        {"filterCutoff", 550.0f}, {"filterResonance", 0.45f}, {"filterContour", 0.30f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.32f}, {"filterSustain", 0.20f}, {"filterRelease", 0.16f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.15f},
        {"masterVolume", 0.60f},
    })});
    factoryPresets.push_back({"Resonant Edge (B)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 550.0f}, {"filterResonance", 0.65f}, {"filterContour", 0.35f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.32f}, {"filterSustain", 0.18f}, {"filterRelease", 0.16f},
        {"filterDrive", 0.35f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.22f}, {"loudnessRelease", 0.15f},
        {"masterVolume", 0.56f},
    })});
    factoryPresets.push_back({"Resonant Scream (C)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.78f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 550.0f}, {"filterResonance", 0.85f}, {"filterContour", 0.45f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.30f}, {"filterSustain", 0.15f}, {"filterRelease", 0.18f},
        {"filterDrive", 0.40f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.18f}, {"loudnessRelease", 0.16f},
        {"masterVolume", 0.50f},
    })});

    // --- Concept 3: Dark Vintage ---
    // Lab version (pure triangle pair, near-zero drive/resonance) was
    // already musical -- the "extreme" here is purity, not harshness, so
    // refinement is about envelope playability and giving three genuinely
    // different intensities rather than fixing anything broken.
    factoryPresets.push_back({"Vintage Whisper (A)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.75f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.30f}, {"osc2Detune", 0.02f},
        {"mixerDrive", 0.25f},
        {"filterCutoff", 300.0f}, {"filterResonance", 0.05f}, {"filterContour", 0.08f},
        {"filterAttack", 0.05f}, {"filterDecay", 0.45f}, {"filterSustain", 0.45f}, {"filterRelease", 0.35f},
        {"filterDrive", 0.08f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.04f}, {"loudnessDecay", 0.40f}, {"loudnessSustain", 0.50f}, {"loudnessRelease", 0.30f},
        {"masterVolume", 0.62f},
    })});
    factoryPresets.push_back({"Vintage Warmth (B)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.78f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 0.35f},
        {"filterCutoff", 400.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.12f},
        {"filterAttack", 0.015f}, {"filterDecay", 0.38f}, {"filterSustain", 0.40f}, {"filterRelease", 0.25f},
        {"filterDrive", 0.12f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.012f}, {"loudnessDecay", 0.34f}, {"loudnessSustain", 0.45f}, {"loudnessRelease", 0.22f},
        {"masterVolume", 0.62f},
    })});
    factoryPresets.push_back({"Vintage Push (C)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.78f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.38f}, {"osc2Detune", 0.03f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.18f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 0.55f},
        {"filterCutoff", 500.0f}, {"filterResonance", 0.14f}, {"filterContour", 0.20f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.32f}, {"filterRelease", 0.18f},
        {"filterDrive", 0.22f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.005f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.35f}, {"loudnessRelease", 0.16f},
        {"masterVolume", 0.60f},
    })});

    // --- Concept 4: Wide Pulse Extreme (Narrow + Wide -- reed/nasal) ---
    // Distinct from Clash: Narrow+Wide (both pulse-capable) instead of
    // Narrow+RevSaw, aimed at a nasal/reedy character rather than Clash's
    // buzzy edge. Production changes mirror Clash's: pulse width backed
    // off the lab extreme for A/B, drive/output balanced per version.
    factoryPresets.push_back({"Reed Whisper (A)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.75f}, {"osc1PulseWidth", 0.35f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 5}, {"osc2Range", 3}, {"osc2Level", 0.30f},
        {"mixerDrive", 0.8f},
        {"filterCutoff", 700.0f}, {"filterResonance", 0.16f}, {"filterContour", 0.25f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.28f}, {"filterSustain", 0.22f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.25f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.28f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.60f},
    })});
    factoryPresets.push_back({"Reed Core (B)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.78f}, {"osc1PulseWidth", 0.22f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 5}, {"osc2Range", 3}, {"osc2Level", 0.35f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 850.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.30f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.20f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.35f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.13f},
        {"masterVolume", 0.57f},
    })});
    factoryPresets.push_back({"Reed Scream (C)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.80f}, {"osc1PulseWidth", 0.14f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 5}, {"osc2Range", 3}, {"osc2Level", 0.42f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 1000.0f}, {"filterResonance", 0.32f}, {"filterContour", 0.38f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.24f}, {"filterSustain", 0.18f}, {"filterRelease", 0.12f},
        {"filterDrive", 0.50f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.22f}, {"loudnessSustain", 0.22f}, {"loudnessRelease", 0.11f},
        {"masterVolume", 0.52f},
    })});

    // --- Concept 5: Max Analog Drift ---
    // Lab maxed drift (1.0 = 10 cents) on a plain saw+saw base and found
    // it reinforced harmonics rather than just detuning. Since drift is
    // inherently subtle, A/B/C vary the drift AMOUNT itself (not just the
    // surrounding patch) so the three versions are genuinely different in
    // how audible the "aliveness" is, not just louder/darker.
    factoryPresets.push_back({"Subtle Drift (A)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.75f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.04f},
        {"analogDrift", 0.35f},
        {"mixerDrive", 0.8f},
        {"filterCutoff", 700.0f}, {"filterResonance", 0.16f}, {"filterContour", 0.22f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.28f}, {"filterSustain", 0.20f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.14f},
        {"masterVolume", 0.60f},
    })});
    factoryPresets.push_back({"Analog Breath (B)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.78f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.05f},
        {"analogDrift", 0.65f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 750.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.25f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.20f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.38f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.25f}, {"loudnessRelease", 0.13f},
        {"masterVolume", 0.57f},
    })});
    factoryPresets.push_back({"Unstable Voltage (C)", "LAB - Bass Shootout", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.78f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.42f}, {"osc2Detune", 0.06f},
        {"analogDrift", 1.0f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 800.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.30f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.18f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.45f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.22f}, {"loudnessRelease", 0.13f},
        {"masterVolume", 0.53f},
    })});

    // ══════════════════════════════════════════════════════════════════════
    // LAB - BASS EXPLORATION 03 (10) — TEMPORARY listening-audition set for
    // this session only. Ten bass IDENTITIES, each built on a different
    // design mechanism (octave/range choice, unison vs. wide-interval
    // tuning, drive-based saturation vs. filter resonance, envelope shape,
    // LFO destination, noise layer, glide) so no two share an oscillator
    // recipe, envelope shape, filter strategy, or mixer balance. Kept in
    // its own category — separate from "Bass" and from "LAB - Bass
    // Shootout" — so it doesn't contaminate either library before an
    // audition decides which (if any) get promoted. Session-isolated per
    // the new LAB workflow: do not merge into Factory Bass, do not edit in
    // a future "LAB - Bass Exploration 0N" session.
    // ══════════════════════════════════════════════════════════════════════

    // 1. 1971 Vintage Moog
    // Musical Identity: The classic three-decade-defining Moog mono bass —
    // warm, punchy sawtooth-square fifth, snapped shut by a fast filter
    // envelope.
    // Sound Design Strategy: Saw + Square at 16', a tight unison detune for
    // that big analog low end, moderate mixer drive for thickness without
    // saturation, a quick filter attack/decay onto a low sustain for the
    // classic percussive "honk", and full keyboard tracking so the tone
    // stays consistent across the range. This is the reference archetype
    // every other preset in this set deliberately breaks from.
    factoryPresets.push_back({"1971 Vintage Moog", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.50f}, {"osc2Detune", 0.05f},
        {"mixerDrive", 1.2f},
        {"filterCutoff", 450.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.45f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.28f}, {"filterSustain", 0.10f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.40f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.22f}, {"loudnessSustain", 0.15f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.60f},
    })});

    // 2. Deep Sub Foundation
    // Musical Identity: A pure, room-shaking sub-bass foundation engineered
    // for maximum low-end weight, not character.
    // Sound Design Strategy: Twin triangles at LO and 32' ranges (true
    // sub-generator territory, not detune-based unison), almost no drive
    // or filter resonance/contour so the fundamental is never disturbed,
    // filter keyboard tracking OFF so the cutoff never rises and thins the
    // low end, and slow envelopes with a very high sustain so the sub
    // holds rock-steady under a note.
    factoryPresets.push_back({"Deep Sub Foundation", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 0}, {"osc1Level", 0.95f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 1}, {"osc2Level", 0.45f},
        {"mixerDrive", 0.50f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.0f}, {"filterContour", 0.05f},
        {"filterAttack", 0.01f}, {"filterDecay", 0.50f}, {"filterSustain", 0.90f}, {"filterRelease", 0.40f},
        {"filterDrive", 0.05f}, {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.01f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.95f}, {"loudnessRelease", 0.35f},
        {"masterVolume", 0.65f},
    })});

    // 3. Funk Snap
    // Musical Identity: A percussive, plucky funk bass whose entire
    // character comes from envelope speed and filter snap, not sustained
    // tone.
    // Sound Design Strategy: Saw + Square at 8' with a small beating
    // detune, a resonance-and-contour peak fired by an almost-instant
    // filter attack/decay to zero sustain, and a matching amp envelope
    // that dies just as fast — the note is over before it can sustain,
    // like a slapped string.
    factoryPresets.push_back({"Funk Snap", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.50f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 1200.0f}, {"filterResonance", 0.55f}, {"filterContour", 0.65f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.09f}, {"filterSustain", 0.0f}, {"filterRelease", 0.05f},
        {"filterDrive", 0.50f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.12f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.06f},
        {"masterVolume", 0.62f},
    })});

    // 4. Acid Bite
    // Musical Identity: A single-oscillator TB-303-style acid line built
    // for squelchy sequenced runs, not chords.
    // Sound Design Strategy: One saw oscillator only (no second voice at
    // all — the defining mechanical difference from every other preset
    // here), resonance pushed near the edge with a high contour and a
    // mid-fast filter decay for the classic acid sweep, glide enabled for
    // slides between notes, and high filter drive for grit.
    factoryPresets.push_back({"Acid Bite", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.90f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 300.0f}, {"filterResonance", 0.82f}, {"filterContour", 0.75f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.22f}, {"filterSustain", 0.05f}, {"filterRelease", 0.10f},
        {"filterDrive", 0.90f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.30f}, {"loudnessSustain", 0.60f}, {"loudnessRelease", 0.15f},
        {"glideEnabled", 1.0f}, {"glideTime", 0.08f},
        {"masterVolume", 0.55f},
    })});

    // 5. Dirty Drive
    // Musical Identity: A saturation-first bass where distortion, not
    // filter shaping, defines the sound.
    // Sound Design Strategy: Mixer drive and filter drive both pushed hard
    // into overdrive, resonance kept moderate so drive stays the star, a
    // dense saw+saw base to feed the saturation stages, and a sustained
    // envelope so the grit is heard through the whole note rather than
    // just the transient. Master volume pulled down to compensate for the
    // gain the drive stages add.
    factoryPresets.push_back({"Dirty Drive", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.60f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 2.6f},
        {"filterCutoff", 700.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.30f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.25f}, {"filterSustain", 0.50f}, {"filterRelease", 0.20f},
        {"filterDrive", 2.2f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.20f}, {"loudnessSustain", 0.70f}, {"loudnessRelease", 0.18f},
        {"masterVolume", 0.45f},
    })});

    // 6. Dark Cinematic
    // Musical Identity: A slow-evolving, brooding drone-bass for cinematic
    // low-end tension, not rhythmic playing.
    // Sound Design Strategy: A dark cutoff with long attack/release on both
    // filter and amp so the note swells in rather than hits, half keyboard
    // tracking so it stays dark across the range, a slow LFO routed to the
    // filter for a subtle breathing motion, and octave-spread
    // triangle/tri-saw oscillators for a foundation-plus-air texture with
    // no edge or drive.
    factoryPresets.push_back({"Dark Cinematic", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 2}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 1}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.02f},
        {"mixerDrive", 0.60f},
        {"filterCutoff", 220.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.35f},
        {"filterAttack", 0.80f}, {"filterDecay", 1.5f}, {"filterSustain", 0.70f}, {"filterRelease", 1.8f},
        {"filterDrive", 0.15f}, {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.60f}, {"loudnessDecay", 1.0f}, {"loudnessSustain", 0.85f}, {"loudnessRelease", 2.0f},
        {"lfoRate", 0.30f}, {"lfoAmount", 0.25f}, {"lfoDestination", 1.0f},
        {"masterVolume", 0.55f},
    })});

    // 7. Rubber Bass
    // Musical Identity: A bouncy, muted "rubber-band" bass with a
    // pronounced filter-envelope boing on every note.
    // Sound Design Strategy: Round narrow-pulse and triangle waveforms for
    // a soft, edge-free timbre, a deep filter contour that snaps quickly
    // down to a much lower sustain, and a filter decay noticeably longer
    // than the amp decay so the "boing" is still audible after the
    // loudness has settled — a different envelope-timing mechanism from
    // Funk Snap's synchronized instant decay.
    factoryPresets.push_back({"Rubber Bass", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 3}, {"osc1Level", 0.75f}, {"osc1PulseWidth", 0.30f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.40f},
        {"mixerDrive", 0.90f},
        {"filterCutoff", 350.0f}, {"filterResonance", 0.40f}, {"filterContour", 0.70f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.35f}, {"filterSustain", 0.15f}, {"filterRelease", 0.20f},
        {"filterDrive", 0.30f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.15f}, {"loudnessSustain", 0.55f}, {"loudnessRelease", 0.18f},
        {"masterVolume", 0.60f},
    })});

    // 8. Aggressive Modern
    // Musical Identity: A modern EDM/trap-style bass built on wide unison
    // and aggressive filter movement for maximum in-your-face energy.
    // Sound Design Strategy: A large osc2 detune for audible beating width
    // plus a third voice pitched an octave down for sub weight, heavy
    // mixer and filter drive, high resonance and contour for movement, and
    // a fast attack into a punchy mid-decay sustain. Master volume pushed
    // up for loudness rather than pulled back like the driven-but-warm
    // Dirty Drive preset.
    factoryPresets.push_back({"Aggressive Modern", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.65f}, {"osc2Detune", 0.25f},
        {"osc3Enabled", 1.0f}, {"osc3Waveform", 2}, {"osc3Range", 2}, {"osc3Level", 0.35f}, {"osc3KeyboardTracking", 1.0f},
        {"mixerDrive", 1.8f},
        {"filterCutoff", 1500.0f}, {"filterResonance", 0.50f}, {"filterContour", 0.55f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.18f}, {"filterSustain", 0.40f}, {"filterRelease", 0.10f},
        {"filterDrive", 1.2f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.15f}, {"loudnessSustain", 0.60f}, {"loudnessRelease", 0.10f},
        {"masterVolume", 0.65f},
    })});

    // 9. Warm Vintage
    // Musical Identity: A mellow, chorus-like vintage bass built on wide
    // triangle detuning for soft analog warmth rather than Moog's punchy
    // saw/square growl.
    // Sound Design Strategy: Deliberately distinct from "1971 Vintage
    // Moog" — two triangle oscillators with a wide, slow-beating detune
    // for chorus warmth instead of saw+square unison, essentially no
    // filter resonance or contour, and slower, rounder attack/decay curves
    // for a soft Rhodes-bass-like character with no punch or edge at all.
    factoryPresets.push_back({"Warm Vintage", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.80f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.12f},
        {"mixerDrive", 0.40f},
        {"filterCutoff", 800.0f}, {"filterResonance", 0.05f}, {"filterContour", 0.10f},
        {"filterAttack", 0.03f}, {"filterDecay", 0.50f}, {"filterSustain", 0.60f}, {"filterRelease", 0.40f},
        {"filterDrive", 0.10f}, {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.02f}, {"loudnessDecay", 0.40f}, {"loudnessSustain", 0.65f}, {"loudnessRelease", 0.35f},
        {"masterVolume", 0.62f},
    })});

    // 10. Experimental Analog
    // Musical Identity: An unconventional interval-tuned bass where the
    // two oscillators sit a wide interval apart rather than in unison,
    // creating an inherently unstable, alien low end.
    // Sound Design Strategy: Osc1 in the LO sub range against osc2 two
    // octaves up at 4' — a wide-interval mechanism instead of detune-based
    // unison — paired with clashing narrow-pulse and reverse-saw
    // waveforms, a touch of noise for grit, an LFO routed to PITCH (not
    // filter, unlike Dark Cinematic) for an unstable wobble, and filter
    // keyboard tracking off so the tone behaves unpredictably across the
    // range.
    factoryPresets.push_back({"Experimental Analog", "LAB - Bass Exploration 03", makePreset({
        {"osc1Waveform", 6}, {"osc1Range", 0}, {"osc1Level", 0.70f}, {"osc1PulseWidth", 0.20f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 3}, {"osc2Range", 4}, {"osc2Level", 0.45f},
        {"noiseLevel", 0.12f}, {"noiseMode", 0.0f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 650.0f}, {"filterResonance", 0.35f}, {"filterContour", 0.30f},
        {"filterAttack", 0.01f}, {"filterDecay", 0.30f}, {"filterSustain", 0.35f}, {"filterRelease", 0.25f},
        {"filterDrive", 0.40f}, {"filterKeyboardTracking", 0.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.50f}, {"loudnessRelease", 0.22f},
        {"lfoRate", 4.5f}, {"lfoAmount", 0.15f}, {"lfoDestination", 0.0f},
        {"masterVolume", 0.55f},
    })});
}

juce::File PresetManager::getUserPresetFolder() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("Ladder Voice")
           .getChildFile("Presets")
           .getChildFile("User");
}

void PresetManager::refreshUserPresets()
{
    userPresetFiles.clear();
    auto folder = getUserPresetFolder();
    if (folder.exists()) {
        for (auto& f : folder.findChildFiles(juce::File::findFiles, false, "*.ladderpreset"))
            userPresetFiles.push_back(f);
        std::sort(userPresetFiles.begin(), userPresetFiles.end(),
                  [](const juce::File& a, const juce::File& b) {
                      return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
                  });
    }
}

juce::String PresetManager::getUserPresetName(int i) const
{
    if (i < 0 || i >= (int)userPresetFiles.size()) return {};
    return userPresetFiles[(size_t)i].getFileNameWithoutExtension();
}

juce::String PresetManager::getUserPresetCategory(int i) const
{
    if (i < 0 || i >= (int)userPresetFiles.size()) return "User";
    auto xml = juce::XmlDocument::parse(userPresetFiles[(size_t)i]);
    if (xml == nullptr) return "User";
    const auto cat = xml->getStringAttribute("category", "User").trim();
    return cat.isEmpty() ? "User" : cat;
}

void PresetManager::applyPreset(const PresetData& p, juce::AudioProcessorValueTreeState& apvts) const
{
    for (auto& [id, val] : p.params) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    }
    // Always force analogDrift to 0
    if (auto* drift = apvts.getParameter("analogDrift"))
        drift->setValueNotifyingHost(drift->convertTo0to1(0.0f));
}

bool PresetManager::loadUserPreset(int index, juce::AudioProcessorValueTreeState& apvts)
{
    if (index < 0 || index >= (int)userPresetFiles.size()) return false;
    return loadPresetFromFile(userPresetFiles[(size_t)index], apvts);
}

bool PresetManager::loadPresetFromFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts) const
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || xml->getTagName() != "LadderVoicePreset")
        return false;

    auto preset = parsePresetXml(*xml);

    for (auto& [id, val] : preset.params) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    }
    // Force analogDrift to 0 regardless of file contents
    if (auto* drift = apvts.getParameter("analogDrift"))
        drift->setValueNotifyingHost(drift->convertTo0to1(0.0f));

    return true;
}

juce::File PresetManager::userPresetFile(const juce::String& name, const juce::String& category) const
{
    const auto filename = sanitizeFilename(category) + "_" + sanitizeFilename(name) + ".ladderpreset";
    return getUserPresetFolder().getChildFile(filename);
}

bool PresetManager::userPresetExists(const juce::String& name, const juce::String& category) const
{
    return userPresetFile(name, category).existsAsFile();
}

juce::File PresetManager::saveUserPreset(const juce::String& name, const juce::String& category,
                                         juce::AudioProcessorValueTreeState& apvts)
{
    auto folder = getUserPresetFolder();
    folder.createDirectory();

    auto xml = presetToXml(name, category, apvts);
    auto file = userPresetFile(name, category);
    xml->writeTo(file);

    refreshUserPresets();
    return file;
}

bool PresetManager::deleteUserPreset(int index)
{
    if (index < 0 || index >= (int)userPresetFiles.size()) return false;
    bool ok = userPresetFiles[(size_t)index].deleteFile();
    if (ok) refreshUserPresets();
    return ok;
}

void PresetManager::loadPresetAtIndex(int combinedIndex, juce::AudioProcessorValueTreeState& apvts)
{
    const int numFactory = getNumFactoryPresets();
    if (combinedIndex < numFactory) {
        applyPreset(factoryPresets[(size_t)combinedIndex], apvts);
        currentPresetName = factoryPresets[(size_t)combinedIndex].name;
    } else {
        int userIndex = combinedIndex - numFactory;
        if (loadUserPreset(userIndex, apvts))
            currentPresetName = getUserPresetName(userIndex);
    }
    currentCombinedIndex = combinedIndex;
}

juce::String PresetManager::getPresetNameAtIndex(int combinedIndex) const
{
    const int numFactory = getNumFactoryPresets();
    if (combinedIndex < numFactory)
        return factoryPresets[(size_t)combinedIndex].name;
    int userIndex = combinedIndex - numFactory;
    return getUserPresetName(userIndex);
}

juce::String PresetManager::sanitizeFilename(const juce::String& name)
{
    juce::String safe;
    for (auto c : name) {
        if (juce::CharacterFunctions::isLetterOrDigit(c) || c == ' ' || c == '-' || c == '_')
            safe += c;
        else
            safe += '_';
    }
    return safe.trim().replaceCharacter(' ', '_');
}

PresetData PresetManager::parsePresetXml(const juce::XmlElement& xml)
{
    PresetData data;
    data.name     = xml.getStringAttribute("name", "User Preset");
    data.category = xml.getStringAttribute("category", "User");

    forEachXmlChildElementWithTagName(xml, child, "PARAM") {
        auto id  = child->getStringAttribute("id");
        auto val = (float)child->getDoubleAttribute("value", 0.0);
        if (id.isNotEmpty())
            data.params.push_back({id, val});
    }
    return data;
}

std::unique_ptr<juce::XmlElement> PresetManager::presetToXml(const juce::String& name,
                                                              const juce::String& category,
                                                              juce::AudioProcessorValueTreeState& apvts)
{
    auto xml = std::make_unique<juce::XmlElement>("LadderVoicePreset");
    xml->setAttribute("name",     name);
    xml->setAttribute("category", category);
    xml->setAttribute("version",  "1");

    for (const char* const* id = kAllParamIds; *id != nullptr; ++id) {
        if (juce::String(*id) == "analogDrift") continue; // never persist drift
        if (auto* raw = apvts.getRawParameterValue(*id)) {
            auto* child = xml->createNewChildElement("PARAM");
            child->setAttribute("id",    *id);
            child->setAttribute("value", (double)raw->load());
        }
    }
    return xml;
}
