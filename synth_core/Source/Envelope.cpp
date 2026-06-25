#include "../Include/Envelope.h"

namespace SynthCore {

void Envelope::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
}

void Envelope::setAttack (float seconds) { _attack  = seconds; }
void Envelope::setDecay  (float seconds) { _decay   = seconds; }
void Envelope::setSustain(float level)   { _sustain = level;   }
void Envelope::setRelease(float seconds) { _release = seconds; }

void Envelope::gate(bool on)
{
    _gateHeld = on;
    if (on) {
        _stage = Stage::Attack;
    } else if (_stage != Stage::Idle) {
        _stage = Stage::Release;
    }
}

float Envelope::process()
{
    // Milestone 5: state-dependent RC charge/discharge curves
    return 0.0f;
}

} // namespace SynthCore
