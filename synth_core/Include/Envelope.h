#pragma once

namespace SynthCore {

// Discrete RC ADSR contour – state-dependent charging (Milestone 5)
class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    Envelope() = default;

    void setSampleRate(double sampleRate);

    // Times in seconds
    void setAttack (float seconds);
    void setDecay  (float seconds);
    void setSustain(float level);    // [0, 1]
    void setRelease(float seconds);

    void gate(bool on);   // high = key held, low = key released

    // Returns envelope value [0, 1]. Outputs 0 until Milestone 5.
    float process();

    Stage currentStage() const { return _stage; }

private:
    double _sampleRate = 44100.0;
    float  _attack     = 0.01f;
    float  _decay      = 0.1f;
    float  _sustain    = 0.7f;
    float  _release    = 0.3f;

    Stage  _stage      = Stage::Idle;
    float  _value      = 0.0f;
    bool   _gateHeld   = false;
};

} // namespace SynthCore
