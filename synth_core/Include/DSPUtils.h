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

inline double removeBias(double value, double bias)
{
    return value - std::tanh(bias);
}

// Character drive used by internal output color and filter input push.
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

// Filter Drive: a stronger pre-ladder exciter. This keeps the ladder topology and
// cutoff/resonance math unchanged, but feeds it a genuinely reshaped signal so
// the control is heard as color, compression, and bite instead of level.
inline double filterDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double curve = std::pow(amount, 0.96);
    const double push = 1.0 + 5.6 * curve + 2.4 * amount;
    const double x = std::clamp(input * push, -5.0, 5.0);

    const double bodyLimit = 1.10 - 0.18 * amount;
    const double body = bodyLimit * std::tanh(x / bodyLimit);

    const double fold = std::tanh(0.55 * x);
    const double bitePre = x
                         + (0.085 + 0.230 * curve) * x * x * x
                         - (0.045 * curve) * fold * fold * fold;
    const double bias = 0.010 * curve;
    const double asym = 0.070 * curve + 0.030 * amount;
    const double edgeIn = std::clamp(bitePre + asym * x * x + bias, -7.0, 7.0);
    const double edge = removeBias(std::tanh(edgeIn), bias);

    const double highPush = smoothstep01(0.55, 1.0, amount);
    const double bite = (edge - body) * (1.0 + 0.45 * highPush);
    const double biteMix = clamp01(0.18 + 0.78 * std::pow(amount, 0.82));
    const double dryBody = 0.18 * (1.0 - amount) + 0.060 * amount;
    double y = body + biteMix * bite + dryBody * input;

    y = antiFizz(input, y, amount);

    const double outputLift = 1.0 + 0.86 * curve;
    const double trim = 1.0 / (1.0 + 0.035 * (push - 1.0));
    return softLimit(y * outputLift * trim, 1.12);
}

// Mixer/Main Drive: parallel modern harmonic exciter inside the existing mixer
// drive block. It keeps Drive 0 dry, adds warm body, extracts an upper-harmonic
// layer from a stronger asymmetric/cubic shaper, then blends that layer back in.
inline double mixerDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double driveCurve = std::pow(amount, 0.52);

    // Warm/body layer: smooth saturation with moderate gain. Dividing by
    // bodyGain keeps weight and movement without turning the layer into volume.
    const double bodyGain = 1.0 + 4.2 * driveCurve + 1.5 * amount;
    const double bodyX = std::clamp(input * bodyGain, -5.0, 5.0);
    const double warm = std::tanh(bodyX) / (1.0 + 0.10 * (bodyGain - 1.0));

    // Exciter layer: higher gain plus asymmetry and cubic curvature. The
    // difference between edge and warm is the modern shine / upper-harmonic layer.
    const double edgeGain = 1.0 + 12.5 * driveCurve + 4.8 * amount;
    const double edgeX = std::clamp(input * edgeGain, -4.0, 4.0);
    const double asym = 0.075 * driveCurve + 0.034 * amount;
    const double cubic = 0.090 + 0.300 * driveCurve;
    const double bias = 0.008 * driveCurve;
    const double edgePre = edgeX + asym * edgeX * edgeX + cubic * edgeX * edgeX * edgeX + bias;
    double edge = std::tanh(edgePre) - std::tanh(bias);
    edge /= (1.0 + 0.012 * edgeGain);

    const double highPush = smoothstep01(0.60, 1.0, amount);
    const double harmonicLayer = (edge - warm) * (1.0 + 0.35 * highPush);
    const double harmonicBlend = clamp01(0.24 + 0.92 * std::pow(amount, 0.62));
    const double bodyPreserve = 0.28 * (1.0 - amount) + 0.070 * amount;
    double modern = warm + harmonicBlend * harmonicLayer + bodyPreserve * input;

    modern = antiFizz(input, modern, amount);
    modern *= 1.0 + 1.85 * driveCurve;
    return softLimit(modern, 1.18);
}

} // namespace DriveUtils

} // namespace SynthCore
