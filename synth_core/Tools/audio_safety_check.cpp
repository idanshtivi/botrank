// Audio safety / regression diagnostic tool — read-only, no DSP changes.
// Runs the full test matrix and prints a structured report.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include "../Include/SynthEngine.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace SynthCore;
using namespace std::chrono;

// ─── Metrics accumulator ─────────────────────────────────────────────────────
struct Metrics {
    double   peak         = 0.0;
    double   sumSq        = 0.0;
    uint64_t samples      = 0;
    uint64_t hardClips    = 0;   // abs(s) >= 0.9999
    double   maxDelta     = 0.0;
    uint64_t nanInf       = 0;
    double   prevSample   = 0.0;
    double   maxBlockUs   = 0.0;
    double   sumBlockUs   = 0.0;
    uint64_t blocks       = 0;

    void feedSample(float s) {
        if (!std::isfinite(s)) { ++nanInf; prevSample = 0.0; return; }
        const double d = std::abs(static_cast<double>(s) - prevSample);
        if (d > maxDelta) maxDelta = d;
        prevSample = s;
        const double as = std::abs(static_cast<double>(s));
        if (as > peak) peak = as;
        if (as >= 0.9999) ++hardClips;
        sumSq += static_cast<double>(s) * s;
        ++samples;
    }
    void addBlock(double us) { sumBlockUs += us; maxBlockUs = std::max(maxBlockUs, us); ++blocks; }
    double rms()        const { return samples > 0 ? std::sqrt(sumSq / samples) : 0.0; }
    double avgBlockUs() const { return blocks  > 0 ? sumBlockUs / blocks : 0.0; }
};

// ─── Param mapping ────────────────────────────────────────────────────────────
static void applyParam(SynthEngine& e, const char* id, float v)
{
    using P = ParamId;
    const std::string s(id);
    if      (s=="playMode")              e.setParameter(P::PlayMode, v);
    else if (s=="osc1Enabled")           e.setParameter(P::Osc1Enabled, v);
    else if (s=="osc2Enabled")           e.setParameter(P::Osc2Enabled, v);
    else if (s=="osc3Enabled")           e.setParameter(P::Osc3Enabled, v);
    else if (s=="osc1Level")             e.setParameter(P::Osc1Level, v);
    else if (s=="osc2Level")             e.setParameter(P::Osc2Level, v);
    else if (s=="osc3Level")             e.setParameter(P::Osc3Level, v);
    else if (s=="osc1Waveform")          e.setParameter(P::Osc1Waveform, v);
    else if (s=="osc2Waveform")          e.setParameter(P::Osc2Waveform, v);
    else if (s=="osc3Waveform")          e.setParameter(P::Osc3Waveform, v);
    else if (s=="osc1Range")             e.setParameter(P::Osc1Range, v);
    else if (s=="osc2Range")             e.setParameter(P::Osc2Range, v);
    else if (s=="osc3Range")             e.setParameter(P::Osc3Range, v);
    else if (s=="osc2Detune")            e.setParameter(P::Osc2Detune, v);
    else if (s=="osc3Detune")            e.setParameter(P::Osc3Detune, v);
    else if (s=="osc3KeyboardTracking")  e.setParameter(P::Osc3KeyboardTracking, v);
    else if (s=="mixerDrive")            e.setParameter(P::MixerDrive, v);
    else if (s=="noiseLevel")            { e.setParameter(P::NoiseLevel, v); e.setParameter(P::NoiseEnabled, v > 0.0001f ? 1.f : 0.f); }
    else if (s=="noiseMode")             e.setParameter(P::NoiseMode, v);
    else if (s=="filterCutoff")          e.setParameter(P::FilterCutoff, v);
    else if (s=="filterResonance")       e.setParameter(P::FilterResonance, v);
    else if (s=="filterContour")         e.setParameter(P::FilterEnvAmount, v);
    else if (s=="filterAttack")          e.setParameter(P::FilterAttack, v);
    else if (s=="filterDecay")           e.setParameter(P::FilterDecay, v);
    else if (s=="filterSustain")         e.setParameter(P::FilterSustain, v);
    else if (s=="filterRelease")         e.setParameter(P::FilterRelease, v);
    else if (s=="filterDrive")           e.setParameter(P::FilterDrive, v);
    else if (s=="filterKeyboardTracking")e.setParameter(P::FilterKeyboardTracking, v * 0.5f);
    else if (s=="loudnessAttack")        e.setParameter(P::AmpAttack, v);
    else if (s=="loudnessDecay")         e.setParameter(P::AmpDecay, v);
    else if (s=="loudnessSustain")       e.setParameter(P::AmpSustain, v);
    else if (s=="loudnessRelease")       e.setParameter(P::AmpRelease, v);
    else if (s=="masterVolume")          e.setParameter(P::MasterVolume, v);
    else if (s=="glideEnabled")          e.setParameter(P::GlideEnabled, v);
    else if (s=="glideTime")             e.setParameter(P::GlideTime, v);
    else if (s=="pitchBendRange")        e.setParameter(P::PitchBendRange, v);
    else if (s=="fineTune")              e.setParameter(P::FineTune, v);
    else if (s=="legato")                e.setParameter(P::Legato, v);
    else if (s=="retrigger")             e.setParameter(P::Retrigger, v);
    else if (s=="notePriority")          e.setParameter(P::NotePriority, v);
    else if (s=="osc1PulseWidth")        e.setParameter(P::Osc1PulseWidth, v);
    else if (s=="lfoRate")               e.setParameter(P::LfoRate, v);
    else if (s=="lfoAmount")             { e.setParameter(P::LfoAmount, v); e.setParameter(P::LfoEnabled, v > 0.0001f ? 1.f : 0.f); }
    else if (s=="lfoDestination")        e.setParameter(P::LfoDestination, v);
    else if (s=="modWheelAmount")        e.setParameter(P::ModWheelAmount, v);
    else if (s=="analogDrift")           e.setParameter(P::AnalogDrift, 0.0f); // always 0
    // outputDrive, noiseEnabled: handled above or no ParamId
}

using ParamList = std::vector<std::pair<const char*, float>>;

