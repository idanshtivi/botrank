#include "../Include/Vca.h"

namespace SynthCore {

void Vca::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
}

void Vca::setLevel(float level)
{
    _level = level;
}

float Vca::process(float in, float envCv)
{
    // Milestone 6: balanced differential tanh saturation
    return in * envCv * _level;
}

} // namespace SynthCore
