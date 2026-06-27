#pragma once

namespace SynthCore {

class Vca {
public:
    Vca() = default;

    void setSampleRate(double sampleRate);
    void setMasterVolume(double volume);
    void setDrive(double drive);
    void reset();
    double processSample(double input, double envelopeValue);
    void setLevel(float level);
    float process(float in, float envCv);

private:
    double _sampleRate = 44100.0;
    double _masterVolume = 1.0;
    double _drive = 0.0;
};

} // namespace SynthCore
