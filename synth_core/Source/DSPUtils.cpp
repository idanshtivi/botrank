#include "../Include/DSPUtils.h"
#include <cmath>

namespace SynthCore {

// ─── ParamSmoother ───────────────────────────────────────────────────────────

void ParamSmoother::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
    // Re-derive coefficient from current time constant.
    // We store the inverse sample-rate change so we can re-apply the same tau.
    // Default tau = 5 ms on construction.
    setTimeConstant(0.005);
}

void ParamSmoother::setTimeConstant(double tau)
{
    if (tau <= 0.0 || _sampleRate <= 0.0) {
        _coeff = 0.0f;
        return;
    }
    _coeff = static_cast<float>(std::exp(-1.0 / (tau * _sampleRate)));
}

float ParamSmoother::process(float target)
{
    _z = _coeff * _z + (1.0f - _coeff) * target;
    return _z;
}

// ─── Math helpers ─────────────────────────────────────────────────────────────

namespace Math {

double midiNoteToFrequency(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

} // namespace Math

} // namespace SynthCore
