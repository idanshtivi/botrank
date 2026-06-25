#include <gtest/gtest.h>
#include "../Include/DSPUtils.h"
#include <cmath>

using namespace SynthCore;

// ═══════════════════════════════════════════════════════════════════════════════
// ParamSmoother
// ═══════════════════════════════════════════════════════════════════════════════

// After a step input, value must converge to within 1% of target within 5× tau.
TEST(ParamSmootherTest, ConvergesWithinTimeConstant)
{
    ParamSmoother smoother;
    smoother.setSampleRate(44100.0);
    smoother.setTimeConstant(0.005);  // 5 ms

    const float target = 1.0f;
    const int   tau5   = static_cast<int>(0.005 * 44100.0 * 5);  // 5 × tau in samples

    float out = 0.0f;
    for (int i = 0; i < tau5; ++i) out = smoother.process(target);

    // After 5 time constants the output must be within 1% of target
    EXPECT_NEAR(out, target, 0.01f);
}

// When sample rate doubles, the smoother must re-derive the coefficient so that
// the same wall-clock time constant is maintained.
TEST(ParamSmootherTest, SampleRateChangePreservesTimeConstant)
{
    const double tau = 0.01;  // 10 ms

    // Run at 44.1 kHz
    ParamSmoother s44;
    s44.setSampleRate(44100.0);
    s44.setTimeConstant(tau);

    int steps44 = static_cast<int>(tau * 44100.0);
    float out44 = 0.0f;
    for (int i = 0; i < steps44; ++i) out44 = s44.process(1.0f);

    // Run at 96 kHz
    ParamSmoother s96;
    s96.setSampleRate(96000.0);
    s96.setTimeConstant(tau);

    int steps96 = static_cast<int>(tau * 96000.0);
    float out96 = 0.0f;
    for (int i = 0; i < steps96; ++i) out96 = s96.process(1.0f);

    // Both should be at approximately the same fraction (~63%) after 1 tau
    EXPECT_NEAR(out44, out96, 0.02f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Math utilities
// ═══════════════════════════════════════════════════════════════════════════════

TEST(MathTest, MidiToFrequencyA4)
{
    EXPECT_NEAR(Math::midiNoteToFrequency(69), 440.0, 1e-6);
}

TEST(MathTest, MidiToFrequencyA5)
{
    EXPECT_NEAR(Math::midiNoteToFrequency(81), 880.0, 1e-6);
}

TEST(MathTest, MidiToFrequencyC4)
{
    // C4 = MIDI 60, expected ~261.626 Hz
    EXPECT_NEAR(Math::midiNoteToFrequency(60), 261.626, 0.001);
}

TEST(MathTest, TanhApproxSymmetry)
{
    // tanh is odd: tanh(-x) == -tanh(x)
    for (float x : {0.5f, 1.0f, 2.0f, 3.5f}) {
        EXPECT_NEAR(Math::tanhApprox(-x), -Math::tanhApprox(x), 1e-5f);
    }
}

TEST(MathTest, TanhApproxAccuracy)
{
    // Padé [3/3] accuracy: < 0.5% for |x| ≤ 0.5, < 2.5% for |x| ≤ 3.
    // Tolerance is intentionally relaxed – audio saturation does not require
    // exact polynomial matching.
    for (float x = -3.0f; x <= 3.0f; x += 0.25f) {
        float ref    = std::tanh(x);
        float approx = Math::tanhApprox(x);
        EXPECT_NEAR(approx, ref, 0.025f)
            << "tanhApprox(" << x << ") error too large";
    }
}

TEST(MathTest, ClampLow)
{
    EXPECT_FLOAT_EQ(Math::clamp(-5.0f, -1.0f, 1.0f), -1.0f);
}

TEST(MathTest, ClampHigh)
{
    EXPECT_FLOAT_EQ(Math::clamp(5.0f, -1.0f, 1.0f), 1.0f);
}

TEST(MathTest, ClampMid)
{
    EXPECT_FLOAT_EQ(Math::clamp(0.5f, -1.0f, 1.0f), 0.5f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// NoiseGenerator
// ═══════════════════════════════════════════════════════════════════════════════

TEST(NoiseTest, WhiteNoiseRange)
{
    NoiseGenerator ng;
    ng.seed(0xDEADBEEFu);

    for (int i = 0; i < 10000; ++i) {
        float v = ng.whiteNoise();
        EXPECT_GE(v, -1.0f);
        EXPECT_LE(v,  1.0f);
    }
}

TEST(NoiseTest, WhiteNoiseDeterministic)
{
    NoiseGenerator a, b;
    a.seed(42u);
    b.seed(42u);

    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(a.whiteNoise(), b.whiteNoise());
    }
}
