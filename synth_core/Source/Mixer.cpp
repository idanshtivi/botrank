#include "../Include/Mixer.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

int Mixer::_index(MixerSource source)
{
    const int idx = static_cast<int>(source);
    return (idx >= 0 && idx < static_cast<int>(MixerSource::Count)) ? idx : 0;
}

void Mixer::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
}

void Mixer::setSourceEnabled(MixerSource source, bool enabled)
{
    _enabled[_index(source)] = enabled;
}

void Mixer::setSourceLevel(MixerSource source, double level)
{
    if (!std::isfinite(level)) level = 0.0;
    _levels[_index(source)] = std::clamp(level, 0.0, 1.0);
}

void Mixer::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 1.6;
    _drive = std::clamp(drive, 0.0, 8.0);
}

void Mixer::reset()
{
}

double Mixer::processSample(double osc1, double osc2, double osc3, double noise, double ext)
{
    const double inputs[static_cast<int>(MixerSource::Count)] = {osc1, osc2, osc3, noise, ext};
    double sum = 0.0;

    for (int i = 0; i < static_cast<int>(MixerSource::Count); ++i) {
        const double sample = std::isfinite(inputs[i]) ? inputs[i] : 0.0;
        if (_enabled[i]) {
            sum += sample * _levels[i];
        }
    }

    constexpr double headroom = 0.45;
    const double x = sum * headroom;
    double out = x;
    if (_drive > 0.0001) {
        const double denom = std::tanh(_drive);
        out = denom > 0.0001 ? std::tanh(_drive * x) / denom : x;
    }

    if (!std::isfinite(out)) out = 0.0;
    return std::clamp(out, -1.0, 1.0);
}

void Mixer::setOsc1Level(float v)  { setSourceLevel(MixerSource::Osc1, v); }
void Mixer::setOsc2Level(float v)  { setSourceLevel(MixerSource::Osc2, v); }
void Mixer::setOsc3Level(float v)  { setSourceLevel(MixerSource::Osc3, v); }
void Mixer::setNoiseLevel(float v) { setSourceLevel(MixerSource::Noise, v); }
void Mixer::setExtLevel(float v)   { setSourceLevel(MixerSource::ExternalInput, v); }

float Mixer::process(float osc1, float osc2, float osc3, float noise, float ext)
{
    return static_cast<float>(processSample(osc1, osc2, osc3, noise, ext));
}

} // namespace SynthCore
