#pragma once

namespace SynthCore {

// Balanced differential tanh VCA stub (Milestone 6)
class Vca {
public:
    Vca() = default;

    void setSampleRate(double sampleRate);

    // Master output level [0, 1]
    void setLevel(float level);

    // Returns amplitude-scaled sample. Linear passthrough until Milestone 6.
    float process(float in, float envCv);

private:
    double _sampleRate = 44100.0;
    float  _level      = 1.0f;
};

} // namespace SynthCore
