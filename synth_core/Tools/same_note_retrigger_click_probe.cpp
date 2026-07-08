// Offline diagnostic probe: "Frozen Cathedral Drone"-style patch, repeated
// single-note tapping. Investigates SameNote vs ReleaseTail voice reuse for
// a REPEATED note (not a different note), specifically because this patch
// uses TWO DETUNED TRIANGLE oscillators -- a waveform/code path none of the
// earlier probes exercised (they all used the default Saw). The triangle
// generator is a leaky integrator whose per-sample increment depends on the
// current phase increment (frequency); unlike the direct-computed Saw/Pulse
// waveforms, an instantaneous frequency change is not obviously value-
// continuous for this integrator, which only earlier probes would have
// missed entirely.
//
// Normal (non-PolyTrace) signal path; RtEventLog used only for ground-truth
// allocation reasons (SameNote/ReleaseTail), not as a crackle detector.
// Every render is a single repeatedly-tapped note -- no chords, no distinct
// notes, so poly voice stealing across different pitches cannot be involved.
//
// This is throwaway investigation tooling -- not part of the shipped product.
#include "../Include/SynthEngine.h"
#include "../Include/RtEventLog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
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

struct TapEvent { int frame; bool isOn; };

// Builds a repeated-tap schedule for ONE note. If releaseGapMs >= 0, each tap
// is released `releaseGapMs` after its own onset (so the next tap arrives
// with the previous instance already off -- ReleaseTail territory once
// Amp/Filter Release is long). If releaseGapMs < 0, the note is never
// released between taps at all (still held when retriggered -- SameNote).
std::vector<TapEvent> buildTapSchedule(double sampleRate, int tapCount, double tapPeriodSec,
                                        double releaseGapMs)
{
    std::vector<TapEvent> ev;
    double t = 0.10;
    for (int i = 0; i < tapCount; ++i) {
        const int onFrame = static_cast<int>(sampleRate * t);
        ev.push_back({ onFrame, true });
        if (releaseGapMs >= 0.0) {
            const int offFrame = onFrame + static_cast<int>(sampleRate * releaseGapMs / 1000.0);
            ev.push_back({ offFrame, false });
        }
        t += tapPeriodSec;
    }
    if (releaseGapMs >= 0.0) {
        // final release handled above; nothing else to add
    } else {
        // never explicitly released -- release long after the last tap
        ev.push_back({ static_cast<int>(sampleRate * (t + 1.0)), false });
    }
    return ev;
}

struct Config {
    std::string name;
    int playMode; // 0 = mono, 1 = poly4
    double ampAttack, ampDecay, ampSustain, ampRelease;
    double filterAttack, filterDecay, filterSustain, filterRelease;
    double releaseGapMs; // >=0: release this long after each tap (-> ReleaseTail territory); <0: never release (-> SameNote)
    double tapPeriodSec = 0.30; // time between successive tap onsets
};

void applyFrozenCathedralPatch(SynthEngine& engine)
{
    engine.setParameter(ParamId::Osc1Enabled, 1.0f);
    engine.setParameter(ParamId::Osc1Waveform, 0.0f); // Triangle
    engine.setParameter(ParamId::Osc1Range, 2.0f);    // 16'
    engine.setParameter(ParamId::Osc1Level, 0.60f);
    engine.setParameter(ParamId::Osc2Enabled, 1.0f);
    engine.setParameter(ParamId::Osc2Waveform, 0.0f); // Triangle
    engine.setParameter(ParamId::Osc2Range, 3.0f);    // 8'
    engine.setParameter(ParamId::Osc2Detune, 0.06f);
    engine.setParameter(ParamId::Osc2Level, 0.50f);
    engine.setParameter(ParamId::Osc3Enabled, 0.0f);
    engine.setParameter(ParamId::Osc3Level, 0.0f);
    engine.setParameter(ParamId::NoiseEnabled, 0.0f);
    engine.setParameter(ParamId::MixerDrive, 0.45f);
    engine.setParameter(ParamId::FilterCutoff, 1200.0f);
    engine.setParameter(ParamId::FilterResonance, 0.06f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.10f);
    engine.setParameter(ParamId::FilterDrive, 0.0f);
    engine.setParameter(ParamId::LfoAmount, 0.0f);
    engine.setParameter(ParamId::ModWheelAmount, 0.0f);
}

