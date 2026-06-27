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
    bool wasHeld = _gateHeld;
    _gateHeld = on;
    if (on && !wasHeld) {
        _stage = Stage::Attack;
    } else if (!on && wasHeld && _stage != Stage::Idle) {
        _stage = Stage::Release;
    }
}

float Envelope::process()
{
    auto rateFor = [this](float secs) -> float {
        return (secs > 0.0f && _sampleRate > 0.0)
               ? 1.0f / (static_cast<float>(_sampleRate) * secs)
               : 1.0f;
    };

    switch (_stage) {
    case Stage::Idle:
        _value = 0.0f;
        break;
    case Stage::Attack:
        _value += rateFor(_attack);
        if (_value >= 1.0f) { _value = 1.0f; _stage = Stage::Decay; }
        break;
    case Stage::Decay:
        _value -= rateFor(_decay);
        if (_value <= _sustain) { _value = _sustain; _stage = Stage::Sustain; }
        break;
    case Stage::Sustain:
        _value = _sustain;
        break;
    case Stage::Release:
        _value -= rateFor(_release);
        if (_value <= 0.0f) { _value = 0.0f; _stage = Stage::Idle; }
        break;
    }
    return _value;
}

} // namespace SynthCore
