// Offline diagnostic probe: directly inspects the loudness-contour envelope
// VALUE (not an audio-derived proxy) around rapid alternating-note
// retriggers with a long Amp Release -- reproducing what was confirmed live
// ("long release + fast alternating fingers like an arpeggiator" -> click).
//
// Hypothesis under test: every retrigger calls ContourGenerator::gateOn(),
// which resets _stage to Attack but does NOT reset _value -- so the envelope
// VALUE is continuous (matches every offline/live finding so far: no value
// jump anywhere). But gateOn() unconditionally starts incrementing toward 1.0
// again regardless of what the value/slope was doing a moment before (e.g.
// mid-Release, falling). That is a discontinuity in the envelope's SLOPE
// (derivative) at the exact retrigger instant, even though the value itself
// is continuous -- and if retriggers repeat fast enough (matching real
// arpeggiator-style playing), these slope-kinks recur at an audible rate,
// independent of Attack time (a longer Attack ramp still kinks immediately
// at the retrigger instant; it just changes direction over a different
// span afterward). This would explain why every value-discontinuity search
// so far has failed while the click remains audible and live-reproducible.
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

int main()
{
    const double sampleRate = 44100.0;
    const int blockSize = 256;
    const double durationSeconds = 3.0;
    const int totalFrames = static_cast<int>(sampleRate * durationSeconds);

    SynthEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.setParameter(ParamId::PlayMode, 1.0f); // poly4

    // Match the live "Frozen Cathedral Drone" repro parameters that produced
    // the confirmed click: long Amp Release, moderate Amp Attack.
    engine.setParameter(ParamId::AmpAttack, 0.009f);
    engine.setParameter(ParamId::AmpDecay, 0.005f);
    engine.setParameter(ParamId::AmpSustain, 1.00f);
    engine.setParameter(ParamId::AmpRelease, 4.00f);
    engine.setParameter(ParamId::FilterAttack, 0.028f);
    engine.setParameter(ParamId::FilterDecay, 1.00f);
    engine.setParameter(ParamId::FilterSustain, 0.90f);
    engine.setParameter(ParamId::FilterRelease, 3.50f);
    engine.setParameter(ParamId::FilterCutoff, 1200.0f);
    engine.setParameter(ParamId::FilterResonance, 0.06f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.10f);
    engine.setParameter(ParamId::MixerDrive, 0.45f);

    // Alternating two-note "arpeggiator finger" pattern matching the live
    // log: NoteOn A, NoteOff A ~80ms later, NoteOn B ~10ms after that, etc.
    const int noteA = 62, noteB = 64;
    struct Ev { int frame; bool on; int note; };
    std::vector<Ev> events;
    double t = 0.10;
    const double period = 0.080; // ~80ms between onsets, matching the live capture
    for (int i = 0; i < 24; ++i) {
        const int note = (i % 2 == 0) ? noteA : noteB;
        const int onFrame = static_cast<int>(sampleRate * t);
        const int offFrame = onFrame + static_cast<int>(sampleRate * (period - 0.010));
        events.push_back({ onFrame, true, note });
        events.push_back({ offFrame, false, note });
        t += period;
    }
    std::sort(events.begin(), events.end(), [](const Ev& a, const Ev& b) {
        if (a.frame != b.frame) return a.frame < b.frame;
        return a.on && !b.on;
    });

    std::ofstream envF("envelope_kink_probe.csv");
    envF.precision(9);
    envF << "sample,output,env_voice0,env_voice1,env_voice2,env_voice3,noteEvent\n";

    size_t evIdx = 0;
    for (int frame = 0; frame < totalFrames; ++frame) {
        std::string tag;
        while (evIdx < events.size() && events[evIdx].frame == frame) {
            const auto& e = events[evIdx];
            if (e.on) { engine.noteOn(e.note, 100.0f); tag = std::string("ON") + std::to_string(e.note); }
            else      { engine.noteOff(e.note);        tag = std::string("OFF") + std::to_string(e.note); }
            ++evIdx;
        }
        const float out = engine.processSample();
        envF << frame << ',' << out;
        for (int v = 0; v < 4; ++v)
            envF << ',' << engine.getVoiceLoudnessValue(v);
        envF << ',' << tag << '\n';
    }

    std::cout << "Done. Wrote envelope_kink_probe.csv (" << totalFrames << " samples)\n";
    return 0;
}