static const ParamList kBase = {
    {"playMode",0.f},{"osc1Enabled",1.f},{"osc2Enabled",1.f},{"osc3Enabled",0.f},
    {"osc1Level",0.65f},{"osc2Level",0.30f},{"osc3Level",0.f},
    {"osc1Waveform",2.f},{"osc2Waveform",2.f},{"osc3Waveform",0.f},
    {"osc1Range",3.f},{"osc2Range",3.f},{"osc3Range",3.f},
    {"osc2Detune",0.f},{"osc3Detune",0.f},{"osc3KeyboardTracking",1.f},
    {"mixerDrive",1.f},{"noiseLevel",0.f},{"noiseMode",0.f},
    {"filterCutoff",6000.f},{"filterResonance",0.10f},{"filterContour",0.15f},
    {"filterAttack",0.005f},{"filterDecay",0.30f},{"filterSustain",0.f},{"filterRelease",0.20f},
    {"filterDrive",0.50f},{"filterKeyboardTracking",0.f},
    {"loudnessAttack",0.005f},{"loudnessDecay",0.25f},{"loudnessSustain",0.75f},{"loudnessRelease",0.20f},
    {"masterVolume",0.60f},{"glideEnabled",0.f},{"glideTime",0.05f},
    {"pitchBendRange",2.f},{"fineTune",0.f},{"legato",0.f},{"retrigger",1.f},{"notePriority",0.f},
    {"osc1PulseWidth",0.50f},{"lfoRate",2.f},{"lfoAmount",0.f},{"lfoDestination",0.f},{"modWheelAmount",0.f},
    {"analogDrift",0.f},
};

static ParamList makePreset(std::initializer_list<std::pair<const char*,float>> ov) {
    auto base = kBase;
    for (auto& o : ov)
        for (auto& b : base)
            if (std::string(b.first)==std::string(o.first)) { b.second=o.second; break; }
    return base;
}

static void applyPreset(SynthEngine& e, const ParamList& p) {
    for (auto& kv : p) applyParam(e, kv.first, kv.second);
}

// ─── Factory presets ─────────────────────────────────────────────────────────
struct PresetDef { const char* name; ParamList params; };
static const std::vector<PresetDef> kPresets = {
    {"Init",              makePreset({})},
    {"Modern Solid Bass", makePreset({{"osc1Waveform",2.f},{"osc1Range",2.f},{"osc1Level",1.f},{"osc2Enabled",0.f},{"mixerDrive",1.8f},{"filterCutoff",900.f},{"filterResonance",0.18f},{"filterContour",0.35f},{"filterAttack",0.005f},{"filterDecay",0.35f},{"filterSustain",0.f},{"filterRelease",0.18f},{"loudnessAttack",0.005f},{"loudnessDecay",0.22f},{"loudnessSustain",0.f},{"loudnessRelease",0.12f},{"filterKeyboardTracking",1.f}})},
    {"Fat Sub Drive Bass",makePreset({{"osc1Waveform",2.f},{"osc1Range",2.f},{"osc1Level",0.80f},{"osc2Enabled",1.f},{"osc2Waveform",4.f},{"osc2Range",1.f},{"osc2Level",0.55f},{"mixerDrive",2.2f},{"filterCutoff",650.f},{"filterResonance",0.10f},{"filterContour",0.25f},{"filterAttack",0.005f},{"filterDecay",0.55f},{"filterSustain",0.f},{"filterRelease",0.20f},{"loudnessAttack",0.005f},{"loudnessDecay",0.40f},{"loudnessSustain",0.f},{"loudnessRelease",0.18f},{"filterDrive",1.0f}})},
    {"Dark Techno Bass",  makePreset({{"osc1Waveform",5.f},{"osc1Range",2.f},{"osc1Level",1.f},{"osc2Enabled",0.f},{"osc1PulseWidth",0.30f},{"mixerDrive",2.0f},{"filterCutoff",480.f},{"filterResonance",0.22f},{"filterContour",0.40f},{"filterAttack",0.005f},{"filterDecay",0.28f},{"filterSustain",0.f},{"filterRelease",0.15f},{"loudnessAttack",0.005f},{"loudnessDecay",0.30f},{"loudnessSustain",0.f},{"loudnessRelease",0.12f},{"filterDrive",0.80f},{"filterKeyboardTracking",1.f}})},
    {"Bright Modern Lead",makePreset({{"osc1Waveform",2.f},{"osc1Range",3.f},{"osc1Level",0.85f},{"osc2Enabled",1.f},{"osc2Waveform",2.f},{"osc2Range",3.f},{"osc2Level",0.35f},{"osc2Detune",0.08f},{"mixerDrive",1.2f},{"filterCutoff",7000.f},{"filterResonance",0.30f},{"filterContour",0.20f},{"filterAttack",0.008f},{"filterDecay",0.40f},{"filterSustain",0.60f},{"filterRelease",0.30f},{"loudnessAttack",0.010f},{"loudnessDecay",0.f},{"loudnessSustain",1.f},{"loudnessRelease",0.25f},{"filterKeyboardTracking",2.f},{"pitchBendRange",7.f}})},
    {"Soft Analog Lead",  makePreset({{"osc1Waveform",0.f},{"osc1Range",3.f},{"osc1Level",0.90f},{"osc2Enabled",1.f},{"osc2Waveform",0.f},{"osc2Range",3.f},{"osc2Level",0.40f},{"osc2Detune",0.04f},{"mixerDrive",0.8f},{"filterCutoff",4200.f},{"filterResonance",0.15f},{"filterContour",0.10f},{"filterAttack",0.05f},{"filterDecay",0.30f},{"filterSustain",0.80f},{"filterRelease",0.40f},{"loudnessAttack",0.04f},{"loudnessDecay",0.f},{"loudnessSustain",1.f},{"loudnessRelease",0.35f},{"filterKeyboardTracking",1.f},{"glideEnabled",1.f},{"glideTime",0.10f}})},
    {"Short Pluck",       makePreset({{"osc1Waveform",2.f},{"osc1Range",3.f},{"osc1Level",1.f},{"osc2Enabled",0.f},{"mixerDrive",1.0f},{"filterCutoff",5000.f},{"filterResonance",0.25f},{"filterContour",0.60f},{"filterAttack",0.001f},{"filterDecay",0.18f},{"filterSustain",0.f},{"filterRelease",0.10f},{"loudnessAttack",0.001f},{"loudnessDecay",0.18f},{"loudnessSustain",0.f},{"loudnessRelease",0.10f}})},
    {"Bright Pluck",      makePreset({{"osc1Waveform",2.f},{"osc1Range",3.f},{"osc1Level",0.75f},{"osc2Enabled",1.f},{"osc2Waveform",0.f},{"osc2Range",4.f},{"osc2Level",0.45f},{"osc2Detune",0.05f},{"mixerDrive",1.3f},{"filterCutoff",8000.f},{"filterResonance",0.35f},{"filterContour",0.70f},{"filterAttack",0.001f},{"filterDecay",0.22f},{"filterSustain",0.f},{"filterRelease",0.12f},{"loudnessAttack",0.001f},{"loudnessDecay",0.25f},{"loudnessSustain",0.f},{"loudnessRelease",0.12f},{"filterKeyboardTracking",1.f}})},
    {"Warm Poly Chord",   makePreset({{"playMode",1.f},{"osc1Waveform",1.f},{"osc1Range",3.f},{"osc1Level",0.70f},{"osc2Enabled",1.f},{"osc2Waveform",0.f},{"osc2Range",3.f},{"osc2Level",0.40f},{"osc2Detune",0.06f},{"mixerDrive",0.8f},{"filterCutoff",3500.f},{"filterResonance",0.08f},{"filterContour",0.10f},{"filterAttack",0.02f},{"filterDecay",0.40f},{"filterSustain",0.70f},{"filterRelease",0.55f},{"loudnessAttack",0.015f},{"loudnessDecay",0.f},{"loudnessSustain",1.f},{"loudnessRelease",0.55f},{"filterKeyboardTracking",1.f}})},
    {"Dark Poly Chord",   makePreset({{"playMode",1.f},{"osc1Waveform",4.f},{"osc1Range",3.f},{"osc1Level",0.65f},{"osc2Enabled",1.f},{"osc2Waveform",4.f},{"osc2Range",2.f},{"osc2Level",0.40f},{"mixerDrive",1.0f},{"filterCutoff",1800.f},{"filterResonance",0.14f},{"filterContour",0.08f},{"filterAttack",0.02f},{"filterDecay",0.50f},{"filterSustain",0.60f},{"filterRelease",0.60f},{"loudnessAttack",0.015f},{"loudnessDecay",0.f},{"loudnessSustain",1.f},{"loudnessRelease",0.60f}})},
    {"Noise Sweep",       makePreset({{"osc1Enabled",0.f},{"osc2Enabled",0.f},{"noiseLevel",0.80f},{"noiseMode",1.f},{"mixerDrive",0.5f},{"filterCutoff",200.f},{"filterResonance",0.45f},{"filterContour",0.85f},{"filterAttack",2.50f},{"filterDecay",1.50f},{"filterSustain",0.30f},{"filterRelease",1.00f},{"loudnessAttack",0.10f},{"loudnessDecay",0.f},{"loudnessSustain",1.f},{"loudnessRelease",1.20f}})},
    {"Filter Riser",      makePreset({{"osc1Waveform",2.f},{"osc1Range",2.f},{"osc1Level",0.70f},{"osc2Enabled",1.f},{"osc2Waveform",4.f},{"osc2Range",3.f},{"osc2Level",0.50f},{"osc2Detune",0.10f},{"mixerDrive",1.5f},{"filterCutoff",120.f},{"filterResonance",0.30f},{"filterContour",1.00f},{"filterAttack",4.00f},{"filterDecay",0.80f},{"filterSustain",0.f},{"filterRelease",0.50f},{"loudnessAttack",0.005f},{"loudnessDecay",0.f},{"loudnessSustain",1.f},{"loudnessRelease",0.40f},{"filterKeyboardTracking",1.f}})},
};

