#pragma once
#include <cstdint>

namespace SynthCore {

// 1-pole IIR parameter smoother – no heap allocation, inline state
class ParamSmoother {
public:
    ParamSmoother() = default;

    // Set time constant in seconds; must be called after every sample-rate change
    void setSampleRate(double sampleRate);

    // tau: smoothing time constant in seconds
    void setTimeConstant(double tau);

    // Feed a new target value and return the smoothed output
    float process(float target);

    float currentValue() const { return _z; }

private:
    float  _z          = 0.0f;
    float  _coeff      = 0.0f;   // 1-pole coefficient = e^(-1/(tau*fs))
    double _sampleRate = 44100.0;
};

// White-noise PRNG (xorshift32 – no heap)
class NoiseGenerator {
public:
    NoiseGenerator() = default;
    void  seed(uint32_t s);
    float whiteNoise();   // uniform [-1, 1]
    float pinkNoise();    // Paul Kellet's approximation

private:
    uint32_t _state = 0x12345678u;
    // Pink-noise state registers
    float _b[7] = {};
};

// Utility math helpers (constexpr-friendly, no dynamic allocation)
namespace Math {
    // Fast tanh approximation (Padé [3/3] – < 2.5% error for |x| < 3, sufficient for audio saturation)
    inline float tanhApprox(float x)
    {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // 1V/oct MIDI note to frequency: f = 440 * 2^((note - 69)/12)
    double midiNoteToFrequency(int midiNote);

    // Clamp value to [lo, hi]
    inline float clamp(float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }
}

} // namespace SynthCore
