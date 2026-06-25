#pragma once

namespace SynthCore {

// 4-pole ZDF transistor ladder filter stub (full model deferred to Milestone 4)
class Filter {
public:
    Filter() = default;

    void setSampleRate(double sampleRate);

    // Cutoff frequency in Hz
    void setCutoff(double hz);

    // Resonance [0, 1]; self-oscillation threshold ~0.9
    void setResonance(float r);

    // Envelope modulation amount [-1, 1]
    void setEnvAmount(float amt);

    // Returns filtered sample. Passes through dry until Milestone 4.
    float process(float in, float envCv);

private:
    double _sampleRate = 44100.0;
    double _cutoff     = 1000.0;
    float  _resonance  = 0.0f;
    float  _envAmount  = 0.0f;
};

} // namespace SynthCore