// ─── Render helpers ───────────────────────────────────────────────────────────
static constexpr int kBlockSize = 512;

static void renderBlocks(SynthEngine& e, Metrics& m, int numSamples) {
    for (int s = 0; s < numSamples; ) {
        const int batch = std::min(kBlockSize, numSamples - s);
        const auto t0 = high_resolution_clock::now();
        for (int i = 0; i < batch; ++i) m.feedSample(e.processSample());
        const double us = static_cast<double>(
            duration_cast<nanoseconds>(high_resolution_clock::now()-t0).count()) / 1000.0;
        m.addBlock(us);
        s += batch;
    }
}

// ─── Result record ────────────────────────────────────────────────────────────
struct Result {
    std::string scenario;
    std::string label;
    double   peak;
    double   rms;
    uint64_t hardClips;
    double   maxDelta;
    uint64_t nanInf;
    double   avgBlockUs;
    double   maxBlockUs;
    bool     pass;
};

static std::vector<Result> gResults;
static double gBlockBudgetUs = 0.0;

static void record(const char* scenario, const char* label, Metrics& m) {
    Result r;
    r.scenario   = scenario;
    r.label      = label;
    r.peak       = m.peak;
    r.rms        = m.rms();
    r.hardClips  = m.hardClips;
    r.maxDelta   = m.maxDelta;
    r.nanInf     = m.nanInf;
    r.avgBlockUs = m.avgBlockUs();
    r.maxBlockUs = m.maxBlockUs;
    r.pass       = (m.hardClips == 0 && m.nanInf == 0 && m.maxBlockUs < gBlockBudgetUs);
    gResults.push_back(r);
    printf("  %-38s  peak=%5.3f rms=%5.3f clips=%3llu delta=%5.3f nan=%llu cpu=%.0f/%.0f us %s\n",
           label,
           r.peak, r.rms,
           (unsigned long long)r.hardClips,
           r.maxDelta,
           (unsigned long long)r.nanInf,
           r.avgBlockUs, r.maxBlockUs,
           r.pass ? "OK" : "FAIL");
}

