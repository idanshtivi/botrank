#include <gtest/gtest.h>
#include "../Include/Mixer.h"
#include <algorithm>
#include <cmath>
#include <utility>

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

TEST(MixerTest, MainDriveHasClearCharacterRange)
{
    auto render = [](double drive) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceEnabled(MixerSource::Osc2, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);
        mixer.setSourceLevel(MixerSource::Osc2, 0.85);

        double diffFromClean = 0.0;
        double peak = 0.0;
        for (int i = 0; i < 512; ++i) {
            const double phase = static_cast<double>(i) / 512.0;
            const double saw = 2.0 * phase - 1.0;
            const double square = phase < 0.5 ? 1.0 : -1.0;
            const double clean = 0.45 * (saw + 0.85 * square);
            const double out = mixer.processSample(saw, square, 0.0, 0.0, 0.0);
            diffFromClean += std::abs(out - clean);
            peak = std::max(peak, std::abs(out));
        }

        return std::pair<double, double>(diffFromClean / 512.0, peak);
    };

    const auto mid = render(1.5);
    const auto high = render(3.0);

    EXPECT_GT(mid.first, 0.045);
    EXPECT_GT(high.first, mid.first * 1.15);
    EXPECT_LE(high.second, 1.0);
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
