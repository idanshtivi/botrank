#include "../Include/Mixer.h"

namespace SynthCore {

void Mixer::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
}

void Mixer::setOsc1Level(float v)  { _levels[0] = v; }
void Mixer::setOsc2Level(float v)  { _levels[1] = v; }
void Mixer::setOsc3Level(float v)  { _levels[2] = v; }
void Mixer::setNoiseLevel(float v) { _levels[3] = v; }
void Mixer::setExtLevel(float v)   { _levels[4] = v; }

float Mixer::process(float osc1, float osc2, float osc3, float noise, float ext)
{
    // Milestone 3: passive summing bus + common-emitter saturation
    (void)osc1; (void)osc2; (void)osc3; (void)noise; (void)ext;
    return 0.0f;
}

} // namespace SynthCore