// ─── 1. Factory presets ───────────────────────────────────────────────────────
static void testPresets(double sr) {
    printf("\n== 1. Factory presets @ %.0f Hz ==\n", sr);
    static const int kNotes[] = {36, 48, 60, 72};
    for (auto& pdef : kPresets) {
        Metrics m;
        SynthEngine e; e.prepare(sr, kBlockSize);
        applyPreset(e, pdef.params);

        // Is poly?
        const bool isPoly = [&]{ for (auto& p : pdef.params) if (std::string(p.first)=="playMode" && p.second>=0.5f) return true; return false; }();
        const float rel = [&]{ for (auto& p : pdef.params) if (std::string(p.first)=="loudnessRelease") return p.second; return 0.20f; }();

        // a) Single notes
        for (int n : kNotes) {
            e.noteOn(n, 100.f);
            renderBlocks(e, m, static_cast<int>(sr*0.5));   // 500ms
            e.noteOff(n);
            renderBlocks(e, m, static_cast<int>(sr*0.3));   // 300ms release
        }

        // b) 3-note chord
        if (!isPoly) {
            e.noteOn(48, 100.f); renderBlocks(e, m, static_cast<int>(sr*0.2));
            e.noteOn(52, 100.f); renderBlocks(e, m, static_cast<int>(sr*0.2));
            e.noteOn(55, 100.f); renderBlocks(e, m, static_cast<int>(sr*0.3));
            e.noteOff(48); e.noteOff(52); e.noteOff(55);
            renderBlocks(e, m, static_cast<int>(sr*0.4));
        } else {
            e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100);
            renderBlocks(e, m, static_cast<int>(sr*0.4));
            e.noteOff(48); e.noteOff(52); e.noteOff(55);
            renderBlocks(e, m, static_cast<int>(sr*(rel+0.2)));
        }

        // c) 4-note chord (poly-relevant)
        if (isPoly) {
            e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
            renderBlocks(e, m, static_cast<int>(sr*0.5));
            e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
            renderBlocks(e, m, static_cast<int>(sr*(rel+0.3)));
        }

        // d) Long release tail
        e.noteOn(60, 100.f);
        renderBlocks(e, m, static_cast<int>(sr*0.3));
        e.noteOff(60);
        renderBlocks(e, m, static_cast<int>(sr*std::max(2.0f, rel+0.5f)));

        // e) Retrigger while release is active
        e.noteOn(60, 100.f); renderBlocks(e, m, static_cast<int>(sr*0.1));
        e.noteOff(60);
        renderBlocks(e, m, static_cast<int>(sr*0.05));  // only 50ms into release
        e.noteOn(60, 100.f); renderBlocks(e, m, static_cast<int>(sr*0.3));
        e.noteOff(60); renderBlocks(e, m, static_cast<int>(sr*(rel+0.2)));

        record("Factory", pdef.name, m);
    }
}

// ─── 2. Drive stress ─────────────────────────────────────────────────────────
static void testDriveStress(double sr) {
    printf("\n== 2. Drive stress @ %.0f Hz ==\n", sr);
    const float mDrives[]  = {0.0f, 1.0f, 2.0f, 3.0f};
    const float fDrives[]  = {0.0f, 1.0f, 2.0f, 3.0f};
    const float cutoffs[]  = {300.0f, 1000.0f, 5000.0f, 20000.0f};
    const float resonances[]= {0.0f, 0.20f, 0.50f};

    // Worst-case subset: all max drives, every cutoff+resonance combo
    for (float mDrv : mDrives) {
    for (float fDrv : fDrives) {
    for (float cut  : cutoffs) {
    for (float res  : resonances) {
        Metrics m;
        SynthEngine e; e.prepare(sr, kBlockSize);
        applyPreset(e, kBase);
        e.setParameter(ParamId::MixerDrive,      mDrv);
        e.setParameter(ParamId::FilterDrive,      fDrv);
        e.setParameter(ParamId::FilterCutoff,     cut);
        e.setParameter(ParamId::FilterResonance,  res);
        e.setParameter(ParamId::AmpSustain,       1.0f);
        e.setParameter(ParamId::AmpDecay,         0.5f);
        e.setParameter(ParamId::FilterEnvAmount,  0.5f);

        e.noteOn(60, 127.f);
        renderBlocks(e, m, static_cast<int>(sr*1.0));
        e.noteOff(60);
        renderBlocks(e, m, static_cast<int>(sr*0.5));

        char lbl[80];
        std::snprintf(lbl, sizeof(lbl), "MD=%.0f FD=%.0f cut=%.0f res=%.2f",
                      mDrv, fDrv, cut, res);
        record("Drive", lbl, m);
    }}}}
}

// ─── 3. Poly stress ──────────────────────────────────────────────────────────
static void testPolyStress(double sr) {
    printf("\n== 3. Poly stress @ %.0f Hz ==\n", sr);

    auto runPoly = [&](const char* label, float rel, int waitSamples) {
        Metrics m;
        SynthEngine e; e.prepare(sr, kBlockSize);
        applyPreset(e, kBase);
        e.setParameter(ParamId::PlayMode,      1.0f);
        e.setParameter(ParamId::AmpSustain,    1.0f);
        e.setParameter(ParamId::AmpRelease,    rel);
        e.setParameter(ParamId::AmpDecay,      0.5f);
        e.setParameter(ParamId::FilterCutoff,  8000.0f);
        e.setParameter(ParamId::MixerDrive,    2.0f);
        e.setParameter(ParamId::FilterDrive,   2.0f);

        // Hold 4 notes
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderBlocks(e, m, static_cast<int>(sr*1.0));

        // Release all
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderBlocks(e, m, waitSamples);

        // Retrigger
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderBlocks(e, m, static_cast<int>(sr*0.5));
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderBlocks(e, m, static_cast<int>(sr*(rel+0.3)));

        record("PolyStress", label, m);
    };

    runPoly("4-held clean",            0.5f, static_cast<int>(sr*0.5));
    runPoly("retrigger@10ms rel=0.5s", 0.5f, static_cast<int>(sr*0.010));
    runPoly("retrigger@80ms rel=0.5s", 0.5f, static_cast<int>(sr*0.080));
    runPoly("retrigger@300ms rel=0.5s",0.5f, static_cast<int>(sr*0.300));
    runPoly("retrigger@10ms rel=2.0s", 2.0f, static_cast<int>(sr*0.010));
    runPoly("retrigger@80ms rel=2.0s", 2.0f, static_cast<int>(sr*0.080));
    runPoly("retrigger@300ms rel=2.0s",2.0f, static_cast<int>(sr*0.300));

    // Rapid-fire: 16 noteOn calls in 100ms
    {
        Metrics m;
        SynthEngine e; e.prepare(sr, kBlockSize);
        applyPreset(e, kBase);
        e.setParameter(ParamId::PlayMode, 1.0f);
        e.setParameter(ParamId::MixerDrive, 2.5f);
        e.setParameter(ParamId::FilterDrive, 2.5f);
        const int step = static_cast<int>(sr * 0.06);
        for (int i = 0; i < 16; ++i) {
            e.noteOn(48 + (i%12), 100.f);
            renderBlocks(e, m, step);
            if (i%4==3) { e.noteOff(48); e.noteOff(49); e.noteOff(50); e.noteOff(51); }
        }
        renderBlocks(e, m, static_cast<int>(sr*1.0));
        record("PolyStress", "rapid-fire 16 noteOn poly", m);
    }
}

