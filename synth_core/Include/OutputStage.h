#pragma once

namespace SynthCore {

class OutputStage {
public:
    OutputStage() = default;

    void setSampleRate(double sampleRate);
    void setMasterVolume(double volume);
    void setDrive(double drive);
    void setReferenceToneEnabled(bool enabled);
    void setCapLarge(bool large);
    double processSample(double input);
    float process(float in);
    void reset();

private:
    double _sampleRate = 44100.0;
    double _masterVolume = 0.6;
    double _drive = 0.0;
    bool _referenceToneEnabled = false;
    double _referencePhase = 0.0;
    bool _capLarge = true;
    double _dcPrevInput = 0.0;
    double _dcPrevOutput = 0.0;
    double _hpCoeff = 0.995;

    void _updateHpCoeff();
};

} // namespace SynthCore
