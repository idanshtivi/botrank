#pragma once

namespace SynthCore {

class LadderFilter {
public:
    void setSampleRate(double sampleRate);
    void setCutoffHz(double cutoffHz);
    void setResonance(double resonance);
    void setContourAmount(double amount);
    void setKeyboardTrackingAmount(double amount);
    void setDrive(double drive);
    void reset();
    double processSample(double input, double filterEnvelopeValue, double keyboardMidiNote);

private:
    double _sampleRate = 44100.0;
    double _cutoffHz = 5000.0;
    double _resonance = 0.12;
    double _contourAmount = 0.25;
    double _keyboardTrackingAmount = 0.0;
    double _drive = 0.0;
    double _stage[4] = {0.0, 0.0, 0.0, 0.0};

    double _effectiveCutoff(double filterEnvelopeValue, double keyboardMidiNote) const;
};

} // namespace SynthCore
