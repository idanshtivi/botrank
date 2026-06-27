#include <gtest/gtest.h>
#include "../Include/LadderFilter.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace SynthCore;

static constexpr double kPi = 3.14159265358979323846;

static double renderSineRms(LadderFilter& filter, double freq, double sampleRate, int samples,
                            double env = 0.0, double midiNote = 60.0)
{
    double sum = 0.0;
    for (int i = 0; i < samples; ++i) {
        const double input = std::sin(2.0 * kPi * freq * static_cast<double>(i) / sampleRate);
        const double out = filter.processSample(input, env, midiNote);
        sum += out * out;
    }
    return std::sqrt(sum / static_cast<double>(samples));
}

TEST(LadderFilterTest, PassesLowMoreThanHigh)
{
    LadderFilter filter;
    filter.setSampleRate(48000.0);
    filter.setCutoffHz(1000.0);
    filter.setResonance(0.0);
    const double low = renderSineRms(filter, 100.0, 48000.0, 48000);
    filter.reset();
    const double high = renderSineRms(filter, 8000.0, 48000.0, 48000);
    EXPECT_GT(low, high * 5.0);
}

TEST(LadderFilterTest, CutoffChangesHighFrequencyOutput)
{
    LadderFilter lowCutoff;
    lowCutoff.setSampleRate(48000.0);
    lowCutoff.setCutoffHz(300.0);
    const double low = renderSineRms(lowCutoff, 2000.0, 48000.0, 48000);

    LadderFilter highCutoff;
    highCutoff.setSampleRate(48000.0);
    highCutoff.setCutoffHz(8000.0);
    const double high = renderSineRms(highCutoff, 2000.0, 48000.0, 48000);
    EXPECT_GT(high, low * 3.0);
}

TEST(LadderFilterTest, ResonanceDriveAndExtremeParamsStayBounded)
{
    LadderFilter filter;
    filter.setSampleRate(48000.0);
    filter.setCutoffHz(-100.0);
    filter.setResonance(100.0);
    filter.setContourAmount(100.0);
    filter.setKeyboardTrackingAmount(100.0);
    filter.setDrive(100.0);

    double peak = 0.0;
    for (int i = 0; i < 48000; ++i) {
        const double y = filter.processSample(i == 0 ? 1.0 : 0.0, 1.0, 127.0);
        EXPECT_TRUE(std::isfinite(y));
        peak = std::max(peak, std::abs(y));
    }
    EXPECT_LE(peak, 1.0);
}

TEST(LadderFilterTest, ResonanceChangesResponse)
{
    LadderFilter clean;
    clean.setSampleRate(48000.0);
    clean.setCutoffHz(1000.0);
    clean.setResonance(0.0);
    const double a = renderSineRms(clean, 900.0, 48000.0, 48000);

    LadderFilter resonant;
    resonant.setSampleRate(48000.0);
    resonant.setCutoffHz(1000.0);
    resonant.setResonance(0.8);
    const double b = renderSineRms(resonant, 900.0, 48000.0, 48000);
    EXPECT_GT(std::abs(a - b), 0.01);
}

TEST(LadderFilterTest, ContourAndKeyboardTrackingOpenCutoff)
{
    LadderFilter contour;
    contour.setSampleRate(48000.0);
    contour.setCutoffHz(300.0);
    contour.setContourAmount(1.0);
    const double closed = renderSineRms(contour, 2000.0, 48000.0, 48000, 0.0, 60.0);
    contour.reset();
    const double opened = renderSineRms(contour, 2000.0, 48000.0, 48000, 1.0, 60.0);
    EXPECT_GT(opened, closed * 2.0);

    LadderFilter tracking;
    tracking.setSampleRate(48000.0);
    tracking.setCutoffHz(500.0);
    tracking.setKeyboardTrackingAmount(1.0);
    const double lowNote = renderSineRms(tracking, 1500.0, 48000.0, 48000, 0.0, 36.0);
    tracking.reset();
    const double highNote = renderSineRms(tracking, 1500.0, 48000.0, 48000, 0.0, 84.0);
    EXPECT_GT(highNote, lowNote * 1.5);
}

TEST(LadderFilterTest, ResetIsDeterministicAndSilenceStable)
{
    LadderFilter filter;
    filter.setSampleRate(48000.0);
    filter.setCutoffHz(1000.0);
    std::vector<double> a(512);
    std::vector<double> b(512);
    filter.reset();
    for (size_t i = 0; i < a.size(); ++i) a[i] = filter.processSample(i == 0 ? 1.0 : 0.0, 0.0, 60.0);
    filter.reset();
    for (size_t i = 0; i < b.size(); ++i) b[i] = filter.processSample(i == 0 ? 1.0 : 0.0, 0.0, 60.0);
    EXPECT_EQ(a, b);

    for (int i = 0; i < 48000; ++i) {
        const double y = filter.processSample(0.0, 0.0, 60.0);
        EXPECT_TRUE(std::isfinite(y));
        EXPECT_LE(std::abs(y), 1.0);
    }
}
