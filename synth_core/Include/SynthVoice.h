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

    // Reset filter + DC-blocker state on a fresh note start (safe when VCA was 0).
    void resetAudioChainState();

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

    // DC-blocking HP filter between mixer and ladder filter.
    // Drive asymmetry (x² terms) adds DC that passes through the LP filter
    // unchanged and appears as a click when the VCA envelope opens.
    double _dcBlockX = 0.0;
    double _dcBlockY = 0.0;
    double _dcBlockCoeff = 0.9993; // ~5 Hz at 44100 Hz

    // Pitch de-click smoother: eliminates PolyBLEP correction discontinuities
    // when frequency changes while the VCA is open (note transitions, voice stealing).
    // Fresh note starts snap immediately; active transitions smooth over ~1.5 ms.
    double _pitchSmoothedHz    = 440.0;
    double _pitchDeclickCoeff  = 0.0;   // computed in prepare()
    bool   _pitchIsFirstSample = false; // snap pitch on first active sample of fresh note

    // Fresh voice start de-click: a short output fade prevents the first active
    // sample of a newly allocated poly voice from entering as a discontinuity.
    double _startDeclickGain = 1.0;
    double _startDeclickStep = 1.0;

    static double _midiFromHz(double hz);
};

} // namespace SynthCore
