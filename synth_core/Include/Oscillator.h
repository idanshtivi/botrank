#pragma once
#include <cstdint>

namespace SynthCore {

// Milestone 1: sawtooth-only, PolyBLEP anti-aliased, double-precision phase
// accumulator. Triangle and Square waveforms are declared for API stability;
// they are implemented in Milestone 2.
enum class OscWaveform { Sawtooth, Triangle, Square };

class Oscillator {
public:
    Oscillator() = default;

    // Must be called before first process(); safe to call mid-stream —
    // recomputes phaseIncrement without resetting phase.
    void setSampleRate(double sampleRate);

    // Set base frequency in Hz. Applied immediately (sample-accurate).
    void setFrequency(double hz);

    // Semitone transpose applied on top of the base frequency.
    // Range: any integer; ±24 covers two octaves either side.
    void setOctave(int semitoneOffset);

    // Waveform selector. Only Sawtooth is rendered in Milestone 1;
    // Triangle and Square fall through to sawtooth until Milestone 2.
    void setWaveform(OscWaveform w);

    // Advance one sample and return the band-limited output in [-1, 1].
    float process();

    // Hard-reset phase to 0.0 (use for hard-sync; not analog soft-reset).
    void reset();

    // Read-only accessors used by tests
    double frequency()   const { return _frequency; }
    double phase()       const { return _phase; }
    double sampleRate()  const { return _sampleRate; }

private:
    double      _sampleRate    = 44100.0;
    double      _frequency     = 440.0;
    int         _semitones     = 0;       // transpose in semitones
    OscWaveform _waveform      = OscWaveform::Sawtooth;

    double      _phase         = 0.0;    // [0, 1)
    double      _phaseInc      = 0.0;    // advance per sample = f / fs

    void   _updatePhaseInc();

    // PolyBLEP residual for a discontinuity at phase t (normalised [0,1))
    // using a two-sample kernel. dt = phaseIncrement.
    static double _polyBlep(double t, double dt);
};

} // namespace SynthCore
