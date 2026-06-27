#pragma once

namespace SynthCore {

class ContourGenerator {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sampleRate);
    void setAttackSeconds(double seconds);
    void setDecaySeconds(double seconds);
    void setSustainLevel(double level);
    void setReleaseSeconds(double seconds);
    void setDecayActsAsRelease(bool enabled);
    void gateOn();
    void gateOff();
    void trigger();
    void reset();
    double processSample();
    bool isActive() const;
    double getCurrentValue() const { return _value; }
    Stage currentStage() const { return _stage; }

private:
    double _sampleRate = 44100.0;
    double _attackSeconds = 0.005;
    double _decaySeconds = 0.25;
    double _sustainLevel = 0.8;
    double _releaseSeconds = 0.25;
    bool _decayActsAsRelease = false;
    bool _gateHigh = false;
    double _value = 0.0;
    Stage _stage = Stage::Idle;

    static double _clampTime(double seconds);
    double _stepFor(double seconds) const;
};

} // namespace SynthCore
