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

    // Two-component preGain: 'early' gives immediate response at Drive 1;
    // 'late' adds extra push in the upper range.
    const double early   = std::pow(amount, 0.72);
    const double late    = smoothstep01(0.35, 1.0, amount);
    const double preGain = 1.0 + 3.8 * early + 2.3 * late;   // max ~7.1x at Drive 3
    const double x       = input * preGain;

    // Slightly relaxed from V4's 2.35 — lets Drive 3 breathe while staying poly-safe.
    const double xp = std::clamp(x, -2.60, 2.60);

    // Warm layer: normalised so slope ≈ 1 at origin — clean feel at low drive.
    const double tanhNorm = std::tanh(0.92);
    const double warm     = std::tanh(xp * 0.92) / tanhNorm;

    // Edge layer: slightly richer than V4, still controlled.
    const double edgeAmount = smoothstep01(0.40, 1.0, amount);
    const double asym       = 0.017 * amount;
    const double cubic      = 0.016 + 0.040 * edgeAmount;
    const double bias       = 0.004 * amount;

    const double shaped = xp + asym * xp * xp + cubic * xp * xp * xp + bias;
    double edge = std::tanh(shaped);
    edge -= std::tanh(bias);

    // Progressive blend: capped at 0.88 so Drive 3 stays musical, not harsh.
    const double edgeMixRaw = 0.10 + 0.24 * early + 0.54 * edgeAmount;
    const double edgeMix    = std::min(clamp01(edgeMixRaw), 0.88);
    double wet = lerp(warm, edge, edgeMix);

    wet = antiFizz(input, wet, amount);

    // Body preserve: keeps low-mid presence as drive increases.
    double y = wet + input * (0.030 * amount);

    // Slightly lighter trim — Drive 3 must not feel smaller than Drive 2.
    const double trim = 1.0 / (1.0 + 0.028 * (preGain - 1.0));
    y *= trim;

    return softLimit(y, 1.05);
}

} // namespace DriveUtils

} // namespace SynthCore
