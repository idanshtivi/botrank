#include "../Include/DSPUtils.h"
#include <cmath>
#include <algorithm>

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

// ─── NoiseGenerator ──────────────────────────────────────────────────────────

void NoiseGenerator::seed(uint32_t s)
{
    _state = (s == 0u) ? 1u : s;
    for (auto& b : _b) b = 0.0f;
}

float NoiseGenerator::whiteNoise()
{
    // xorshift32
    _state ^= _state << 13u;
    _state ^= _state >> 17u;
    _state ^= _state << 5u;
    // Map to [-1, 1]
    return static_cast<float>(_state) * (1.0f / 2147483648.0f) - 1.0f;
}

// Paul Kellet's pink-noise filter bank (no heap)
float NoiseGenerator::pinkNoise()
{
    float white = whiteNoise();
    _b[0] = 0.99886f * _b[0] + white * 0.0555179f;
    _b[1] = 0.99332f * _b[1] + white * 0.0750759f;
    _b[2] = 0.96900f * _b[2] + white * 0.1538520f;
    _b[3] = 0.86650f * _b[3] + white * 0.3104856f;
    _b[4] = 0.55000f * _b[4] + white * 0.5329522f;
    _b[5] = -0.7616f * _b[5] - white * 0.0168980f;
    float pink = (_b[0] + _b[1] + _b[2] + _b[3] + _b[4] + _b[5] + _b[6] + white * 0.5362f) * 0.11f;
    _b[6] = white * 0.115926f;
    return pink;
}

// ─── Math helpers ─────────────────────────────────────────────────────────────

namespace Math {

double midiNoteToFrequency(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

} // namespace Math

} // namespace SynthCore
