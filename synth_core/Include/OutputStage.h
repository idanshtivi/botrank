#pragma once

namespace SynthCore {

// Class-A output amplifier + coupling capacitor HP filter stub (Milestone 8)
class OutputStage {
public:
    OutputStage() = default;

    void setSampleRate(double sampleRate);

    // Coupling capacitor value: true = 47µF, false = 10µF
    void setCapLarge(bool large);

    // Returns output sample. Passes through dry until Milestone 8.
    float process(float in);

    void reset();

private:
    double _sampleRate  = 44100.0;
    bool   _capLarge    = true;

    // HP state for coupling capacitor simulation
    float  _hpState     = 0.0f;
    float  _hpCoeff     = 0.0f;

    void _updateHpCoeff();
};

} // namespace SynthCore