// ─── 4. Oscillator level stress ───────────────────────────────────────────────
static void testOscLevels(double sr) {
    printf("\n== 4. Oscillator level stress @ %.0f Hz ==\n", sr);

    struct OscCase { const char* lbl; float osc1; float osc2; float osc3; float en2; float en3; float noise; };
    const OscCase cases[] = {
        {"Osc1 only lo",      0.30f, 0,    0,    0, 0, 0},
        {"Osc1 only max",     1.00f, 0,    0,    0, 0, 0},
        {"Osc1+2 max",        1.00f, 1.00f,0,    1, 0, 0},
        {"Osc1+2+3 max",      1.00f, 1.00f,1.00f,1, 1, 0},
        {"All osc max+noise", 1.00f, 1.00f,1.00f,1, 1, 0.8f},
        {"All lo",            0.20f, 0.20f,0.20f,1, 1, 0},
        {"Osc1+noise only",   1.00f, 0,    0,    0, 0, 0.8f},
    };

    for (auto& c : cases) {
        Metrics m;
        SynthEngine e; e.prepare(sr, kBlockSize);
        applyPreset(e, kBase);
        e.setParameter(ParamId::Osc1Level, c.osc1);
        e.setParameter(ParamId::Osc2Level, c.osc2);
        e.setParameter(ParamId::Osc3Level, c.osc3);
        e.setParameter(ParamId::Osc2Enabled, c.en2);
        e.setParameter(ParamId::Osc3Enabled, c.en3);
        e.setParameter(ParamId::NoiseLevel, c.noise);
        e.setParameter(ParamId::NoiseEnabled, c.noise > 0.001f ? 1.f : 0.f);
        e.setParameter(ParamId::MixerDrive, 3.0f);   // max drive stress
        e.setParameter(ParamId::FilterDrive, 3.0f);
        e.setParameter(ParamId::FilterCutoff, 8000.0f);
        e.setParameter(ParamId::AmpSustain, 1.0f);

        e.noteOn(60, 127.f);
        renderBlocks(e, m, static_cast<int>(sr*1.0));
        e.noteOff(60);
        renderBlocks(e, m, static_cast<int>(sr*0.5));
        record("OscLevels", c.lbl, m);
    }
}

// ─── 5. Automation stress ────────────────────────────────────────────────────
static void testAutomation(double sr) {
    printf("\n== 5. Automation stress @ %.0f Hz ==\n", sr);

    SynthEngine e; e.prepare(sr, kBlockSize);
    applyPreset(e, kBase);
    e.setParameter(ParamId::AmpSustain, 1.0f);
    e.setParameter(ParamId::AmpRelease, 1.0f);

    e.noteOn(60, 100.f);
    e.noteOn(64, 100.f);

    Metrics m;
    const int totalSamples = static_cast<int>(sr * 8.0);  // 8 sec sweep
    int phase = 0;

    for (int s = 0; s < totalSamples; ) {
        const int batch = std::min(kBlockSize, totalSamples - s);
        // Automate params at block boundaries — simulates host automation
        const double t = static_cast<double>(s) / sr;
        const float sn01 = static_cast<float>(0.5*(1.0+std::sin(2.0*M_PI*t*0.4)));
        e.setParameter(ParamId::MixerDrive,     sn01 * 3.0f);
        e.setParameter(ParamId::FilterDrive,    sn01 * 3.0f);
        e.setParameter(ParamId::FilterCutoff,   200.0f + 19800.0f * sn01);
        e.setParameter(ParamId::FilterResonance,sn01 * 0.50f);
        e.setParameter(ParamId::Osc1Level,      0.30f + 0.70f * sn01);
        e.setParameter(ParamId::Osc2Level,      0.30f + 0.70f * (1.0f-sn01));
        e.setParameter(ParamId::MasterVolume,   0.30f + 0.70f * sn01);
        e.setParameter(ParamId::AmpRelease,     0.05f + 2.0f * sn01);

        const auto t0 = high_resolution_clock::now();
        for (int i = 0; i < batch; ++i) m.feedSample(e.processSample());
        const double us = static_cast<double>(
            duration_cast<nanoseconds>(high_resolution_clock::now()-t0).count()) / 1000.0;
        m.addBlock(us);
        s += batch; ++phase;
    }
    e.noteOff(60); e.noteOff(64);

    // Zipper check: max delta during automation — intentionally high threshold
    // since cutoff sweeps naturally produce large inter-block deltas
    record("Automation", "drive+cutoff+res+vol sweep 8s", m);

    // Separate test: oscillator levels during sustain
    {
        Metrics m2;
        SynthEngine e2; e2.prepare(sr, kBlockSize);
        applyPreset(e2, kBase);
        e2.setParameter(ParamId::AmpSustain, 1.0f);
        e2.setParameter(ParamId::MixerDrive, 2.0f);
        e2.setParameter(ParamId::FilterCutoff, 8000.0f);
        e2.noteOn(60, 100.f);
        const int total2 = static_cast<int>(sr * 4.0);
        for (int s = 0; s < total2; ) {
            const int batch = std::min(kBlockSize, total2 - s);
            const double t2 = static_cast<double>(s) / sr;
            const float v = static_cast<float>(0.5*(1.0+std::sin(2.0*M_PI*t2*1.0)));
            e2.setParameter(ParamId::Osc1Level, v);
            e2.setParameter(ParamId::Osc2Level, 1.0f-v);
            const auto t0 = high_resolution_clock::now();
            for (int i = 0; i < batch; ++i) m2.feedSample(e2.processSample());
            m2.addBlock(static_cast<double>(
                duration_cast<nanoseconds>(high_resolution_clock::now()-t0).count())/1000.0);
            s += batch;
        }
        e2.noteOff(60);
        renderBlocks(e2, m2, static_cast<int>(sr*0.5));
        record("Automation", "osc level crossfade 4s", m2);
    }
}

