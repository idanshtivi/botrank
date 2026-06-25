#pragma once

namespace SynthCore {

// Passive resistive summing + common-emitter pre-amp saturation stub
// (full model deferred to Milestone 3)
class Mixer {
public:
    Mixer() = default;

    void setSampleRate(double sampleRate);

    // Level controls [0, 1] for each source
    void setOsc1Level(float v);
    void setOsc2Level(float v);
    void setOsc3Level(float v);
    void setNoiseLevel(float v);
    void setExtLevel(float v);

    // Returns mixed+saturated sample. Outputs silence until Milestone 3.
    float process(float osc1, float osc2, float osc3, float noise, float ext);

private:
    double _sampleRate = 44100.0;
    float  _levels[5]  = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f};
};

} // namespace SynthCore
