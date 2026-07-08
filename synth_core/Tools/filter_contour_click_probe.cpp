// Offline diagnostic probe: single-note note-on click investigation, focused
// on the Filter Contour / filter / drive interaction hypothesis -- NOT
// voice stealing (every test here is a single fresh note, guaranteed
// FreeSlot allocation) and NOT CPU/real-time timing (this is an offline,
// non-real-time render). Normal production signal path only.
//
// Hypothesis under test: LadderFilter::processSample runs BEFORE Vca in the
// per-voice chain (see SynthVoice::processSample), and is fed the oscillator
// signal scaled only by the ~2ms fresh-voice declick ramp -- NOT by the
// (potentially much slower) Amp Attack envelope, which is applied by the VCA
// *after* the filter. So with a slow Amp Attack (e.g. 500ms) but a fast
// declick (~2ms), the filter's own resonance/drive nonlinearity can see a
// near-full-amplitude signal well before the audible envelope has caught up
// -- a genuinely different mechanism than anything tested so far (allocation,
// CPU, release tails), and NOT fixed by a longer Attack time, matching the
// reported symptom.
//
// This is throwaway investigation tooling -- not part of the shipped product.
#include "../Include/SynthEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace SynthCore;

namespace {

void writeLE16(std::ofstream& f, uint16_t v) {
    uint8_t buf[2] = { static_cast<uint8_t>(v & 0xFFu), static_cast<uint8_t>(v >> 8u) };
    f.write(reinterpret_cast<const char*>(buf), 2);
}
void writeLE32(std::ofstream& f, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>(v & 0xFFu), static_cast<uint8_t>((v >> 8u) & 0xFFu),
        static_cast<uint8_t>((v >> 16u) & 0xFFu), static_cast<uint8_t>((v >> 24u) & 0xFFu)
    };
    f.write(reinterpret_cast<const char*>(buf), 4);
}
void writeWav(const std::string& path, const std::vector<float>& mono, double sampleRate) {
    std::vector<int16_t> pcm(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); ++i) {
        const int16_t s = static_cast<int16_t>(std::clamp(mono[i], -1.0f, 1.0f) * 32767.0f);
        pcm[i * 2] = s; pcm[i * 2 + 1] = s;
    }
    std::ofstream f(path, std::ios::binary);
    const uint32_t dataSize = static_cast<uint32_t>(pcm.size()) * 2u;
    const uint32_t fmtSize = 16u;
    const uint32_t riffSize = 4u + (8u + fmtSize) + (8u + dataSize);
    f.write("RIFF", 4); writeLE32(f, riffSize); f.write("WAVE", 4);
    f.write("fmt ", 4); writeLE32(f, fmtSize);
    writeLE16(f, 1u); writeLE16(f, 2u);
    writeLE32(f, static_cast<uint32_t>(sampleRate));
    writeLE32(f, static_cast<uint32_t>(sampleRate) * 2u * 2u);
    writeLE16(f, 4u); writeLE16(f, 16u);
    f.write("data", 4); writeLE32(f, dataSize);
    f.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(dataSize));
}

double predict(const std::vector<float>& raw, int i) {
    const double y0 = raw[static_cast<size_t>(i - 3)], y1 = raw[static_cast<size_t>(i - 2)], y2 = raw[static_cast<size_t>(i - 1)];
    const double d1 = y2 - y1, d2 = (y2 - y1) - (y1 - y0);
    return y2 + d1 + d2;
}

struct Config {
    std::string name;
    double cutoffHz;
    double resonance;      // "Emphasis"
    double filterContour;
    double filterAttack;
    double ampAttack;
    double mixerDrive;
    double filterDrive;
};

