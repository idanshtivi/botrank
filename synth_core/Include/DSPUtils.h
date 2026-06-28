#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace SynthCore {

// 1-pole IIR parameter smoother - no heap allocation, inline state
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

// Minimal deterministic RNG - xorshift32 step only.
// Minimoog-style white/pink noise generation is deferred to the Mixer milestone.
inline uint32_t xorshift32(uint32_t& state)
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

// Utility math helpers (constexpr-friendly, no dynamic allocation)
namespace Math {
    // Fast tanh approximation (Pade [3/3] - < 2.5% error for |x| < 3, sufficient for audio saturation)
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

// Drive-stage utilities - shared across Mixer, LadderFilter, and OutputStage
namespace DriveUtils {

inline double clamp01(double x)
{
    return std::max(0.0, std::min(1.0, x));
}

inline double normDrive(double uiValue)
{
    return clamp01(uiValue / 3.0);
}

inline double smoothstep01(double edge0, double edge1, double x)
{
    const double t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

inline double lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

inline double softLimit(double x, double limit)
{
    limit = std::max(0.1, limit);
    return limit * std::tanh(x / limit);
}

inline double antiFizz(double input, double shaped, double amount)
{
    const double tame = smoothstep01(0.42, 1.0, amount);
    const double hiDelta = shaped - input;
    return shaped - (0.10 * tame) * hiDelta * hiDelta * hiDelta;
}

// Character drive: warm body at low settings, extra edge as the knob rises.
// amount in [0,1] (use normDrive to convert UI value 0..3).
// Coefficients are deliberately conservative to avoid inter-oscillator
// intermodulation products that would create LFO-like pumping.
inline double mainDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double driveCurve = std::pow(amount, 1.12);
    const double preGain = 1.0 + 7.2 * driveCurve;
    const double x = input * preGain;

    const double presence = x + (0.055 * amount) * (x - std::tanh(0.72 * x) / 0.72);
    const double warmLimit = 1.04 - 0.12 * amount;
    const double warm = warmLimit * std::tanh(presence / warmLimit);

    const double bias = 0.012 * amount;
    const double asym = 0.026 * amount;
    const double edgeInput = presence + asym * presence * presence
                           + (0.030 + 0.060 * amount) * presence * presence * presence
                           + bias;
    double edge = std::tanh(edgeInput);
    edge -= std::tanh(bias);

    const double edgeMix = smoothstep01(0.28, 0.95, amount);
    double y = lerp(warm, edge, edgeMix);

    const double bodyMix = 0.08 * amount * (1.0 - 0.35 * edgeMix);
    y = lerp(y, input, bodyMix);
    y = antiFizz(input, y, amount);

    const double trim = 1.0 / (1.0 + 0.105 * (preGain - 1.0));
    return y * trim;
}

inline double mixerDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double driveCurve = std::pow(amount, 0.82);
    const double preGain = 1.0 + 12.5 * driveCurve;
    const double x = input * preGain;

    const double pushed = x + (0.10 * amount) * (x - std::tanh(0.58 * x) / 0.58);
    const double warm = (1.08 - 0.10 * amount) * std::tanh(pushed / (1.08 - 0.10 * amount));

    const double bias = 0.020 * amount;
    const double asym = 0.044 * amount;
    const double edgeInput = pushed + asym * pushed * pushed
                           + (0.050 + 0.110 * amount) * pushed * pushed * pushed
                           + bias;
    double edge = std::tanh(edgeInput);
    edge -= std::tanh(bias);

    const double edgeMix = smoothstep01(0.18, 0.88, amount);
    double y = lerp(warm, edge, edgeMix);

    const double bodyMix = 0.055 * amount * (1.0 - 0.25 * edgeMix);
    y = lerp(y, input, bodyMix);
    y = antiFizz(input, y, amount);

    const double trim = 1.0 / (1.0 + 0.078 * (preGain - 1.0));
    return y * trim;
}

} // namespace DriveUtils

} // namespace SynthCore
