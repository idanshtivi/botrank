#include <gtest/gtest.h>
#include "../Include/OutputStage.h"
#include <cmath>

using namespace SynthCore;

TEST(OutputStageTest, MasterVolumeControlsSignal)
{
    OutputStage output;
    output.setSampleRate(48000.0);
    output.setMasterVolume(0.0);
    EXPECT_NEAR(output.processSample(0.5), 0.0, 1.0e-6);

    output.reset();
    output.setMasterVolume(1.0);
    EXPECT_GT(output.processSample(0.5), 0.1);

    output.reset();
    output.setMasterVolume(2.0);
    EXPECT_LE(output.processSample(2.0), 1.0);
}

TEST(OutputStageTest, DcBlockerReducesConstantDc)
{
    OutputStage output;
    output.setSampleRate(48000.0);
    output.setMasterVolume(1.0);
    double y = 0.0;
    for (int i = 0; i < 48000; ++i) y = output.processSample(0.5);
    EXPECT_LT(std::abs(y), 0.05);
}

TEST(OutputStageTest, DriveBoundsAndRemainsFinite)
{
    OutputStage output;
    output.setDrive(100.0);
    for (int i = 0; i < 1000; ++i) {
        const double y = output.processSample(100.0);
        EXPECT_TRUE(std::isfinite(y));
        EXPECT_LE(y, 1.0);
        EXPECT_GE(y, -1.0);
    }
}

TEST(OutputStageTest, ResetClearsDcState)
{
    OutputStage output;
    output.setMasterVolume(1.0);
    for (int i = 0; i < 1000; ++i) output.processSample(0.5);
    output.reset();
    const double y = output.processSample(0.5);
    EXPECT_GT(y, 0.1);
}

TEST(OutputStageTest, ReferenceToneIsIsolated)
{
    OutputStage output;
    output.setSampleRate(48000.0);
    output.setMasterVolume(1.0);
    output.setReferenceToneEnabled(false);
    output.reset();
    const double dry = output.processSample(0.0);

    output.setReferenceToneEnabled(true);
    output.reset();
    double peak = 0.0;
    for (int i = 0; i < 1000; ++i) {
        peak = std::max(peak, std::abs(output.processSample(0.0)));
    }

    EXPECT_NEAR(dry, 0.0, 1.0e-6);
    EXPECT_GT(peak, 0.05);
}