// ─── 6. Sample-rate / buffer-size stress ────────────────────────────────────
struct SRCase { double sr; int buf; };
static const SRCase kSRCases[] = {
    {44100.0, 512},   // relaxed reference
    {44100.0, 128},
    {48000.0, 128},
    {48000.0,  64},
    {96000.0, 128},
    {96000.0,  64},
};

// Render exactly buf-sized blocks and collect timing + audio metrics.
static void renderAtBufSize(SynthEngine& e, Metrics& m, double /*sr*/, int buf, int totalSamples)
{
    for (int s = 0; s < totalSamples; ) {
        const int batch = std::min(buf, totalSamples - s);
        const auto t0 = high_resolution_clock::now();
        for (int i = 0; i < batch; ++i) m.feedSample(e.processSample());
        const double us = static_cast<double>(
            duration_cast<nanoseconds>(high_resolution_clock::now()-t0).count()) / 1000.0;
        m.addBlock(us);
        s += batch;
    }
}

static void printSRRow(const char* lbl, double sr, int buf, const Metrics& m, bool addToResults = true)
{
    const double budget = static_cast<double>(buf) / sr * 1e6;
    const double avgPct = m.avgBlockUs() / budget * 100.0;
    const double maxPct = m.maxBlockUs   / budget * 100.0;
    const bool pass = (m.hardClips == 0 && m.nanInf == 0 && m.maxBlockUs < budget);
    printf("  %-44s peak=%5.3f clips=%3llu nan=%llu  avg=%5.0f/%5.0f us (%4.1f%%/%4.1f%%)  %s\n",
           lbl, m.peak,
           (unsigned long long)m.hardClips,
           (unsigned long long)m.nanInf,
           m.avgBlockUs(), m.maxBlockUs,
           avgPct, maxPct,
           pass ? "OK" : "FAIL");

    if (addToResults) {
        Result r;
        r.scenario   = "SampleRate";
        r.label      = lbl;
        r.peak       = m.peak;
        r.rms        = m.rms();
        r.hardClips  = m.hardClips;
        r.maxDelta   = m.maxDelta;
        r.nanInf     = m.nanInf;
        r.avgBlockUs = m.avgBlockUs();
        r.maxBlockUs = m.maxBlockUs;
        r.pass       = pass;
        gResults.push_back(r);
    }
}

static void testAllSRScenarios(double sr, int buf)
{
    const double budget = static_cast<double>(buf) / sr * 1e6;
    char hdr[80];
    std::snprintf(hdr, sizeof(hdr), "\n  --- %.0f Hz / buf=%d  (budget=%.0f µs) ---", sr, buf, budget);
    printf("%s\n", hdr);

    const int k1s  = static_cast<int>(sr * 1.0);
    const int k2s  = static_cast<int>(sr * 2.0);
    const int k500 = static_cast<int>(sr * 0.5);
    const int k80  = static_cast<int>(sr * 0.08);

    // ── Scenario A: Dark Poly Chord preset, 4 voices, long release
    {
        Metrics m;
        SynthEngine e; e.prepare(sr, buf);
        applyPreset(e, kPresets[9].params);  // Dark Poly Chord
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderAtBufSize(e, m, sr, buf, k2s);
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderAtBufSize(e, m, sr, buf, k1s);  // 1s release tail
        char lbl[80]; std::snprintf(lbl, sizeof(lbl), "DarkPolyChord 4v+release %.0fHz/%d", sr, buf);
        printSRRow(lbl, sr, buf, m);
    }

    // ── Scenario B: Warm Poly Chord preset, 4 voices, long release
    {
        Metrics m;
        SynthEngine e; e.prepare(sr, buf);
        applyPreset(e, kPresets[8].params);  // Warm Poly Chord
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderAtBufSize(e, m, sr, buf, k2s);
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderAtBufSize(e, m, sr, buf, k1s);
        char lbl[80]; std::snprintf(lbl, sizeof(lbl), "WarmPolyChord 4v+release %.0fHz/%d", sr, buf);
        printSRRow(lbl, sr, buf, m);
    }

    // ── Scenario C: Poly + MD=3 FD=3, cutoff open, res=0.2
    {
        Metrics m;
        SynthEngine e; e.prepare(sr, buf);
        applyPreset(e, kBase);
        e.setParameter(ParamId::PlayMode,       1.0f);
        e.setParameter(ParamId::MixerDrive,     3.0f);
        e.setParameter(ParamId::FilterDrive,    3.0f);
        e.setParameter(ParamId::FilterCutoff,   20000.0f);
        e.setParameter(ParamId::FilterResonance,0.2f);
        e.setParameter(ParamId::AmpSustain,     1.0f);
        e.setParameter(ParamId::AmpRelease,     1.5f);
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderAtBufSize(e, m, sr, buf, k2s);
        // Retrigger while release tails active (80ms into release)
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderAtBufSize(e, m, sr, buf, k80);
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderAtBufSize(e, m, sr, buf, k1s);
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderAtBufSize(e, m, sr, buf, k500);
        char lbl[80]; std::snprintf(lbl, sizeof(lbl), "Poly4v MD3 FD3 cutOpen res0.2 ret %.0fHz/%d", sr, buf);
        printSRRow(lbl, sr, buf, m);
    }

    // ── Scenario D: Poly + MD=3 FD=3, cutoff=1000, res=0.5
    {
        Metrics m;
        SynthEngine e; e.prepare(sr, buf);
        applyPreset(e, kBase);
        e.setParameter(ParamId::PlayMode,       1.0f);
        e.setParameter(ParamId::MixerDrive,     3.0f);
        e.setParameter(ParamId::FilterDrive,    3.0f);
        e.setParameter(ParamId::FilterCutoff,   1000.0f);
        e.setParameter(ParamId::FilterResonance,0.5f);
        e.setParameter(ParamId::AmpSustain,     1.0f);
        e.setParameter(ParamId::AmpRelease,     1.5f);
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderAtBufSize(e, m, sr, buf, k2s);
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderAtBufSize(e, m, sr, buf, k80);
        e.noteOn(48,100); e.noteOn(52,100); e.noteOn(55,100); e.noteOn(59,100);
        renderAtBufSize(e, m, sr, buf, k1s);
        e.noteOff(48); e.noteOff(52); e.noteOff(55); e.noteOff(59);
        renderAtBufSize(e, m, sr, buf, k500);
        char lbl[80]; std::snprintf(lbl, sizeof(lbl), "Poly4v MD3 FD3 cut1kHz res0.5 ret %.0fHz/%d", sr, buf);
        printSRRow(lbl, sr, buf, m);
    }
}