void runConfig(const Config& cfg, double sampleRate, int blockSize)
{
    const double durationSeconds = 5.0;
    const int totalFrames = static_cast<int>(sampleRate * durationSeconds);
    const int noteOnFrame = static_cast<int>(sampleRate * 0.10);
    const int note = 48; // C3 -- comfortably inside the low-cutoff-relevant register

    SynthEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setParameter(ParamId::PlayMode, 1.0f); // poly -- but only ONE note is ever played
    engine.setParameter(ParamId::FilterCutoff, static_cast<float>(cfg.cutoffHz));
    engine.setParameter(ParamId::FilterResonance, static_cast<float>(cfg.resonance));
    engine.setParameter(ParamId::FilterEnvAmount, static_cast<float>(cfg.filterContour));
    engine.setParameter(ParamId::FilterAttack, static_cast<float>(cfg.filterAttack));
    engine.setParameter(ParamId::AmpAttack, static_cast<float>(cfg.ampAttack));
    engine.setParameter(ParamId::MixerDrive, static_cast<float>(cfg.mixerDrive));
    engine.setParameter(ParamId::FilterDrive, static_cast<float>(cfg.filterDrive));
    engine.setParameter(ParamId::LfoAmount, 0.0f);
    engine.setParameter(ParamId::ModWheelAmount, 0.0f);

    std::vector<float> out(static_cast<size_t>(totalFrames), 0.0f);
    for (int frame = 0; frame < totalFrames; frame += blockSize) {
        const int thisBlockSize = std::min(blockSize, totalFrames - frame);
        for (int i = 0; i < thisBlockSize; ++i) {
            const int globalFrame = frame + i;
            if (globalFrame == noteOnFrame)
                engine.noteOn(note, 100.0f);
            out[static_cast<size_t>(globalFrame)] = engine.processSample();
        }
    }

    writeWav("filter_contour_probe_" + cfg.name + ".wav", out, sampleRate);

    {
        std::ofstream rawF("filter_contour_probe_" + cfg.name + "_raw.csv");
        rawF.precision(9);
        rawF << "sample,value\n";
        const int dumpEnd = std::min(totalFrames, noteOnFrame + static_cast<int>(sampleRate * 0.8));
        for (int i = 0; i < dumpEnd; ++i)
            rawF << i << ',' << out[static_cast<size_t>(i)] << '\n';
    }

    // Baseline: well-settled region near the end (past even a 3.5s Filter
    // Attack, guaranteed to be in Decay/Sustain and no longer moving fast).
    const int bStart = static_cast<int>(sampleRate * 4.5);
    const int bEnd = static_cast<int>(sampleRate * 4.9);
    double bmax = 0.0, bsum = 0.0; int bcount = 0;
    for (int i = std::max(3, bStart); i < bEnd; ++i) {
        const double r = std::abs(out[static_cast<size_t>(i)] - predict(out, i));
        bmax = std::max(bmax, r);
        bsum += r; ++bcount;
    }
    const double bmean = bcount > 0 ? bsum / bcount : 0.0;

    // Test window: note-on through 700ms after (covers the fast declick, the
    // Amp Attack ramp for short-attack configs, and the early part of even a
    // 3.5s Filter Attack ramp).
    const int wStart = noteOnFrame;
    const int wEnd = std::min(totalFrames - 1, noteOnFrame + static_cast<int>(sampleRate * 0.7));
    double worst = 0.0; int worstOffMs = 0;
    for (int i = std::max(3, wStart); i < wEnd; ++i) {
        const double r = std::abs(out[static_cast<size_t>(i)] - predict(out, i));
        if (r > worst) { worst = r; worstOffMs = static_cast<int>((i - noteOnFrame) * 1000.0 / sampleRate); }
    }

    const double ratio = worst / std::max(bmax, 1e-9);
    const bool flagged = ratio > 3.0 && worst > 0.02;

    std::cout << "config=" << cfg.name
              << "  cutoff=" << cfg.cutoffHz << " res=" << cfg.resonance
              << " filtContour=" << cfg.filterContour << " filtAtk=" << cfg.filterAttack
              << " ampAtk=" << cfg.ampAttack << " mixDrive=" << cfg.mixerDrive
              << " filtDrive=" << cfg.filterDrive
              << "  worstResidual=" << worst << " at +" << worstOffMs << "ms"
              << "  baseline(max=" << bmax << ",mean=" << bmean << ")"
              << "  ratio=" << ratio << (flagged ? "  <== FLAGGED" : "") << "\n";
}

} // namespace

int main()
{
    const double sampleRate = 44100.0;
    const int blockSize = 256;

    // Reported patch (baseline for this investigation).
    const double bCutoff = 250.0, bRes = 0.30, bContour = 0.35, bFAtk = 3.5, bAAtk = 0.5,
                 bMixDrive = 0.70, bFiltDrive = 0.50;

    const std::vector<Config> configs = {
        // 1-3: Filter Contour sweep
        { "1_contour_0",      bCutoff, bRes, 0.0,  bFAtk, bAAtk, bMixDrive, bFiltDrive },
        { "2_contour_0.35",   bCutoff, bRes, 0.35, bFAtk, bAAtk, bMixDrive, bFiltDrive },
        { "3_contour_1.0",    bCutoff, bRes, 1.0,  bFAtk, bAAtk, bMixDrive, bFiltDrive },
        // 4: Filter Attack short vs long (baseline contour)
        { "4a_filtAtk_short", bCutoff, bRes, bContour, 0.005, bAAtk, bMixDrive, bFiltDrive },
        { "4b_filtAtk_long",  bCutoff, bRes, bContour, 3.5,   bAAtk, bMixDrive, bFiltDrive },
        // 5: Amp Attack short vs long
        { "5a_ampAtk_short",  bCutoff, bRes, bContour, bFAtk, 0.005, bMixDrive, bFiltDrive },
        { "5b_ampAtk_long",   bCutoff, bRes, bContour, bFAtk, 0.5,   bMixDrive, bFiltDrive },
        // 6: Cutoff low vs high
        { "6a_cutoff_low",    250.0,   bRes, bContour, bFAtk, bAAtk, bMixDrive, bFiltDrive },
        { "6b_cutoff_high",   5000.0,  bRes, bContour, bFAtk, bAAtk, bMixDrive, bFiltDrive },
        // 7: Emphasis (resonance) low vs medium
        { "7a_res_low",       bCutoff, 0.10, bContour, bFAtk, bAAtk, bMixDrive, bFiltDrive },
        { "7b_res_medium",    bCutoff, 0.50, bContour, bFAtk, bAAtk, bMixDrive, bFiltDrive },
        // 8: Filter Drive off vs on
        { "8a_filtDrive_off", bCutoff, bRes, bContour, bFAtk, bAAtk, bMixDrive, 0.0 },
        { "8b_filtDrive_on",  bCutoff, bRes, bContour, bFAtk, bAAtk, bMixDrive, 0.50 },
        // 9: Mixer Drive off vs on
        { "9a_mixDrive_off",  bCutoff, bRes, bContour, bFAtk, bAAtk, 0.0,       bFiltDrive },
        { "9b_mixDrive_on",   bCutoff, bRes, bContour, bFAtk, bAAtk, 0.70,      bFiltDrive },
        // Exact reported patch, and everything-off control.
        { "0_reported_patch", bCutoff, bRes, bContour, bFAtk, bAAtk, bMixDrive, bFiltDrive },
        { "0_control_no_drive_no_contour", bCutoff, bRes, 0.0, bFAtk, bAAtk, 0.0, 0.0 },
    };

    for (const auto& cfg : configs)
        runConfig(cfg, sampleRate, blockSize);

    std::cout << "Done.\n";
    return 0;
}
