// Offline diagnostic probe: drives SynthEngine (no JUCE, no real-time audio)
// through scripted note sequences designed to exercise each voice-allocation
// reason (FreeSlot, ReleaseTail, SameNote, Oldest) and replicates the exact
// crackle-detection logic from PluginProcessor::processBlock so the resulting
// PolyTraceLogger CSVs can be correlated against AllocReason offline.
//
// This is throwaway investigation tooling for the held-voice-steal click
// hypothesis — not part of the shipped product.
#include "../Include/SynthEngine.h"
#include "../Include/PolyTraceLogger.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if LADDERVOICE_ENABLE_POLY_TRACE

using namespace SynthCore;

namespace {

struct MidiEvent {
    int   frame;
    bool  isOn;
    int   note;
    float velocity;
};

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
        pcm[i * 2] = s;
        pcm[i * 2 + 1] = s;
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

// Stage-isolation config: which parts of the per-voice signal chain
// (Oscillators -> Mixer[+Drive] -> LadderFilter[+Drive] -> VCA) are engaged.
// Used to bisect exactly where in the chain a click first appears.
struct StageConfig {
    bool   bypassFilter   = false; // skip LadderFilter::processSample entirely
    double mixerDrive     = 0.0;   // Mixer saturation amount
    double filterDrive    = 0.0;   // LadderFilter's own drive/saturation amount
    double resonance      = 0.10;
    double filterEnvAmount = 0.0;
    bool   twoOscillators = true;  // osc1+osc2 (realistic arpeggio timbre) vs osc1 only
};

// Runs one scenario end-to-end: schedules MIDI events sample-accurately,
// renders in the given block size, and pushes Block/Crackle/Snapshot trace
// events using the identical detection thresholds PluginProcessor uses.
void runScenario(const std::string& name, double sampleRate, int blockSize,
                  const StageConfig& stage, const std::vector<MidiEvent>& events, int totalFrames)
{
    std::cout << "== Scenario: " << name << "  (sr=" << sampleRate
              << " block=" << blockSize << " bypassFilter=" << stage.bypassFilter
              << " mixerDrive=" << stage.mixerDrive << " filterDrive=" << stage.filterDrive
              << " resonance=" << stage.resonance << ") ==\n";

    SynthEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setParameter(ParamId::PlayMode, 1.0f); // poly
    engine.setParameter(ParamId::FilterResonance, static_cast<float>(stage.resonance));
    engine.setParameter(ParamId::Osc2Enabled, stage.twoOscillators ? 1.0f : 0.0f);
    engine.setParameter(ParamId::Osc3Enabled, 0.0f);
    engine.setParameter(ParamId::NoiseEnabled, 0.0f);
    engine.setParameter(ParamId::MixerDrive, static_cast<float>(stage.mixerDrive));
    engine.setParameter(ParamId::FilterDrive, static_cast<float>(stage.filterDrive));
    engine.setParameter(ParamId::FilterEnvAmount, static_cast<float>(stage.filterEnvAmount));
    engine.setDebugBypassFilter(stage.bypassFilter);

    auto& logger = PolyTraceLogger::instance();

    size_t   evIdx = 0;
    uint64_t sessionSample = 0;
    uint64_t blockIndex = 0;
    float    tracePrev = 0.0f;
    uint32_t crackleIndex = 0;
    uint64_t lastNoteEventSample = 0;

    std::vector<float> wavBuf;
    wavBuf.reserve(static_cast<size_t>(totalFrames));

    std::vector<float> blockOut(static_cast<size_t>(blockSize));

    for (int blockStart = 0; blockStart < totalFrames; blockStart += blockSize) {
        const int thisBlockSize = std::min(blockSize, totalFrames - blockStart);
        POLY_TRACE_SET_CTX(blockIndex, sessionSample, static_cast<uint32_t>(thisBlockSize),
                            static_cast<float>(sampleRate));

        for (int i = 0; i < thisBlockSize; ++i) {
            const int globalFrame = blockStart + i;
            while (evIdx < events.size() && events[evIdx].frame == globalFrame) {
                const auto& ev = events[evIdx];
                if (ev.isOn) engine.noteOn(ev.note, ev.velocity);
                else engine.noteOff(ev.note);
                lastNoteEventSample = sessionSample + static_cast<uint64_t>(i);
                ++evIdx;
            }
            blockOut[static_cast<size_t>(i)] = engine.processSample();
        }
        wavBuf.insert(wavBuf.end(), blockOut.begin(), blockOut.begin() + thisBlockSize);

        // ---- Replicate PluginProcessor's block scan + crackle detection ----
        float maxDelta = 0.0f;
        int   maxDeltaOff = 0;
        float prev = tracePrev;
        float peak = 0.0f, sumSq = 0.0f;
        for (int i = 0; i < thisBlockSize; ++i) {
            const float s = blockOut[static_cast<size_t>(i)];
            const float d = std::abs(s - prev);
            if (d > maxDelta) { maxDelta = d; maxDeltaOff = i; }
            const float a = std::abs(s);
            if (a > peak) peak = a;
            sumSq += s * s;
            prev = s;
        }
        tracePrev = prev;
        const float blockRms = std::sqrt(sumSq / static_cast<float>(thisBlockSize));
        const float avgDelta = logger.recentAvgDelta();
        logger.updateRecentAvgDelta(maxDelta);

        SnapshotPayload blockSnaps[kPolyVoiceCount];
        engine.fillVoiceSnapshots(blockSnaps, 0);
        uint8_t blockActive = 0, blockHeld = 0, blockReleasing = 0;
        for (int vi = 0; vi < kPolyVoiceCount; ++vi) {
            if (blockSnaps[vi].isActive) { ++blockActive; if (!blockSnaps[vi].isHeld) ++blockReleasing; }
            if (blockSnaps[vi].isHeld) ++blockHeld;
        }

        BlockPayload bp{};
        bp.sessionSample = sessionSample;
        bp.blockIndex = blockIndex;
        bp.blockSize = static_cast<uint32_t>(thisBlockSize);
        bp.sampleRate = static_cast<float>(sampleRate);
        bp.playMode = static_cast<uint8_t>(engine.playModeForTrace());
        bp.activeVoices = blockActive;
        bp.heldVoices = blockHeld;
        bp.releasingVoices = blockReleasing;
        bp.finalPeak = peak;
        bp.finalRms = blockRms;
        bp.maxDelta = maxDelta;
        bp.maxDeltaOffset = static_cast<uint16_t>(maxDeltaOff < 65535 ? maxDeltaOff : 65535);
        bp.blockTimeUs = 0;
        bp.cpuPercent = 0.0f;
        bp.droppedEvents = logger.droppedCount();
        logger.pushBlock(bp);

        const bool aboveAbs = maxDelta > PolyTraceLogger::kAbsoluteThreshold;
        const bool aboveRatio = avgDelta > 0.002f && maxDelta > avgDelta * PolyTraceLogger::kRatioThreshold;
        if (aboveAbs || aboveRatio) {
            float prevAtMax = 0.0f, curAtMax = 0.0f;
            if (maxDeltaOff > 0) {
                prevAtMax = blockOut[static_cast<size_t>(maxDeltaOff - 1)];
                curAtMax = blockOut[static_cast<size_t>(maxDeltaOff)];
            } else if (thisBlockSize > 1) {
                prevAtMax = tracePrev;
                curAtMax = blockOut[0];
            }

            SnapshotPayload snaps[kPolyVoiceCount];
            engine.fillVoiceSnapshots(snaps, crackleIndex);
            uint8_t activeCount = 0, heldCount = 0, releasingCount = 0;
            for (int vi = 0; vi < kPolyVoiceCount; ++vi) {
                if (snaps[vi].isActive) { ++activeCount; if (!snaps[vi].isHeld) ++releasingCount; }
                if (snaps[vi].isHeld) ++heldCount;
            }

            CracklePayload cp{};
            cp.sessionSample = sessionSample + static_cast<uint64_t>(maxDeltaOff);
            cp.blockIndex = blockIndex;
            cp.sampleOffset = static_cast<uint32_t>(maxDeltaOff);
            cp.crackleIndex = crackleIndex;
            cp.maxDelta = maxDelta;
            cp.prevSample = prevAtMax;
            cp.curSample = curAtMax;
            cp.recentAvgDelta = avgDelta;
            cp.activeVoices = activeCount;
            cp.heldVoices = heldCount;
            cp.releasingVoices = releasingCount;
            cp.playModeAtCrackle = static_cast<uint8_t>(engine.playModeForTrace());
            cp.lastNoteEventSample = lastNoteEventSample;
            cp.droppedEvents = logger.droppedCount();
            cp.monoVoiceActive = engine.monoVoiceActiveForTrace() ? 1u : 0u;
            cp.monoLoudnessValue = static_cast<float>(engine.monoLoudnessValueForTrace());
            cp.monoFilterValue = static_cast<float>(engine.monoFilterValueForTrace());
            logger.pushCrackle(cp);
            for (int vi = 0; vi < kPolyVoiceCount; ++vi) logger.pushSnapshot(snaps[vi]);
            ++crackleIndex;
        }

        sessionSample += static_cast<uint64_t>(thisBlockSize);
        ++blockIndex;
    }

    const std::string wavPath = "poly_click_probe_" + name + ".wav";
    writeWav(wavPath, wavBuf, sampleRate);

    // Raw per-sample dump (sample index, value) for direct waveform inspection —
    // the delta-threshold "crackle" detector can't tell a real discontinuity
    // from a normal steep (but legitimate) waveform edge, so isolated scenarios
    // get their full sample trace written out for offline analysis.
    const std::string rawPath = "poly_click_probe_" + name + "_raw.csv";
    std::ofstream rawF(rawPath);
    rawF << "sample,value\n";
    rawF.precision(9);
    for (size_t i = 0; i < wavBuf.size(); ++i)
        rawF << i << ',' << wavBuf[i] << '\n';

    std::cout << "  crackles detected: " << crackleIndex << "   wav: " << wavPath
               << "   raw: " << rawPath << "\n";
}

// ---- Scenario builders ----------------------------------------------------

// Holds a 2-note chord (never released), then fires a rapid arpeggio of new
// notes on top without releasing any of them either. Poly voice count is 4,
// so the chord (2) + first 2 arpeggio notes fill all 4 slots; every further
// note-on forces the "Oldest" (held-voice steal) allocation path.
std::vector<MidiEvent> scenarioHeldStealArpeggio(double sampleRate)
{
    std::vector<MidiEvent> ev;
    const int stepFrames = static_cast<int>(sampleRate * 0.012); // 12 ms between arp notes — fast
    ev.push_back({ static_cast<int>(sampleRate * 0.05), true, 60, 100.0f });
    ev.push_back({ static_cast<int>(sampleRate * 0.05) + 5, true, 64, 100.0f });
    int frame = static_cast<int>(sampleRate * 0.20);
    const int notes[] = { 67, 71, 74, 79, 62, 69, 65, 72, 76, 81, 68, 73 };
    for (int n : notes) {
        ev.push_back({ frame, true, n, 100.0f });
        frame += stepFrames;
    }
    return ev;
}

} // namespace

