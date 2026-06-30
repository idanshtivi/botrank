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

    // 1 – Modern Solid Bass
    factoryPresets.push_back({"Modern Solid Bass", "Bass", makePreset({
        {"osc1Waveform", 2},   // Saw
        {"osc1Range",    2},   // 16'
        {"osc1Level",    1.0f},
        {"osc2Enabled",  0.0f},
        {"mixerDrive",   1.8f},
        {"filterCutoff", 900.0f}, {"filterResonance", 0.18f},
        {"filterContour",0.35f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.35f}, {"filterSustain", 0.0f}, {"filterRelease", 0.18f},
        {"loudnessAttack",0.005f},{"loudnessDecay",0.22f},{"loudnessSustain",0.0f},{"loudnessRelease",0.12f},
        {"filterKeyboardTracking", 1.0f},
    })});

    // 2 – Fat Sub Drive Bass
    factoryPresets.push_back({"Fat Sub Drive Bass", "Bass", makePreset({
        {"osc1Waveform", 2},   // Saw
        {"osc1Range",    2},   // 16'
        {"osc1Level",    0.80f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 4},   // Square
        {"osc2Range",    1},   // 32'
        {"osc2Level",    0.55f},
        {"osc2Detune",   0.0f},
        {"mixerDrive",   2.2f},
        {"filterCutoff", 650.0f}, {"filterResonance", 0.10f},
        {"filterContour",0.25f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.55f}, {"filterSustain", 0.0f}, {"filterRelease", 0.20f},
        {"loudnessAttack",0.005f},{"loudnessDecay",0.40f},{"loudnessSustain",0.0f},{"loudnessRelease",0.18f},
        {"filterDrive",  1.0f},
    })});

    // 3 – Dark Techno Bass
    factoryPresets.push_back({"Dark Techno Bass", "Bass", makePreset({
        {"osc1Waveform", 5},   // Wide pulse
        {"osc1Range",    2},   // 16'
        {"osc1Level",    1.0f},
        {"osc2Enabled",  0.0f},
        {"osc1PulseWidth",0.30f},
        {"mixerDrive",   2.0f},
        {"filterCutoff", 480.0f}, {"filterResonance", 0.22f},
        {"filterContour",0.40f},
        {"filterAttack", 0.005f}, {"filterDecay", 0.28f}, {"filterSustain", 0.0f}, {"filterRelease", 0.15f},
        {"loudnessAttack",0.005f},{"loudnessDecay",0.30f},{"loudnessSustain",0.0f},{"loudnessRelease",0.12f},
        {"filterDrive",  0.80f},
        {"filterKeyboardTracking", 1.0f},
    })});

    // 4 – Bright Modern Lead
    factoryPresets.push_back({"Bright Modern Lead", "Lead", makePreset({
        {"osc1Waveform", 2},   // Saw
        {"osc1Range",    3},   // 8'
        {"osc1Level",    0.85f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 2},
        {"osc2Range",    3},
        {"osc2Level",    0.35f},
        {"osc2Detune",   0.08f},
        {"mixerDrive",   1.2f},
        {"filterCutoff", 7000.0f}, {"filterResonance", 0.30f},
        {"filterContour",0.20f},
        {"filterAttack", 0.008f}, {"filterDecay", 0.40f}, {"filterSustain", 0.60f}, {"filterRelease", 0.30f},
        {"loudnessAttack",0.010f},{"loudnessDecay",0.0f},{"loudnessSustain",1.0f},{"loudnessRelease",0.25f},
        {"filterKeyboardTracking", 2.0f},
        {"pitchBendRange", 7.0f},
        {"glideEnabled",  0.0f},
    })});

    // 5 – Soft Analog Lead
    factoryPresets.push_back({"Soft Analog Lead", "Lead", makePreset({
        {"osc1Waveform", 0},   // Tri
        {"osc1Range",    3},   // 8'
        {"osc1Level",    0.90f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 0},
        {"osc2Range",    3},
        {"osc2Level",    0.40f},
        {"osc2Detune",   0.04f},
        {"mixerDrive",   0.8f},
        {"filterCutoff", 4200.0f}, {"filterResonance", 0.15f},
        {"filterContour",0.10f},
        {"filterAttack", 0.05f}, {"filterDecay", 0.30f}, {"filterSustain", 0.80f}, {"filterRelease", 0.40f},
        {"loudnessAttack",0.04f},{"loudnessDecay",0.0f},{"loudnessSustain",1.0f},{"loudnessRelease",0.35f},
        {"filterKeyboardTracking", 1.0f},
        {"glideEnabled",  1.0f}, {"glideTime", 0.10f},
        {"pitchBendRange", 2.0f},
    })});

    // 6 – Short Pluck
    factoryPresets.push_back({"Short Pluck", "Pluck", makePreset({
        {"osc1Waveform", 2},   // Saw
        {"osc1Range",    3},   // 8'
        {"osc1Level",    1.0f},
        {"osc2Enabled",  0.0f},
        {"mixerDrive",   1.0f},
        {"filterCutoff", 5000.0f}, {"filterResonance", 0.25f},
        {"filterContour",0.60f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.18f}, {"filterSustain", 0.0f}, {"filterRelease", 0.10f},
        {"loudnessAttack",0.001f},{"loudnessDecay",0.18f},{"loudnessSustain",0.0f},{"loudnessRelease",0.10f},
    })});

    // 7 – Bright Pluck
    factoryPresets.push_back({"Bright Pluck", "Pluck", makePreset({
        {"osc1Waveform", 2},   // Saw
        {"osc1Range",    3},   // 8'
        {"osc1Level",    0.75f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 0},   // Tri
        {"osc2Range",    4},   // 4'
        {"osc2Level",    0.45f},
        {"osc2Detune",   0.05f},
        {"mixerDrive",   1.3f},
        {"filterCutoff", 8000.0f}, {"filterResonance", 0.35f},
        {"filterContour",0.70f},
        {"filterAttack", 0.001f}, {"filterDecay", 0.22f}, {"filterSustain", 0.0f}, {"filterRelease", 0.12f},
        {"loudnessAttack",0.001f},{"loudnessDecay",0.25f},{"loudnessSustain",0.0f},{"loudnessRelease",0.12f},
        {"filterKeyboardTracking", 1.0f},
    })});

    // 8 – Warm Poly Chord
    factoryPresets.push_back({"Warm Poly Chord", "Poly", makePreset({
        {"playMode",     1.0f}, // POLY 4
        {"osc1Waveform", 1},   // Tri-Saw
        {"osc1Range",    3},   // 8'
        {"osc1Level",    0.70f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 0},   // Tri
        {"osc2Range",    3},
        {"osc2Level",    0.40f},
        {"osc2Detune",   0.06f},
        {"mixerDrive",   0.8f},
        {"filterCutoff", 3500.0f}, {"filterResonance", 0.08f},
        {"filterContour",0.10f},
        {"filterAttack", 0.02f}, {"filterDecay", 0.40f}, {"filterSustain", 0.70f}, {"filterRelease", 0.55f},
        {"loudnessAttack",0.015f},{"loudnessDecay",0.0f},{"loudnessSustain",1.0f},{"loudnessRelease",0.55f},
        {"filterKeyboardTracking", 1.0f},
    })});

    // 9 – Dark Poly Chord
    factoryPresets.push_back({"Dark Poly Chord", "Poly", makePreset({
        {"playMode",     1.0f}, // POLY 4
        {"osc1Waveform", 4},   // Square
        {"osc1Range",    3},   // 8'
        {"osc1Level",    0.65f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 4},   // Square
        {"osc2Range",    2},   // 16'
        {"osc2Level",    0.40f},
        {"osc2Detune",   0.0f},
        {"mixerDrive",   1.0f},
        {"filterCutoff", 1800.0f}, {"filterResonance", 0.14f},
        {"filterContour",0.08f},
        {"filterAttack", 0.02f}, {"filterDecay", 0.50f}, {"filterSustain", 0.60f}, {"filterRelease", 0.60f},
        {"loudnessAttack",0.015f},{"loudnessDecay",0.0f},{"loudnessSustain",1.0f},{"loudnessRelease",0.60f},
    })});

    // 10 – Noise Sweep
    factoryPresets.push_back({"Noise Sweep", "FX", makePreset({
        {"osc1Enabled",  0.0f},
        {"osc2Enabled",  0.0f},
        {"noiseLevel",   0.80f}, {"noiseMode", 1.0f}, // Pink
        {"mixerDrive",   0.5f},
        {"filterCutoff", 200.0f}, {"filterResonance", 0.45f},
        {"filterContour",0.85f},
        {"filterAttack", 2.50f}, {"filterDecay", 1.50f}, {"filterSustain", 0.30f}, {"filterRelease", 1.00f},
        {"loudnessAttack",0.10f},{"loudnessDecay",0.0f},{"loudnessSustain",1.0f},{"loudnessRelease",1.20f},
    })});

    // 11 – Filter Riser
    factoryPresets.push_back({"Filter Riser", "FX", makePreset({
        {"osc1Waveform", 2},   // Saw
        {"osc1Range",    2},   // 16'
        {"osc1Level",    0.70f},
        {"osc2Enabled",  1.0f},
        {"osc2Waveform", 4},   // Square
        {"osc2Range",    3},
        {"osc2Level",    0.50f},
        {"osc2Detune",   0.10f},
        {"mixerDrive",   1.5f},
        {"filterCutoff", 120.0f}, {"filterResonance", 0.30f},
        {"filterContour",1.00f},
        {"filterAttack", 4.00f}, {"filterDecay", 0.80f}, {"filterSustain", 0.0f}, {"filterRelease", 0.50f},
        {"loudnessAttack",0.005f},{"loudnessDecay",0.0f},{"loudnessSustain",1.0f},{"loudnessRelease",0.40f},
        {"filterKeyboardTracking", 1.0f},
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
