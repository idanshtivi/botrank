#pragma once
#include "OscillatorBank.h"
#include "NoiseGenerator.h"
#include "Mixer.h"
#include "ContourGenerator.h"
#include "LadderFilter.h"
#include "Vca.h"

namespace SynthCore {

// Per-note signal chain: OscillatorBank → Mixer → LadderFilter → VCA.
// Envelopes (loudness + filter contour) live here so they travel with the note.
// Pitch is fed in each sample from the engine's VoiceController.
class SynthVoice {
public:
    // Called once on startup and on every sample-rate change.
    void prepare(double sampleRate, int blockSize);

    // Trigger/release envelopes. Pitch is controlled externally via processSample args.
    void noteOn(int midiNote, float velocity);
    void noteOff();

    // Run one sample through the entire per-voice chain.
    // pitchHz       – current frequency from the VoiceController (includes glide + bend)
    // currentMidiNote – used by the filter for keyboard-tracking
    float processSample(double pitchHz, double currentMidiNote);

    // True while the loudness contour is still decaying/sustaining.
    bool isActive() const;

    // Zero all internal state (equivalent to hard reset on a single voice).
    void reset();

    // DSP sub-objects are public so SynthEngine can forward parameter changes
    // directly without a proliferation of proxy setters.
    OscillatorBank   oscillators;
    NoiseGenerator   noise;
    Mixer            mixer;
    LadderFilter     ladderFilter;
    ContourGenerator loudnessContour;
    ContourGenerator filterContour;
    Vca              vca;

private:
    double _sampleRate = 44100.0;

    static double _midiFromHz(double hz);
};

} // namespace SynthCore