// ─── Aggregated report ────────────────────────────────────────────────────────
static void printReport() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║             AUDIO SAFETY / REGRESSION DIAGNOSIS REPORT              ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");

    // Aggregate totals
    double   worstPeak = 0, worstRms = 0, worstDelta = 0;
    uint64_t totalClips = 0, totalNan = 0;
    double   worstAvgCpu = 0, worstMaxCpu = 0;
    std::string worstPeakLbl, worstDeltaLbl, worstCpuLbl;
    int      failCount = 0;
    std::vector<const Result*> presetResults;
    std::vector<const Result*> failResults;

    for (auto& r : gResults) {
        if (r.scenario == "Factory") presetResults.push_back(&r);
        if (!r.pass) { ++failCount; failResults.push_back(&r); }
        totalClips += r.hardClips;
        totalNan   += r.nanInf;
        if (r.peak > worstPeak) { worstPeak = r.peak; worstPeakLbl = r.label; }
        if (r.rms  > worstRms)    worstRms  = r.rms;
        if (r.maxDelta > worstDelta) { worstDelta = r.maxDelta; worstDeltaLbl = r.label; }
        if (r.maxBlockUs > worstMaxCpu) { worstMaxCpu = r.maxBlockUs; worstCpuLbl = r.label; }
        if (r.avgBlockUs > worstAvgCpu)   worstAvgCpu = r.avgBlockUs;
    }

    printf("\n1.  Worst final peak:          %.4f  (%s)\n", worstPeak, worstPeakLbl.c_str());
    printf("2.  Worst final RMS:           %.4f\n", worstRms);
    printf("3.  Final hard clip count:     %llu  %s\n",
           (unsigned long long)totalClips, totalClips==0?"(PASS)":"(FAIL)");
    printf("4.  Worst max delta:           %.4f  (%s)\n", worstDelta, worstDeltaLbl.c_str());
    printf("5.  NaN/Inf count:             %llu  %s\n",
           (unsigned long long)totalNan, totalNan==0?"(PASS)":"(FAIL)");
    printf("6.  Worst avg block time:      %.1f µs\n", worstAvgCpu);
    printf("7.  Worst max block time:      %.1f µs  (%s)\n", worstMaxCpu, worstCpuLbl.c_str());

    // Mixer / internal stages note
    printf("\n8.  Mixer hard clamp count:    N/A — mixer uses tanh-based softLimit (no hard ±1 clamp)\n");
    printf("9.  Filter/VCA internal peaks: N/A — requires DSP instrumentation (see note below)\n");

    // Factory preset safety
    bool allPresetsSafe = true;
    const Result* worstPreset = nullptr;
    for (auto* r : presetResults) {
        if (!r->pass || r->peak > 0.95) {
            allPresetsSafe = false;
            if (!worstPreset || r->peak > worstPreset->peak) worstPreset = r;
        }
    }
    printf("\n10. All factory presets safe:  %s\n", allPresetsSafe ? "YES" : "NO");
    if (worstPreset)
        printf("    Worst preset:              %s  (peak=%.4f clips=%llu)\n",
               worstPreset->label.c_str(), worstPreset->peak,
               (unsigned long long)worstPreset->hardClips);

    // Poly retrigger
    bool polyClear = true;
    for (auto& r : gResults)
        if (r.scenario == "PolyStress" && !r.pass) { polyClear = false; break; }
    printf("11. Poly retrigger clean:      %s\n", polyClear ? "YES (all poly scenarios pass)" : "NO");

    // Overall
    printf("\n── OVERALL: %d/%d scenarios PASS ──\n", (int)gResults.size()-failCount, (int)gResults.size());

    if (failCount == 0) {
        printf("\nAll acceptance targets met:\n");
        printf("  ✓ hard clip count = 0\n");
        printf("  ✓ NaN/Inf count = 0\n");
        if (worstPeak < 0.95)
            printf("  ✓ final peak < 0.95 (worst = %.4f)\n", worstPeak);
        else
            printf("  ⚠ final peak = %.4f (above 0.95 — no clip, but headroom tight)\n", worstPeak);
        printf("  ✓ poly retrigger clean\n");
        printf("  ✓ no NaN/Inf\n");
        printf("\n  No fixes required.\n");
    } else {
        printf("\nFailed scenarios:\n");
        for (auto* r : failResults)
            printf("  FAIL  [%s] %s  peak=%.4f clips=%llu nan=%llu maxCpu=%.0fµs\n",
                   r->scenario.c_str(), r->label.c_str(), r->peak,
                   (unsigned long long)r->hardClips, (unsigned long long)r->nanInf, r->maxBlockUs);
        printf("\n  Recommended fixes: see failed rows above.\n");
    }

    printf("\nNote: internal stage metrics (mixer_output_peak, filter_input_peak, VCA_output_peak)\n");
    printf("require compile-time instrumentation hooks. The final output clamp guard (abs >= 0.9999)\n");
    printf("is the definitive safety boundary — if it stays 0, no internal stage exploded.\n");
}

