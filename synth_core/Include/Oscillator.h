#pragma once

namespace SynthCore {

// Oscillator waveform selection – waveform generation deferred to Milestone 2
enum class OscWaveform { Sawtooth, Triangle, Square };

// Stub placeholder for Milestone 1 implementation.
// Declares the full interface; all audio-generating methods return silence.
class Oscillator {
public:
    Oscillator() = default;

    void setSampleRate(double sampleRate);
    void setFrequency(double hz);
    void setWaveform(OscWaveform w);
    void setOctave(int semitoneOffset);  // +/-24 semitones

    // Returns next sample [-1, 1]. Outputs silence until Milestone 1.
    float process();

    void reset();

private:
    double _sampleRate = 44100.0;
    double _frequency  = 440.0;
    OscWaveform _waveform = OscWaveform::Sawtooth;
    int    _octaveOffset  = 0;

    // Phase accumulator (populated in Milestone 1)
    double _phase = 0.0;
};

} // namespace SynthCore
