#include <gtest/gtest.h>
#include "../Include/Mixer.h"
#include <cmath>

using namespace SynthCore;

TEST(MixerTest, AllSourcesDisabledProducesSilence)
{
    Mixer mixer;
    mixer.setSourceEnabled(MixerSource::Osc1, false);
    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceEnabled(MixerSource::Osc3, false);
    mixer.setSourceEnabled(MixerSource::Noise, false);
    mixer.setSourceEnabled(MixerSource::ExternalInput, false);

    EXPECT_DOUBLE_EQ(mixer.processSample(1.0, 1.0, 1.0, 1.0, 1.0), 0.0);
}

TEST(MixerTest, OscillatorSourcesPassSignal)
{
    Mixer mixer;
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceEnabled(MixerSource::Osc3, false);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);
    EXPECT_GT(mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0), 0.4);

    mixer.setSourceEnabled(MixerSource::Osc1, false);
    mixer.setSourceEnabled(MixerSource::Osc2, true);
    mixer.setSourceLevel(MixerSource::Osc2, 1.0);
    EXPECT_GT(mixer.processSample(0.0, 1.0, 0.0, 0.0, 0.0), 0.4);

    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceEnabled(MixerSource::Osc3, true);
    mixer.setSourceLevel(MixerSource::Osc3, 1.0);
    EXPECT_GT(mixer.processSample(0.0, 0.0, 1.0, 0.0, 0.0), 0.4);
}

TEST(MixerTest, LevelsAndDisabledSourcesAffectOutput)
{
    Mixer mixer;
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceLevel(MixerSource::Osc1, 0.25);
    const double low = mixer.processSample(1.0, 1.0, 0.0, 0.0, 0.0);

    mixer.setSourceLevel(MixerSource::Osc1, 1.0);
    const double high = mixer.processSample(1.0, 1.0, 0.0, 0.0, 0.0);

    EXPECT_GT(high, low * 3.0);
}

TEST(MixerTest, MultipleSourcesSum)
{
    Mixer mixer;
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, true);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);
    mixer.setSourceLevel(MixerSource::Osc2, 1.0);

    const double one = mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0);
    const double two = mixer.processSample(1.0, 1.0, 0.0, 0.0, 0.0);
    EXPECT_GT(two, one * 1.8);
}

TEST(MixerTest, LevelClampWorks)
{
    Mixer mixer;
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);

    mixer.setSourceLevel(MixerSource::Osc1, -1.0);
    EXPECT_DOUBLE_EQ(mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0), 0.0);

    mixer.setSourceLevel(MixerSource::Osc1, 2.0);
    EXPECT_LE(mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0), 0.45);
}

TEST(MixerTest, DriveClampAndSaturationBoundsOutput)
{
    Mixer mixer;
    mixer.setDrive(100.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);

    const double out = mixer.processSample(100.0, 0.0, 0.0, 0.0, 0.0);
    EXPECT_TRUE(std::isfinite(out));
    EXPECT_LE(out, 1.0);
    EXPECT_GE(out, -1.0);
}

TEST(MixerTest, ResetIsSafeAndDeterministic)
{
    Mixer mixer;
    mixer.setDrive(1.6);
    const double before = mixer.processSample(0.5, 0.25, 0.0, 0.0, 0.0);
    mixer.reset();
    const double after = mixer.processSample(0.5, 0.25, 0.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(before, after);
}
