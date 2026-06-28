#pragma once

namespace SynthCore {

enum class MixerSource {
    Osc1 = 0,
    Osc2,
    Osc3,
    Noise,
    ExternalInput,
    Count
};

class Mixer {
public:
    Mixer() = default;

    void setSampleRate(double sampleRate);
    void setSourceEnabled(MixerSource source, bool enabled);
    void setSourceLevel(MixerSource source, double level);
    void setDrive(double drive);
    void reset();
    double processSample(double osc1, double osc2, double osc3, double noise, double ext);

    void setOsc1Level(float v);
    void setOsc2Level(float v);
    void setOsc3Level(float v);
    void setNoiseLevel(float v);
    void setExtLevel(float v);
    float process(float osc1, float osc2, float osc3, float noise, float ext);

private:
    double _sampleRate    = 44100.0;
    double _levels[static_cast<int>(MixerSource::Count)] = {0.7, 0.4, 0.0, 0.0, 0.0};
    bool   _enabled[static_cast<int>(MixerSource::Count)] = {true, true, false, false, false};
    double _drive           = 1.0;
    double _driveSmoothed   = 1.0;  // 1-pole LP toward _drive; eliminates zipper noise
    double _driveAlpha      = 0.0;  // coefficient = exp(-1/(tau*fs))
    bool   _driveNeedsSnap  = true; // snap on first processSample after init/reset

    static int _index(MixerSource source);
};

} // namespace SynthCore