void runConfig(const Config& cfg, double sampleRate, int blockSize)
{
    const int tapCount = 10;
    const double tapPeriodSec = cfg.tapPeriodSec;
    const double durationSeconds = tapCount * tapPeriodSec + 2.0;
    const int totalFrames = static_cast<int>(sampleRate * durationSeconds);
    const int note = 55; // G3 -- comfortably audible register, matches a drone-style patch

    SynthEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setParameter(ParamId::PlayMode, static_cast<float>(cfg.playMode));
    applyFrozenCathedralPatch(engine);
    engine.setParameter(ParamId::AmpAttack, static_cast<float>(cfg.ampAttack));
    engine.setParameter(ParamId::AmpDecay, static_cast<float>(cfg.ampDecay));
    engine.setParameter(ParamId::AmpSustain, static_cast<float>(cfg.ampSustain));
    engine.setParameter(ParamId::AmpRelease, static_cast<float>(cfg.ampRelease));
    engine.setParameter(ParamId::FilterAttack, static_cast<float>(cfg.filterAttack));
    engine.setParameter(ParamId::FilterDecay, static_cast<float>(cfg.filterDecay));
    engine.setParameter(ParamId::FilterSustain, static_cast<float>(cfg.filterSustain));
    engine.setParameter(ParamId::FilterRelease, static_cast<float>(cfg.filterRelease));

    auto& rtLog = RtEventLog::instance();
    rtLog.start();
    if (!rtLog.isActive()) {
        std::cerr << "ERROR: RtEventLog not active -- set LADDERVOICE_RT_LOG=1\n";
        return;
    }

    auto schedule = buildTapSchedule(sampleRate, tapCount, tapPeriodSec, cfg.releaseGapMs);

    std::vector<float> out(static_cast<size_t>(totalFrames), 0.0f);
    size_t evIdx = 0;
    uint64_t sessionSample = 0;
    uint64_t blockIndex = 0;
    for (int blockStart = 0; blockStart < totalFrames; blockStart += blockSize) {
        const int thisBlockSize = std::min(blockSize, totalFrames - blockStart);
        rtLog.setBlockContext(blockIndex, sessionSample);
        for (int i = 0; i < thisBlockSize; ++i) {
            const int globalFrame = blockStart + i;
            while (evIdx < schedule.size() && schedule[evIdx].frame == globalFrame) {
                if (schedule[evIdx].isOn) engine.noteOn(note, 100.0f);
                else engine.noteOff(note);
                ++evIdx;
            }
            out[static_cast<size_t>(globalFrame)] = engine.processSample();
        }
        sessionSample += static_cast<uint64_t>(thisBlockSize);
        ++blockIndex;
    }

    rtLog.stop();
    writeWav("same_note_probe_" + cfg.name + ".wav", out, sampleRate);

    {
        std::ofstream rawF("same_note_probe_" + cfg.name + "_raw.csv");
        rawF.precision(9);
        rawF << "sample,value\n";
        for (int i = 0; i < totalFrames; ++i)
            rawF << i << ',' << out[static_cast<size_t>(i)] << '\n';
    }

    // Read back ground-truth VoiceAlloc reasons for each NoteOn.
    std::ifstream csv(rtLog.sessionDirectory() + "/rt_events.csv");
    std::string line;
    std::getline(csv, line);
    struct AllocEvt { uint64_t sessionSample; std::string reason; };
    std::vector<AllocEvt> allocEvents;
    while (std::getline(csv, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> f;
        while (std::getline(ss, field, ',')) f.push_back(field);
        if (f.size() < 9) continue;
        if (f[4] == "VoiceAlloc")
            allocEvents.push_back({ static_cast<uint64_t>(std::stoull(f[1])), f[8] });
    }

    // For mono mode there's no VoiceAlloc event at all (that logging only
    // fires in the poly branch) -- fall back to the tap's own onFrame with
    // reason label "Mono".
    std::vector<std::pair<int, std::string>> tapsWithReason;
    if (cfg.playMode == 0) {
        for (const auto& e : schedule) {
            if (e.isOn) tapsWithReason.emplace_back(e.frame, "Mono");
        }
    } else {
        for (const auto& a : allocEvents)
            tapsWithReason.emplace_back(static_cast<int>(a.sessionSample), a.reason);
    }

    std::cout << "\n=== config=" << cfg.name << " (playMode=" << cfg.playMode
              << " ampRel=" << cfg.ampRelease << " ampAtk=" << cfg.ampAttack
              << " releaseGapMs=" << cfg.releaseGapMs << ") ===\n";

    for (size_t k = 1; k < tapsWithReason.size(); ++k) { // skip tap #0 (fresh from silence)
        const int t = tapsWithReason[k].first;
        const std::string& reason = tapsWithReason[k].second;

        // Baseline: residual just before this tap (still within the previous
        // tap's decaying/sustaining tail -- the actual "normal" texture this
        // retrigger interrupts).
        const int bStart = std::max(3, t - 2000);
        const int bEnd = std::max(bStart + 1, t - 100);
        double bmax = 0.0;
        for (int i = bStart; i < bEnd; ++i)
            bmax = std::max(bmax, std::abs(out[static_cast<size_t>(i)] - predict(out, i)));

        // Test window: EXACTLY the first 20ms after this NoteOn, as requested.
        const int wEnd = std::min(totalFrames - 1, t + static_cast<int>(sampleRate * 0.020));
        double worst = 0.0; int worstOffSamples = 0;
        for (int i = std::max(3, t); i < wEnd; ++i) {
            const double r = std::abs(out[static_cast<size_t>(i)] - predict(out, i));
            if (r > worst) { worst = r; worstOffSamples = i - t; }
        }
        const double ratio = worst / std::max(bmax, 1e-9);
        const double tailValueAtTap = (t > 0) ? out[static_cast<size_t>(t - 1)] : 0.0;

        // Hard-jump check: is there a literal single-sample discontinuity
        // AT the retrigger boundary (not just curvature somewhere in the
        // 20ms window)? Compares the actual delta at t, t+1, t+2 against the
        // typical (median) sample-to-sample delta in the 200 samples just
        // before the tap -- immune to the "fast legitimate oscillation
        // elsewhere in the window" false positives the residual metric hit.
        std::vector<double> localDeltas;
        for (int i = std::max(1, t - 200); i < t; ++i)
            localDeltas.push_back(std::abs(out[static_cast<size_t>(i)] - out[static_cast<size_t>(i - 1)]));
        std::sort(localDeltas.begin(), localDeltas.end());
        const double typicalDelta = localDeltas.empty() ? 0.0 : localDeltas[localDeltas.size() / 2];
        double maxBoundaryDelta = 0.0; int boundaryOff = 0;
        for (int d = 0; d <= 3; ++d) {
            const int i = t + d;
            if (i < 1 || i >= totalFrames) continue;
            const double delta = std::abs(out[static_cast<size_t>(i)] - out[static_cast<size_t>(i - 1)]);
            if (delta > maxBoundaryDelta) { maxBoundaryDelta = delta; boundaryOff = d; }
        }
        const double jumpRatio = maxBoundaryDelta / std::max(typicalDelta, 1e-9);

        std::cout << "  tap#" << k << " reason=" << reason << " t=" << t
                  << "  tailValueAtTap=" << tailValueAtTap
                  << "  worst(0-20ms)=" << worst << " at +" << worstOffSamples << "smp"
                  << "  baselineMax=" << bmax << "  ratio=" << ratio
                  << (ratio > 3.0 && worst > 0.005 ? "  <==CURVE_FLAG" : "")
                  << "  |  boundaryDelta=" << maxBoundaryDelta << " at +" << boundaryOff
                  << " vs typicalDelta=" << typicalDelta << " jumpRatio=" << jumpRatio
                  << (jumpRatio > 5.0 && maxBoundaryDelta > 0.003 ? "  <==HARD_JUMP" : "") << "\n";
    }
}

} // namespace