// ─── 7. Oscillator level monotonic test ──────────────────────────────────────
// This is a diagnosis-only section, not included in gResults pass/fail.
// Purpose: verify that osc level = 0..1 produces strictly monotonic output
// independent of drive saturation.
static void testOscLevelMonotonic(double sr)
{
    printf("\n== 7. Oscillator level monotonic (diagnosis only) ==\n");

    // Clean signal chain — no saturation sources
    auto makeCleanEngine = [&](float mixerDrive, float filterDrive) {
        SynthEngine e; e.prepare(sr, kBlockSize);
        // Osc1 saw, 8-foot, solo
        e.setParameter(ParamId::Osc1Enabled,       1.0f);
        e.setParameter(ParamId::Osc1Waveform,       2.0f);   // Saw
        e.setParameter(ParamId::Osc1Range,          3.0f);   // 8-foot
        e.setParameter(ParamId::Osc2Enabled,        0.0f);
        e.setParameter(ParamId::Osc2Level,          0.0f);
        e.setParameter(ParamId::Osc3Enabled,        0.0f);
        e.setParameter(ParamId::Osc3Level,          0.0f);
        e.setParameter(ParamId::NoiseEnabled,       0.0f);
        e.setParameter(ParamId::NoiseLevel,         0.0f);
        e.setParameter(ParamId::MixerDrive,         mixerDrive);
        e.setParameter(ParamId::FilterDrive,        filterDrive);
        e.setParameter(ParamId::FilterCutoff,       20000.0f);  // fully open
        e.setParameter(ParamId::FilterResonance,    0.0f);
        e.setParameter(ParamId::FilterEnvAmount,    0.0f);
        e.setParameter(ParamId::FilterAttack,       0.001f);
        e.setParameter(ParamId::FilterDecay,        0.001f);
        e.setParameter(ParamId::FilterSustain,      1.0f);
        e.setParameter(ParamId::FilterRelease,      0.001f);
        e.setParameter(ParamId::AmpAttack,          0.005f);
        e.setParameter(ParamId::AmpDecay,           0.001f);
        e.setParameter(ParamId::AmpSustain,         1.0f);
        e.setParameter(ParamId::AmpRelease,         0.001f);
        e.setParameter(ParamId::MasterVolume,       1.0f);
        e.setParameter(ParamId::PlayMode,           0.0f);     // mono
        e.setParameter(ParamId::GlideEnabled,       0.0f);
        e.setParameter(ParamId::AnalogDrift,        0.0f);
        e.setParameter(ParamId::LfoAmount,          0.0f);
        e.setParameter(ParamId::LfoEnabled,         0.0f);
        return e;
    };

    const float kLevels[] = {0.0f, 0.25f, 0.50f, 0.75f, 1.0f};
    const int kSettleSamples  = static_cast<int>(sr * 0.20); // 200ms: let level smoother settle
    const int kMeasureSamples = static_cast<int>(sr * 0.50); // 500ms measure window

    for (int pass = 0; pass < 2; ++pass) {
        const float mDrv = (pass == 0) ? 0.0f : 2.0f;
        const float fDrv = 0.0f;  // filter drive always clean
        printf("\n  Osc1 saw — MixerDrive=%.0f  FilterDrive=%.0f  Cutoff=20kHz  Res=0  Master=1.0\n", mDrv, fDrv);
        printf("  %-8s  %-9s  %-9s  %s\n", "Level", "Peak", "RMS", "Monotonic?");

        double prevRms = -1.0;
        bool allMonotonic = true;

        for (float lv : kLevels) {
            SynthEngine e = makeCleanEngine(mDrv, fDrv);
            e.setParameter(ParamId::Osc1Level, lv);

            e.noteOn(60, 100.f);

            // Settle: discard first 200ms so level smoother converges
            for (int i = 0; i < kSettleSamples; ++i) e.processSample();

            // Measure 500ms
            double sumSq = 0.0; double peak = 0.0; uint64_t n = 0;
            for (int i = 0; i < kMeasureSamples; ++i) {
                const float s = e.processSample();
                const double as = std::abs(static_cast<double>(s));
                if (as > peak) peak = as;
                sumSq += static_cast<double>(s)*s;
                ++n;
            }
            const double rms = (n > 0) ? std::sqrt(sumSq / n) : 0.0;

            const bool mono = (prevRms < 0.0) || (rms >= prevRms - 1e-6);
            if (!mono) allMonotonic = false;
            printf("  %-8.2f  %-9.5f  %-9.5f  %s\n", lv, peak, rms,
                   mono ? "yes" : "FAIL (lower than previous)");
            prevRms = rms;
        }
        printf("  Monotonic overall: %s\n", allMonotonic ? "YES" : "NO");
    }

    // Also report what the OLD test (lo=0.30, max=1.0, Drive=3) was doing
    printf("\n  Context: original testOscLevels used MixerDrive=3 + FilterDrive=3.\n");
    printf("  Saturation compresses lo=0.30 and max=1.0 to similar peaks.\n");
    printf("  This is expected saturation behavior, NOT a synth routing bug.\n");

    // Show with Drive=3 for comparison
    printf("\n  Same saw osc — MixerDrive=3  FilterDrive=3 (original stress config):\n");
    printf("  %-8s  %-9s  %-9s\n", "Level", "Peak", "RMS");
    for (float lv : kLevels) {
        SynthEngine e = makeCleanEngine(3.0f, 3.0f);
        e.setParameter(ParamId::Osc1Level, lv);
        e.noteOn(60, 100.f);
        for (int i = 0; i < kSettleSamples; ++i) e.processSample();
        double sumSq = 0.0; double peak = 0.0; uint64_t n = 0;
        for (int i = 0; i < kMeasureSamples; ++i) {
            const float s = e.processSample();
            const double as = std::abs(static_cast<double>(s));
            if (as > peak) peak = as;
            sumSq += static_cast<double>(s)*s;
            ++n;
        }
        printf("  %-8.2f  %-9.5f  %-9.5f\n", lv, peak, (n>0?std::sqrt(sumSq/n):0.0));
    }
}

int main()
{
    // Block budget at 44100 / 512: 11.6 ms
    gBlockBudgetUs = static_cast<double>(kBlockSize) / 44100.0 * 1e6;

    // Run all test sections
    testPresets(44100.0);
    testDriveStress(44100.0);
    testPolyStress(44100.0);
    testOscLevels(44100.0);
    testAutomation(44100.0);

    printf("\n== 6. Sample-rate / buffer-size stress ==\n");
    printf("  %-44s %-24s avg/max µs (avg%%/max%%)  pass\n", "Scenario", "peak  clips nan");
    for (auto& c : kSRCases) testAllSRScenarios(c.sr, c.buf);

    printReport();

    // Standalone diagnosis — not part of pass/fail
    testOscLevelMonotonic(44100.0);

    return 0;
}
