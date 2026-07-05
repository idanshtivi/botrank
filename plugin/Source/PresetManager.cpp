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

    // ─── THICK BASSES (mono, filter tracks keyboard) ───────────────────────
    factoryPresets.push_back({"Fathom Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 1}, {"osc1Level", 0.90f},          // Saw, 32'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 2}, {"osc2Level", 0.60f}, {"osc2Detune", 0.03f}, // Square, 16'
        {"mixerDrive", 1.6f},
        {"filterCutoff", 380.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.55f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.32f}, {"filterSustain", 0.05f}, {"filterRelease", 0.15f},
        {"filterDrive", 0.6f}, {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.15f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Analog Growl Bass", "Bass", makePreset({
        {"osc1Waveform", 5}, {"osc1Range", 2}, {"osc1Level", 1.0f}, {"osc1PulseWidth", 0.35f}, // Wide pulse, 16'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.25f},     // Saw, 8'
        {"mixerDrive", 1.8f},
        {"filterCutoff", 420.0f}, {"filterResonance", 0.38f}, {"filterContour", 0.45f},
        {"filterAttack", 0.003f}, {"filterDecay", 0.30f}, {"filterSustain", 0.0f}, {"filterRelease", 0.14f},
        {"filterDrive", 0.9f}, {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.003f}, {"loudnessDecay", 0.26f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Vintage Ladder Bass", "Bass", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.85f},          // Saw, 16'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.55f}, {"osc2Detune", 0.06f}, // Saw, 16'
        {"mixerDrive", 1.4f},
        {"filterCutoff", 520.0f}, {"filterResonance", 0.16f}, {"filterContour", 0.30f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.45f}, {"filterSustain", 0.20f}, {"filterRelease", 0.20f},
        {"filterDrive", 0.5f}, {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.004f}, {"loudnessDecay", 0.35f}, {"loudnessSustain", 0.30f}, {"loudnessRelease", 0.18f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Sub Foundation Bass", "Bass", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 1}, {"osc1Level", 0.90f},          // Triangle, 32'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 2}, {"osc2Level", 0.35f},     // Saw, 16'
        {"mixerDrive", 1.0f},
        {"filterCutoff", 340.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.15f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.50f}, {"filterSustain", 0.40f}, {"filterRelease", 0.30f},
        {"filterDrive", 0.3f}, {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.006f}, {"loudnessDecay", 0.40f}, {"loudnessSustain", 0.55f}, {"loudnessRelease", 0.28f},
        {"masterVolume", 0.62f},
    })});

    // ─── CLASSIC SOLO LEADS (mono) ──────────────────────────────────────────
    factoryPresets.push_back({"Singing Saw Lead", "Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 6500.0f}, {"filterResonance", 0.28f}, {"filterContour", 0.18f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.35f}, {"filterSustain", 0.65f}, {"filterRelease", 0.28f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.22f},
        {"pitchBendRange", 2.0f}, {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Analog Screamer Lead", "Lead", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},          // Square, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.35f}, {"osc2Detune", 0.08f}, // Square, 4'
        {"mixerDrive", 1.6f},
        {"filterCutoff", 5200.0f}, {"filterResonance", 0.48f}, {"filterContour", 0.25f},
        {"filterAttack", 0.004f}, {"filterDecay", 0.30f}, {"filterSustain", 0.55f}, {"filterRelease", 0.25f},
        {"filterDrive", 0.9f}, {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.005f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Classic Unison Lead", "Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.15f},
        {"mixerDrive", 1.2f},
        {"filterCutoff", 6000.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.15f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.70f}, {"filterRelease", 0.28f},
        {"filterKeyboardTracking", 2.0f},
        {"loudnessAttack", 0.006f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.25f},
        {"pitchBendRange", 7.0f}, {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Warm Reed Lead", "Lead", makePreset({
        {"osc1Waveform", 1}, {"osc1Range", 3}, {"osc1Level", 0.85f},          // TriangleSaw, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.04f}, // Triangle, 8'
        {"mixerDrive", 0.8f},
        {"filterCutoff", 3200.0f}, {"filterResonance", 0.14f}, {"filterContour", 0.12f},
        {"filterAttack", 0.03f}, {"filterDecay", 0.30f}, {"filterSustain", 0.75f}, {"filterRelease", 0.35f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.025f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.30f},
        {"masterVolume", 0.60f},
    })});

    // ─── VINTAGE BRASS (poly 4) ─────────────────────────────────────────────
    factoryPresets.push_back({"Analog Horn Section", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.75f},          // Square, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.05f}, // Saw, 8'
        {"mixerDrive", 1.1f},
        {"filterCutoff", 2600.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.55f},
        {"filterAttack", 0.06f}, {"filterDecay", 0.25f}, {"filterSustain", 0.65f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.03f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.28f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Vintage Brass Ensemble", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.70f},          // Saw, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.06f}, // Square, 8'
        {"mixerDrive", 1.0f},
        {"filterCutoff", 2200.0f}, {"filterResonance", 0.16f}, {"filterContour", 0.60f},
        {"filterAttack", 0.09f}, {"filterDecay", 0.30f}, {"filterSustain", 0.70f}, {"filterRelease", 0.35f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.05f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.32f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Punchy Brass Stab", "Brass", makePreset({
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

    factoryPresets.push_back({"Mellow Brass Pad", "Brass", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 1}, {"osc1Range", 3}, {"osc1Level", 0.70f},          // TriangleSaw, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.05f}, // Saw, 8'
        {"mixerDrive", 0.8f},
        {"filterCutoff", 1900.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.35f},
        {"filterAttack", 0.15f}, {"filterDecay", 0.30f}, {"filterSustain", 0.75f}, {"filterRelease", 0.45f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.12f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.45f},
        {"masterVolume", 0.55f},
    })});

    // ─── EXPRESSIVE GLIDE LEADS (mono, legato + glide on) ──────────────────
    factoryPresets.push_back({"Slide Lead", "Glide Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.40f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 5000.0f}, {"filterResonance", 0.22f}, {"filterContour", 0.15f},
        {"filterAttack", 0.006f}, {"filterDecay", 0.30f}, {"filterSustain", 0.65f}, {"filterRelease", 0.25f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.008f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.22f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.12f},
        {"pitchBendRange", 2.0f}, {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Portamento Solo", "Glide Lead", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.85f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.30f}, {"osc2Detune", 0.03f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 4200.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.15f},
        {"filterAttack", 0.008f}, {"filterDecay", 0.30f}, {"filterSustain", 0.70f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.01f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.28f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.25f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Fluid Mono Lead", "Glide Lead", makePreset({
        {"osc1Waveform", 1}, {"osc1Range", 3}, {"osc1Level", 0.90f},          // TriangleSaw, 8'
        {"osc2Enabled", 0.0f},
        {"mixerDrive", 0.9f},
        {"filterCutoff", 5500.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.12f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.25f}, {"filterSustain", 0.70f}, {"filterRelease", 0.22f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.006f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.20f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.08f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Synth Whistle Glide", "Glide Lead", makePreset({
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.95f},          // Triangle, 8'
        {"osc2Enabled", 0.0f},
        {"mixerDrive", 0.6f},
        {"filterCutoff", 2600.0f}, {"filterResonance", 0.06f}, {"filterContour", 0.08f},
        {"filterAttack", 0.02f}, {"filterDecay", 0.20f}, {"filterSustain", 0.80f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.02f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.35f},
        {"legato", 1.0f}, {"retrigger", 0.0f}, {"glideEnabled", 1.0f}, {"glideTime", 0.35f},
        {"masterVolume", 0.62f},
    })});

    // ─── WARM ANALOG KEYS (poly 4) ──────────────────────────────────────────
    factoryPresets.push_back({"Warm Analog Piano", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.75f},          // Triangle, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.03f}, // Saw, 8'
        {"mixerDrive", 0.9f},
        {"filterCutoff", 3200.0f}, {"filterResonance", 0.15f}, {"filterContour", 0.40f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.40f}, {"filterSustain", 0.35f}, {"filterRelease", 0.30f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.55f}, {"loudnessSustain", 0.55f}, {"loudnessRelease", 0.35f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Velvet Keys", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 1}, {"osc1Range", 3}, {"osc1Level", 0.75f},          // TriangleSaw, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.35f}, {"osc2Detune", 0.05f}, // Triangle, 8'
        {"mixerDrive", 0.7f},
        {"filterCutoff", 2400.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.25f},
        {"filterAttack", 0.04f}, {"filterDecay", 0.35f}, {"filterSustain", 0.65f}, {"filterRelease", 0.40f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.03f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.40f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Classic Clav Keys", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.75f},          // Square, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.35f},                        // Square, 4'
        {"mixerDrive", 1.2f},
        {"filterCutoff", 3500.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.55f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.20f}, {"filterSustain", 0.15f}, {"filterRelease", 0.15f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.25f}, {"loudnessSustain", 0.20f}, {"loudnessRelease", 0.15f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Analog Organ Keys", "Keys", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.65f},          // Square, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 4}, {"osc2Level", 0.40f},                        // Square, 4'
        {"mixerDrive", 0.8f},
        {"filterCutoff", 4500.0f}, {"filterResonance", 0.08f}, {"filterContour", 0.05f},
        {"filterAttack", 0.002f}, {"filterDecay", 0.10f}, {"filterSustain", 1.0f}, {"filterRelease", 0.10f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.002f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.10f},
        {"masterVolume", 0.58f},
    })});

    // ─── SHORT PLUCKS (2 mono, 2 poly) ──────────────────────────────────────
    factoryPresets.push_back({"Analog Pluck", "Pluck", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 1.0f},
        {"osc2Enabled", 0.0f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 4800.0f}, {"filterResonance", 0.25f}, {"filterContour", 0.65f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.15f}, {"filterSustain", 0.0f}, {"filterRelease", 0.10f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.16f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.10f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Muted Pluck Bass", "Pluck", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 1.0f},          // Saw, 16'
        {"osc2Enabled", 0.0f},
        {"mixerDrive", 1.3f},
        {"filterCutoff", 1200.0f}, {"filterResonance", 0.18f}, {"filterContour", 0.55f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.10f}, {"filterSustain", 0.0f}, {"filterRelease", 0.06f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.10f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.06f},
        {"masterVolume", 0.60f},
    })});

    factoryPresets.push_back({"Poly Pluck Keys", "Pluck", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.75f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.07f},
        {"mixerDrive", 0.9f},
        {"filterCutoff", 4200.0f}, {"filterResonance", 0.20f}, {"filterContour", 0.50f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.20f}, {"filterSustain", 0.0f}, {"filterRelease", 0.12f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.22f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.12f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Bright Pluck Bell", "Pluck", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.65f},          // Square, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 5}, {"osc2Level", 0.40f},                        // Triangle, 2'
        {"mixerDrive", 0.9f},
        {"filterCutoff", 7000.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.75f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.25f}, {"filterSustain", 0.0f}, {"filterRelease", 0.15f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.001f}, {"loudnessDecay", 0.28f}, {"loudnessSustain", 0.0f}, {"loudnessRelease", 0.15f},
        {"masterVolume", 0.55f},
    })});

    // ─── VINTAGE PADS (poly 4, slow attack, long release) ──────────────────
    factoryPresets.push_back({"Slow Analog Pad", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.65f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.55f}, {"osc2Detune", 0.12f},
        {"mixerDrive", 0.7f},
        {"filterCutoff", 2800.0f}, {"filterResonance", 0.12f}, {"filterContour", 0.20f},
        {"filterAttack", 0.7f}, {"filterDecay", 0.6f}, {"filterSustain", 0.8f}, {"filterRelease", 1.5f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.8f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.8f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Warm String Pad", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 4}, {"osc2Level", 0.35f}, {"osc2Detune", 0.08f}, // Saw, 4'
        {"mixerDrive", 0.7f},
        {"filterCutoff", 2400.0f}, {"filterResonance", 0.10f}, {"filterContour", 0.15f},
        {"filterAttack", 0.5f}, {"filterDecay", 0.5f}, {"filterSustain", 0.85f}, {"filterRelease", 1.2f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.5f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.4f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Misty Triangle Pad", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 0}, {"osc1Range", 3}, {"osc1Level", 0.65f},          // Triangle, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 0}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.10f},
        {"mixerDrive", 0.5f},
        {"filterCutoff", 1800.0f}, {"filterResonance", 0.06f}, {"filterContour", 0.10f},
        {"filterAttack", 0.9f}, {"filterDecay", 0.5f}, {"filterSustain", 0.85f}, {"filterRelease", 1.8f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 1.0f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 2.0f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Analog Choir Pad", "Pad", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.55f},          // Saw, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.09f}, // Square, 8'
        {"mixerDrive", 0.7f},
        {"filterCutoff", 2200.0f}, {"filterResonance", 0.14f}, {"filterContour", 0.45f},
        {"filterAttack", 0.6f}, {"filterDecay", 0.7f}, {"filterSustain", 0.75f}, {"filterRelease", 1.4f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.6f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.5f},
        {"masterVolume", 0.55f},
    })});

    // ─── CLASSIC FILTER SWEEPS ───────────────────────────────────────────────
    factoryPresets.push_back({"Rising Filter Sweep", "Sweep", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.70f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 4}, {"osc2Range", 3}, {"osc2Level", 0.45f}, {"osc2Detune", 0.06f},
        {"mixerDrive", 1.0f},
        {"filterCutoff", 200.0f}, {"filterResonance", 0.35f}, {"filterContour", 1.0f},
        {"filterAttack", 2.5f}, {"filterDecay", 0.8f}, {"filterSustain", 0.6f}, {"filterRelease", 0.8f},
        {"filterKeyboardTracking", 0.5f},
        {"loudnessAttack", 0.02f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.6f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Resonant Sweep Bass", "Sweep", makePreset({
        {"osc1Waveform", 2}, {"osc1Range", 2}, {"osc1Level", 0.90f},          // Saw, 16'
        {"osc2Enabled", 0.0f},
        {"mixerDrive", 1.2f},
        {"filterCutoff", 150.0f}, {"filterResonance", 0.55f}, {"filterContour", 1.0f},
        {"filterAttack", 1.8f}, {"filterDecay", 0.5f}, {"filterSustain", 0.4f}, {"filterRelease", 0.6f},
        {"filterKeyboardTracking", 0.5f},
        {"loudnessAttack", 0.01f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.5f},
        {"masterVolume", 0.55f},
    })});

    factoryPresets.push_back({"Vintage Auto-Wah Sweep", "Sweep", makePreset({
        {"osc1Waveform", 4}, {"osc1Range", 3}, {"osc1Level", 0.85f},          // Square, 8'
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 3}, {"osc2Level", 0.30f}, {"osc2Detune", 0.04f},
        {"mixerDrive", 1.1f},
        {"filterCutoff", 3500.0f}, {"filterResonance", 0.30f}, {"filterContour", 0.75f},
        {"filterAttack", 0.005f}, {"filterDecay", 1.2f}, {"filterSustain", 0.15f}, {"filterRelease", 0.5f},
        {"filterKeyboardTracking", 1.0f},
        {"loudnessAttack", 0.005f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 0.5f},
        {"masterVolume", 0.58f},
    })});

    factoryPresets.push_back({"Emphasis Riser Sweep", "Sweep", makePreset({
        {"playMode", 1.0f},
        {"osc1Waveform", 2}, {"osc1Range", 3}, {"osc1Level", 0.60f},
        {"osc2Enabled", 1.0f}, {"osc2Waveform", 2}, {"osc2Range", 4}, {"osc2Level", 0.45f}, {"osc2Detune", 0.10f}, // Saw, 4'
        {"mixerDrive", 0.9f},
        {"filterCutoff", 150.0f}, {"filterResonance", 0.40f}, {"filterContour", 1.0f},
        {"filterAttack", 4.0f}, {"filterDecay", 0.8f}, {"filterSustain", 0.7f}, {"filterRelease", 1.0f},
        {"filterKeyboardTracking", 0.3f},
        {"loudnessAttack", 0.3f}, {"loudnessDecay", 0.0f}, {"loudnessSustain", 1.0f}, {"loudnessRelease", 1.0f},
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

juce::File PresetManager::saveUserPreset(const juce::String& name, const juce::String& category,
                                         juce::AudioProcessorValueTreeState& apvts)
{
    auto folder = getUserPresetFolder();
    folder.createDirectory();

    auto xml = presetToXml(name, category, apvts);
    auto filename = sanitizeFilename(name) + ".ladderpreset";
    auto file = folder.getChildFile(filename);
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