int main()
{
#ifdef _MSC_VER
    _putenv_s("LADDERVOICE_RT_LOG", "1");
#else
    setenv("LADDERVOICE_RT_LOG", "1", 1);
#endif

    const double sampleRate = 44100.0;
    const int blockSize = 256;

    // Reported patch defaults.
    const double rAtk = 0.009, rDec = 0.005, rSus = 1.00, rRel = 4.00;
    const double rFAtk = 0.028, rFDec = 1.00, rFSus = 0.90, rFRel = 3.50;

    const std::vector<Config> configs = {
        // 1/3: SameNote (never released between taps) vs ReleaseTail (released
        // 60ms after each tap, well within the 4s Amp Release / 3.5s Filter Release)
        { "A_poly_SameNote_reported",     1, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, -1.0 },
        { "B_poly_ReleaseTail_reported",  1, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, 60.0 },
        // 6: mono vs poly4, same tap pattern (mono always "retriggers" the
        // single voice; legato/retrigger params left at engine defaults)
        { "C_mono_reported",              0, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, 60.0 },
        // 7: Amp Release short vs long (ReleaseTail path)
        { "D_poly_ReleaseTail_ampRel50ms",1, rAtk, rDec, rSus, 0.05, rFAtk, rFDec, rFSus, rFRel, 60.0 },
        { "E_poly_ReleaseTail_ampRel4s",  1, rAtk, rDec, rSus, 4.00, rFAtk, rFDec, rFSus, rFRel, 60.0 },
        // 8: Attack 9ms vs 100ms vs 500ms (SameNote path, since that's most sensitive to envelope restart)
        { "F_poly_SameNote_atk9ms",       1, 0.009, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, -1.0 },
        { "G_poly_SameNote_atk100ms",     1, 0.100, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, -1.0 },
        { "H_poly_SameNote_atk500ms",     1, 0.500, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, -1.0 },
        // Faster tapping: 150ms and 90ms between onsets (genuine fast
        // repeated tapping), with a short 25ms press before release so the
        // key genuinely "taps" rather than holds -- both SameNote (release
        // gap disabled, note held through) and ReleaseTail (quick tap-release)
        // variants at each speed.
        { "I_poly_SameNote_fast150ms",    1, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, -1.0, 0.15 },
        { "J_poly_ReleaseTail_fast150ms", 1, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, 25.0, 0.15 },
        { "K_poly_SameNote_fast90ms",     1, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, -1.0, 0.09 },
        { "L_poly_ReleaseTail_fast90ms",  1, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, 25.0, 0.09 },
        { "M_mono_fast90ms",              0, rAtk, rDec, rSus, rRel, rFAtk, rFDec, rFSus, rFRel, 25.0, 0.09 },
    };

    for (const auto& cfg : configs) {
        runConfig(cfg, sampleRate, blockSize);
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    }

    std::cout << "\nDone.\n";
    return 0;
}
