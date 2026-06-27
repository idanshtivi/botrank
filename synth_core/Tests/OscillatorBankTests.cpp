#include <gtest/gtest.h>
#include "../Include/OscillatorBank.h"
#include <cmath>
#include <vector>

using namespace SynthCore;

static double measureHz(const std::vector<float>& buf, double sampleRate)
{
    double first = -1.0;
    double last = -1.0;
    int count = 0;
    for (int i = 1; i < static_cast<int>(buf.size()); ++i) {
        if (buf[static_cast<size_t>(i - 1)] < 0.0f && buf[static_cast<size_t>(i)] >= 0.0f) {
            const double a = -buf[static_cast<size_t>(i - 1)];
            const double b = static_cast<double>(buf[static_cast<size_t>(i)]) - buf[static_cast<size_t>(i - 1)];
            const double pos = static_cast<double>(i - 1) + (b != 0.0 ? a / b : 0.0);
            if (first < 0.0) first = pos;
            last = pos;
            ++count;
        }
    }
    if (count < 2) return 0.0;
    return static_cast<double>(count - 1) / ((last - first) / sampleRate);
}

TEST(OscillatorBankTest, AllThreeOscillatorsProduceOutput)
{
    OscillatorBank bank;
    bank.setSampleRate(44100.0);
    bank.setBaseMidiNote(57.0);
    bank.reset();

    float peak1 = 0.0f;
    float peak2 = 0.0f;
    float peak3 = 0.0f;
    for (int i = 0; i < 2048; ++i) {
        const auto out = bank.process();
        peak1 = std::max(peak1, std::abs(out.osc1));
        peak2 = std::max(peak2, std::abs(out.osc2));
        peak3 = std::max(peak3, std::abs(out.osc3));
    }

    EXPECT_GT(peak1, 0.1f);
    EXPECT_GT(peak2, 0.1f);
    EXPECT_GT(peak3, 0.1f);
}

TEST(OscillatorBankTest, OscillatorDetuneChangesFrequency)
{
    constexpr double sr = 44100.0;
    OscillatorBank bank;
    bank.setSampleRate(sr);
    bank.setBaseMidiNote(57.0);
    bank.setOscillatorDetuneSemitones(2, 7.0);
    bank.setOscillatorDetuneSemitones(3, -7.0);
    bank.reset();

    std::vector<float> osc1(static_cast<size_t>(sr));
    std::vector<float> osc2(static_cast<size_t>(sr));
    std::vector<float> osc3(static_cast<size_t>(sr));
    for (size_t i = 0; i < osc1.size(); ++i) {
        const auto out = bank.process();
        osc1[i] = out.osc1;
        osc2[i] = out.osc2;
        osc3[i] = out.osc3;
    }

    const double f1 = measureHz(osc1, sr);
    EXPECT_GT(measureHz(osc2, sr), f1 * 1.45);
    EXPECT_LT(measureHz(osc3, sr), f1 * 0.75);
}

TEST(OscillatorBankTest, Oscillator3KeyboardTrackingSwitch)
{
    constexpr double sr = 44100.0;
    OscillatorBank bank;
    bank.setSampleRate(sr);
    bank.setBaseMidiNote(45.0);
    bank.setOscillator3KeyboardTrackingEnabled(false);
    bank.setBaseMidiNote(69.0);
    bank.reset();

    std::vector<float> off(static_cast<size_t>(sr));
    for (auto& sample : off) sample = bank.process().osc3;
    const double trackingOffHz = measureHz(off, sr);

    bank.setOscillator3KeyboardTrackingEnabled(true);
    bank.reset();

    std::vector<float> on(static_cast<size_t>(sr));
    for (auto& sample : on) sample = bank.process().osc3;
    const double trackingOnHz = measureHz(on, sr);

    EXPECT_LT(trackingOffHz, trackingOnHz * 0.55);
    EXPECT_NEAR(trackingOnHz, 440.0, 1.0);
}

TEST(OscillatorBankTest, ResetIsDeterministic)
{
    OscillatorBank bank;
    bank.setSampleRate(44100.0);
    bank.setBaseMidiNote(60.0);

    std::vector<float> a(512);
    std::vector<float> b(512);
    bank.reset();
    for (auto& sample : a) sample = bank.process().osc1;
    bank.reset();
    for (auto& sample : b) sample = bank.process().osc1;

    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_FLOAT_EQ(a[i], b[i]) << "sample " << i;
    }
}

TEST(OscillatorBankTest, OutputRemainsFinite)
{
    OscillatorBank bank;
    bank.setSampleRate(48000.0);
    bank.setBaseMidiNote(108.0);
    bank.setPitchBendSemitones(12.0);

    for (int i = 0; i < 48000; ++i) {
        const auto out = bank.process();
        EXPECT_TRUE(std::isfinite(out.osc1));
        EXPECT_TRUE(std::isfinite(out.osc2));
        EXPECT_TRUE(std::isfinite(out.osc3));
    }
}
