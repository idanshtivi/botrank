#pragma once

#include "Oscillator.h"
#include <cstdint>

namespace SynthCore {

struct OscillatorBankOutput {
    float osc1 = 0.0f;
    float osc2 = 0.0f;
    float osc3 = 0.0f;
};

class OscillatorBank {
public:
    void setSampleRate(double sampleRate);
    void setBaseMidiNote(double note);
    void setPitchBendSemitones(double semitones);
    void setOscillatorWaveform(int oscIndex, Waveform waveform);
    void setOscillatorRange(int oscIndex, OscillatorRange range);
    void setOscillatorDetuneSemitones(int oscIndex, double semitones);
    void setOscillatorFineTuneCents(int oscIndex, double cents);
    void setOscillatorPulseWidth(int oscIndex, double width);
    void setOscillator3KeyboardTrackingEnabled(bool enabled);
    void setAnalogDrift(double maxCents);
    void randomizePhases(uint32_t seed); // set independent random start phase per oscillator
    void reset();
    OscillatorBankOutput process();

private:
    Oscillator _osc1;
    Oscillator _osc2;
    Oscillator _osc3;
    double _baseMidiNote = 69.0;
    double _pitchBendSemitones = 0.0;

    // Analog drift: slow random walk per oscillator
    double   _driftMaxCents   = 0.0;
    double   _driftCents[3]   = {0.0, 0.0, 0.0};
    uint32_t _driftRng        = 0xdeadbeef;
    int      _driftCounter    = 0;

    Oscillator* _oscillator(int oscIndex);
    const Oscillator* _oscillator(int oscIndex) const;
    void _updatePitch();
};

} // namespace SynthCore
