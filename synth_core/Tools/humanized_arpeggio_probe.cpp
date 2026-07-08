// Offline diagnostic probe: reproduces a humanized fast-arpeggio performance
// (2 held notes + rapid overlapping notes on top, with human-like timing
// jitter) through SynthEngine, using the NORMAL production signal path --
// no PolyTraceLogger, no crackle-delta heuristic. Detection is done with a
// paired A/B differential render: for each note-on that is forced to steal
// an already-HELD voice (all 4 poly voices occupied by currently-held
// notes), we render the schedule once normally and once with that single
// note event removed (so the voice that would have been stolen just keeps
// playing its original note). Because both renders are bit-deterministic
// and identical up to the moment that note would have fired, any nonzero
// difference after that point is attributable *exactly* to that one
// note-on -- not to ordinary waveform edges or resonance ringing elsewhere,
// which are identical in both renders and cancel out completely.
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

struct NoteInstance {
    int onFrame;
    int offFrame;
    int note;
};

struct MidiEvent {
    int  frame;
    bool isOn;
    int  note;
};

uint32_t xorshift32(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
// Deterministic uniform double in [lo, hi)
double jitter(uint32_t& rng, double lo, double hi) {
    const double u = static_cast<double>(xorshift32(rng) & 0x00FFFFFFu) / static_cast<double>(0x01000000u);
    return lo + u * (hi - lo);
}

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

// Builds a humanized performance: 2 pad notes held for the whole duration,
// plus a fast arpeggio layered on top with per-note timing jitter and
// randomized legato overlap (note-off of arp note i lands AFTER arp note
// i+1 has already started, like real overlapping fingers), occasionally
// staccato (a gap) for realism. Deterministic given (bpm, seedBase).
std::vector<NoteInstance> buildHumanizedPerformance(double sampleRate, double bpm,
                                                     double durationSeconds, uint32_t seedBase)
{
    std::vector<NoteInstance> notes;
    uint32_t rng = seedBase;

    const int padA = 48, padB = 55; // C3, G3 -- held continuously
    const int padOnFrame = static_cast<int>(sampleRate * 0.05);
    const int padOffFrame = static_cast<int>(sampleRate * durationSeconds);
    notes.push_back({ padOnFrame, padOffFrame, padA });
    notes.push_back({ padOnFrame + 3, padOffFrame, padB });

    const double sixteenth = 60.0 / bpm / 4.0; // seconds per 16th note
    const int arpPattern[] = { 60, 64, 67, 72, 76, 79, 84, 79, 76, 72, 67, 64 };
    const int patternLen = static_cast<int>(sizeof(arpPattern) / sizeof(arpPattern[0]));

    double onsetTime = 0.20;
    int patternIdx = 0;
    std::vector<double> onsetTimes;
    std::vector<int> onsetNotes;
    while (onsetTime < durationSeconds - 0.05) {
        onsetTimes.push_back(onsetTime);
        onsetNotes.push_back(arpPattern[patternIdx % patternLen]);
        ++patternIdx;
        // Human timing: +/-18% jitter around the nominal 16th-note interval.
        const double interval = sixteenth * (1.0 + jitter(rng, -0.18, 0.18));
        onsetTime += interval;
    }

    for (size_t i = 0; i < onsetTimes.size(); ++i) {
        const double onset = onsetTimes[i];
        double offset;
        if (i + 1 < onsetTimes.size()) {
            const double nextOnset = onsetTimes[i + 1];
            const double gap = nextOnset - onset;
            const double r = jitter(rng, 0.0, 1.0);
            if (r < 0.45) {
                // "Sloppy hold": a finger lags behind and stays down through
                // several subsequent notes -- real fast/imprecise playing
                // frequently stacks up 3-5 held notes at once, not just 2.
                const size_t overlapNotes = 2 + static_cast<size_t>(jitter(rng, 0.0, 3.0)); // 2..4 notes ahead
                const size_t targetIdx = std::min(onsetTimes.size() - 1, i + overlapNotes);
                offset = (targetIdx > i) ? onsetTimes[targetIdx] + jitter(rng, 0.0, sixteenth * 0.4)
                                          : nextOnset + gap * jitter(rng, 0.5, 1.5);
            } else if (r < 0.80) {
                // Ordinary legato: release lands just after the next onset.
                offset = nextOnset + gap * jitter(rng, 0.15, 0.65);
            } else {
                // Staccato: release before the next onset, small gap.
                offset = onset + gap * jitter(rng, 0.35, 0.85);
            }
        } else {
            offset = onset + sixteenth * 1.5;
        }
        const int onFrame = static_cast<int>(sampleRate * onset);
        const int offFrame = std::max(onFrame + 4, static_cast<int>(sampleRate * offset));
        notes.push_back({ onFrame, offFrame, onsetNotes[i] });
    }

    return notes;
}

// How many notes are already held (on, not yet off) at time `atFrame`,
// counting only notes whose onFrame < atFrame -- used to identify which
// arp note-on events are forced to steal an already-held voice (i.e. all
// kPolyVoiceCount slots already occupied by currently-held notes).
int heldCountBefore(const std::vector<NoteInstance>& notes, size_t excludeIdx, int atFrame)
{
    int count = 0;
    for (size_t i = 0; i < notes.size(); ++i) {
        if (i == excludeIdx) continue;
        if (notes[i].onFrame < atFrame && notes[i].offFrame > atFrame) ++count;
    }
    return count;
}

std::vector<MidiEvent> toFlatEvents(const std::vector<NoteInstance>& notes, int skipIdx = -1)
{
    std::vector<MidiEvent> ev;
    for (size_t i = 0; i < notes.size(); ++i) {
        if (static_cast<int>(i) == skipIdx) continue;
        ev.push_back({ notes[i].onFrame, true, notes[i].note });
        ev.push_back({ notes[i].offFrame, false, notes[i].note });
    }
    std::sort(ev.begin(), ev.end(), [](const MidiEvent& a, const MidiEvent& b) {
        if (a.frame != b.frame) return a.frame < b.frame;
        return a.isOn && !b.isOn; // on before off at the same frame
    });
    return ev;
}

// Renders the schedule sample-accurately (matching PluginProcessor's
// renderUntil pattern) at the given buffer size, through the mono voice's
// engine using the normal signal path. Returns the full mono render.
std::vector<float> render(const std::vector<MidiEvent>& events, double sampleRate,
                           int blockSize, int totalFrames)
{
    SynthEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setParameter(ParamId::PlayMode, 1.0f); // poly

    std::vector<float> out(static_cast<size_t>(totalFrames), 0.0f);
    size_t evIdx = 0;
    for (int blockStart = 0; blockStart < totalFrames; blockStart += blockSize) {
        const int thisBlockSize = std::min(blockSize, totalFrames - blockStart);
        for (int i = 0; i < thisBlockSize; ++i) {
            const int globalFrame = blockStart + i;
            while (evIdx < events.size() && events[evIdx].frame == globalFrame) {
                const auto& e = events[evIdx];
                if (e.isOn) engine.noteOn(e.note, 100.0f);
                else engine.noteOff(e.note);
                ++evIdx;
            }
            out[static_cast<size_t>(globalFrame)] = engine.processSample();
        }
    }
    return out;
}

void testCombo(double bpm, int blockSize, double sampleRate, int comboSeed)
{
    const double durationSeconds = 3.0;
    const int totalFrames = static_cast<int>(sampleRate * durationSeconds);

    auto notes = buildHumanizedPerformance(sampleRate, bpm, durationSeconds,
                                            0x9E3779B9u ^ static_cast<uint32_t>(comboSeed));

    // Find forced-Oldest candidates: arp note-on events where kPolyVoiceCount
    // (4) other notes are already held at that instant.
    std::vector<size_t> forcedOldest;
    for (size_t i = 0; i < notes.size(); ++i) {
        if (heldCountBefore(notes, i, notes[i].onFrame) >= kPolyVoiceCount)
            forcedOldest.push_back(i);
    }

    std::cout << "== bpm=" << bpm << " block=" << blockSize
              << "  notes=" << notes.size()
              << "  forced-Oldest candidates=" << forcedOldest.size() << " ==\n";

    if (forcedOldest.empty()) {
        std::cout << "  (no forced held-voice steal in this schedule; skipping diff test)\n";
        return;
    }

    // Full normal render (for reference / WAV export).
    const auto normalEvents = toFlatEvents(notes);
    const auto normalRender = render(normalEvents, sampleRate, blockSize, totalFrames);

    const std::string tag = "bpm" + std::to_string(static_cast<int>(bpm)) + "_bs" + std::to_string(blockSize);
    writeWav("humanized_arp_" + tag + ".wav", normalRender, sampleRate);

    // Full raw per-sample dump of the actual (normal) render -- needed so a
    // proper self-continuity check has real pre-event baseline context,
    // not just the +/-few-hundred-sample window around one event.
    {
        std::ofstream rawF("humanized_arp_" + tag + "_raw.csv");
        rawF.precision(9);
        rawF << "sample,value\n";
        for (size_t i = 0; i < normalRender.size(); ++i)
            rawF << i << ',' << normalRender[i] << '\n';
    }

    // Test up to the first 3 forced-Oldest events per combo (keeps runtime
    // bounded while still covering more than one steal per schedule).
    const size_t maxToTest = std::min<size_t>(3, forcedOldest.size());
    for (size_t k = 0; k < maxToTest; ++k) {
        const size_t idx = forcedOldest[k];
        const int onFrame = notes[idx].onFrame;

        // Counterfactual: identical schedule with just this one note instance
        // removed (the voice it would have stolen just keeps playing).
        const auto cfEvents = toFlatEvents(notes, static_cast<int>(idx));
        const auto cfRender = render(cfEvents, sampleRate, blockSize, totalFrames);

        // Sanity: renders must be bit-identical before onFrame (determinism check).
        bool identicalBefore = true;
        for (int s = 0; s < onFrame && s < totalFrames; ++s) {
            if (normalRender[static_cast<size_t>(s)] != cfRender[static_cast<size_t>(s)]) {
                identicalBefore = false;
                break;
            }
        }

        const int winStart = std::max(0, onFrame - 5);
        const int winEnd = std::min(totalFrames, onFrame + 400);
        float maxDiff = 0.0f;
        int maxDiffOffset = 0;
        for (int s = winStart; s < winEnd; ++s) {
            const float d = std::abs(normalRender[static_cast<size_t>(s)] - cfRender[static_cast<size_t>(s)]);
            if (d > maxDiff) { maxDiff = d; maxDiffOffset = s - onFrame; }
        }

        std::cout << "  steal#" << k << " note=" << notes[idx].note
                  << " onFrame=" << onFrame
                  << " identicalBeforeEvent=" << (identicalBefore ? "yes" : "NO(!!)")
                  << " maxDiff=" << maxDiff << " at offset " << maxDiffOffset << "\n";

        {
            std::ofstream mf("humanized_arp_events_manifest.csv", std::ios::app);
            mf << tag << ',' << k << ',' << onFrame << ',' << notes[idx].note << '\n';
        }

        // Dump the diff trace around the event for manual inspection.
        const std::string diffPath = "humanized_arp_" + tag + "_steal" + std::to_string(k) + "_diff.csv";
        std::ofstream df(diffPath);
        df.precision(9);
        df << "offset,normal,counterfactual,diff\n";
        for (int s = winStart; s < winEnd; ++s) {
            df << (s - onFrame) << ',' << normalRender[static_cast<size_t>(s)] << ','
               << cfRender[static_cast<size_t>(s)] << ','
               << (normalRender[static_cast<size_t>(s)] - cfRender[static_cast<size_t>(s)]) << '\n';
        }
    }
}

} // namespace

int main()
{
    {
        std::ofstream mf("humanized_arp_events_manifest.csv", std::ios::trunc);
        mf << "tag,stealIndex,onFrame,note\n";
    }

    const double sampleRate = 44100.0;
    const double bpms[] = { 120.0, 140.0, 160.0, 180.0 };
    const int blockSizes[] = { 64, 128, 256, 512, 1024 };

    int comboSeed = 0;
    for (double bpm : bpms) {
        for (int bs : blockSizes) {
            testCombo(bpm, bs, sampleRate, comboSeed);
            ++comboSeed;
        }
    }

    std::cout << "Done.\n";
    return 0;
}
