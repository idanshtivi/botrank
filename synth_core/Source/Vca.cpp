#include "../Include/Vca.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

void Vca::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
}

void Vca::setMasterVolume(double volume)
{
    if (!std::isfinite(volume)) volume = 0.0;
    _masterVolume = std::clamp(volume, 0.0, 1.0);
}

void Vca::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 0.0;
    _drive = std::clamp(drive, 0.0, 4.0);
}

void Vca::reset()
{
}

double Vca::processSample(double input, double envelopeValue)
{
    if (!std::isfinite(input)) input = 0.0;
    if (!std::isfinite(envelopeValue)) envelopeValue = 0.0;

    const double gain = std::clamp(envelopeValue, 0.0, 1.0) * _masterVolume;
    double out = input * gain;
    if (_drive > 0.0001) {
        const double denom = std::tanh(_drive);
        out = denom > 0.0001 ? std::tanh(_drive * out) / denom : out;
    }

    if (!std::isfinite(out)) out = 0.0;
    return std::clamp(out, -1.0, 1.0);
}

void Vca::setLevel(float level)
{
    setMasterVolume(level);
}

float Vca::process(float in, float envCv)
{
    return static_cast<float>(processSample(in, envCv));
}

} // namespace SynthCore
