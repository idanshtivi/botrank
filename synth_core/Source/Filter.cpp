#include "../Include/Filter.h"

namespace SynthCore {

void Filter::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
}

void Filter::setCutoff(double hz)
{
    _cutoff = hz;
}

void Filter::setResonance(float r)
{
    _resonance = r;
}

void Filter::setEnvAmount(float amt)
{
    _envAmount = amt;
}

float Filter::process(float in, float /*envCv*/)
{
    // Milestone 4: ZDF transistor ladder matrix solver
    return in;
}

} // namespace SynthCore
