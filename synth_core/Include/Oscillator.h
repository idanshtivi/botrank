#pragma once

namespace SynthCore {

enum class Waveform {
    Triangle,
    TriangleSaw,
    Saw,
    ReverseSaw,
    Square,
    WidePulse,
    NarrowPulse,
    Sawtooth = Saw
};

using OscWaveform = Waveform;

enum class OscillatorRange {
    Low,
    ThirtyTwoFoot,
    SixteenFoot,
    EightFoot,
    FourFoot,
    TwoFoot
};

// Band-limited oscillator using the PolyBLEP residual method.
class Oscillator {
public:
    Oscillator();

    void setSampleRate(double sampleRate);
    void setFrequency(double hz);
    void setMidiNote(double note);
    void setWaveform(Waveform w);
    void setRange(OscillatorRange range);
    void setDetuneSemitones(double semitones);
    void setFineTuneCents(double cents);
    void setPulseWidth(double width);
    void setDriftCents(double cents);
    void setKeyboardTrackingEnabled(bool enabled);
    void setOctave(int semitoneOffset);

    float processSample();
    float process();

    void reset();

private:
    double _sampleRate = 44100.0;
    double _frequency = 440.0;
    double _baseMidiNote = 69.0;
    Waveform _waveform = Waveform::Saw;
    OscillatorRange _range = OscillatorRange::EightFoot;
    int _octaveOffset = 0;
    double _detuneSemitones = 0.0;
    double _fineTuneCents = 0.0;
    double _driftCents    = 0.0;
    double _pulseWidth = 0.5;
    bool _keyboardTrackingEnabled = true;

    double _phase = 0.0;
    double _phaseInc = 0.0;
    double _triangleState = 0.0;

    void _updatePhaseInc();
    double _effectiveFrequency() const;
    static int _rangeSemitones(OscillatorRange range);
};

} // namespace SynthCore