int main()
{
#ifdef _MSC_VER
    _putenv_s("LADDERVOICE_POLY_TRACE", "1");
#else
    setenv("LADDERVOICE_POLY_TRACE", "1", 1);
#endif
    PolyTraceLogger::instance().start();
    if (!PolyTraceLogger::instance().isActive()) {
        std::cerr << "ERROR: PolyTraceLogger failed to start (env var / directory issue).\n";
        return 1;
    }
    std::cout << "Trace session dir: " << PolyTraceLogger::instance().sessionDirectory() << "\n";

    const double sampleRate = 44100.0;
    const int totalFrames = static_cast<int>(sampleRate * 2.5);
    const auto arp = scenarioHeldStealArpeggio(sampleRate); // same fast arpeggio for every config

    // ---- Stage-isolation A/B: same note schedule, same block size, only
    // the DSP stages engaged differ. The goal is to find which stage's
    // *inclusion* changes the discontinuity at the known Oldest-steal
    // sample offsets (from voice_allocations.csv) relative to that same
    // config's own steady-state baseline. ----

    StageConfig oscOnly;
    oscOnly.bypassFilter = true;
    oscOnly.mixerDrive = 0.0; oscOnly.filterDrive = 0.0;
    oscOnly.resonance = 0.10; // irrelevant when filter is bypassed
    runScenario("stage1_osc_only", sampleRate, 256, oscOnly, arp, totalFrames);

    StageConfig oscPlusFilter;
    oscPlusFilter.bypassFilter = false;
    oscPlusFilter.mixerDrive = 0.0; oscPlusFilter.filterDrive = 0.0;
    oscPlusFilter.resonance = 0.10;
    runScenario("stage2_osc_plus_filter", sampleRate, 256, oscPlusFilter, arp, totalFrames);

    StageConfig oscPlusFilterHighRes = oscPlusFilter;
    oscPlusFilterHighRes.resonance = 0.70;
    runScenario("stage2b_osc_plus_filter_highres", sampleRate, 256, oscPlusFilterHighRes, arp, totalFrames);

    StageConfig oscPlusDrive;
    oscPlusDrive.bypassFilter = true; // filter skipped, only mixer drive engaged
    oscPlusDrive.mixerDrive = 1.0; oscPlusDrive.filterDrive = 0.0;
    oscPlusDrive.resonance = 0.10;
    runScenario("stage3_osc_plus_drive", sampleRate, 256, oscPlusDrive, arp, totalFrames);

    StageConfig fullChain;
    fullChain.bypassFilter = false;
    fullChain.mixerDrive = 1.0; fullChain.filterDrive = 0.5;
    fullChain.resonance = 0.10;
    runScenario("stage4_full_chain", sampleRate, 256, fullChain, arp, totalFrames);

    StageConfig fullChainHighRes = fullChain;
    fullChainHighRes.resonance = 0.70;
    runScenario("stage4b_full_chain_highres", sampleRate, 256, fullChainHighRes, arp, totalFrames);

    PolyTraceLogger::instance().stop();
    std::cout << "Done.\n";
    return 0;
}

#else // LADDERVOICE_ENABLE_POLY_TRACE == 0

int main()
{
    std::cout << "poly_click_probe: build with -DLADDERVOICE_ENABLE_POLY_TRACE=ON to run this probe.\n";
    return 0;
}

#endif
