#include <gtest/gtest.h>
#include "../Include/Vca.h"
#include <cmath>

using namespace SynthCore;

TEST(VCATest, EnvelopeControlsGain)
{
    Vca vca;
    vca.setMasterVolume(1.0);
    EXPECT_DOUBLE_EQ(vca.processSample(0.8, 0.0), 0.0);
    EXPECT_NEAR(vca.processSample(0.8, 1.0), 0.8, 0.001);
    EXPECT_NEAR(vca.processSample(0.8, 0.5), 0.4, 0.001);
}

TEST(VCATest, MasterVolumeAndEnvelopeClamp)
{
    Vca vca;
    vca.setMasterVolume(0.25);
    EXPECT_NEAR(vca.processSample(1.0, 1.0), 0.25, 0.001);
    vca.setMasterVolume(2.0);
    EXPECT_NEAR(vca.processSample(1.0, 2.0), 1.0, 0.001);
    vca.setMasterVolume(-1.0);
    EXPECT_DOUBLE_EQ(vca.processSample(1.0, 1.0), 0.0);
}

TEST(VCATest, DriveBoundsHighInput)
{
    Vca vca;
    vca.setMasterVolume(1.0);
    vca.setDrive(100.0);
    const double out = vca.processSample(100.0, 1.0);
    EXPECT_TRUE(std::isfinite(out));
    EXPECT_LE(out, 1.0);
    EXPECT_GE(out, -1.0);
}

TEST(VCATest, ResetIsSafeAndLongRunFinite)
{
    Vca vca;
    vca.setSampleRate(48000.0);
    vca.reset();
    for (int i = 0; i < 48000; ++i) {
        EXPECT_TRUE(std::isfinite(vca.processSample(0.25, 0.5)));
    }
}
