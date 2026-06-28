#include <gtest/gtest.h>
#include "../Include/Mixer.h"
#include <algorithm>
#include <array>
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

TEST(MixerTest, MainDriveHasClearCharacterRange)
{
    auto render = [](double drive) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceEnabled(MixerSource::Osc2, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);
        mixer.setSourceLevel(MixerSource::Osc2, 0.85);

        std::array<double, 512> output {};
        double diffFromClean = 0.0;
        double peak = 0.0;
        double rms = 0.0;
        for (int i = 0; i < 512; ++i) {
            const double phase = static_cast<double>(i) / 512.0;
            const double saw = 2.0 * phase - 1.0;
            const double square = phase < 0.5 ? 1.0 : -1.0;
            const double clean = 0.45 * (saw + 0.85 * square);
            const double out = mixer.processSample(saw, square, 0.0, 0.0, 0.0);
            output[static_cast<size_t>(i)] = out;
            diffFromClean += std::abs(out - clean);
            peak = std::max(peak, std::abs(out));
            rms += out * out;
        }

        struct Result {
            std::array<double, 512> output;
            double diffFromClean;
            double rms;
            double peak;
        };

        return Result {
            output,
            diffFromClean / 512.0,
            std::sqrt(rms / 512.0),
            peak
        };
    };

    const auto clean = render(0.0);
    const auto mid = render(2.0);
    const auto high = render(3.0);

    double midToHigh = 0.0;
    for (int i = 0; i < 512; ++i)
        midToHigh += std::abs(high.output[static_cast<size_t>(i)] - mid.output[static_cast<size_t>(i)]);
    midToHigh /= 512.0;

    // V4.1: stronger character targets — main drive must feel satisfying on its own.
    EXPECT_GT(mid.diffFromClean, 0.14);
    EXPECT_GT(high.diffFromClean, mid.diffFromClean * 0.90);
    EXPECT_GT(midToHigh, 0.050);
    // Drive 3 RMS must not collapse below 92% of Drive 2 RMS.
    EXPECT_GT(high.rms, mid.rms * 0.92);
    EXPECT_LE(clean.peak, 1.0);
    EXPECT_LE(high.peak, 1.0);
}

// Single-osc (saw only) Drive 3 must not produce lower RMS than Drive 2.
// This mirrors the real single-oscillator open-filter patch that revealed the collapse.
TEST(MixerTest, MainDriveDrive3NoCollapseSingleOsc)
{
    auto renderSaw = [](double drive) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);

        double rms = 0.0;
        for (int i = 0; i < 512; ++i) {
            const double saw = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
            const double out = mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
            rms += out * out;
        }
        return std::sqrt(rms / 512.0);
    };

    const double rms2 = renderSaw(2.0);
    const double rms3 = renderSaw(3.0);

    // Drive 3 must not be more than 8% quieter than Drive 2.
    EXPECT_GT(rms3, rms2 * 0.92) << "Drive 3 collapsed below Drive 2 on single saw oscillator";
    // Drive 3 must be audibly driven (not silent or near-clean).
    EXPECT_GT(rms3, 0.20) << "Drive 3 single osc output too low";
}

// Drive 1 must already be audibly different from Drive 0 — the knob cannot feel dead
// until the middle of the range.
TEST(MixerTest, MainDriveDrive1AudibleVsDrive0)
{
    auto renderSaw = [](double drive) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);

        std::vector<double> output(512);
        for (int i = 0; i < 512; ++i) {
            const double saw = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
            output[static_cast<size_t>(i)] = mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        }
        return output;
    };

    const auto d0 = renderSaw(0.0);
    const auto d1 = renderSaw(1.0);

    double diff = 0.0;
    for (size_t i = 0; i < 512; ++i)
        diff += std::abs(d1[i] - d0[i]);
    diff /= 512.0;

    // V4.1 target: Drive 1 must be clearly audible (was 0.025 in V4, expecting more in V4.1).
    EXPECT_GT(diff, 0.030) << "Drive 1 must be clearly audible vs Drive 0";
}

// Each drive step must contribute meaningful character — no dead zone in the knob.
TEST(MixerTest, MainDriveStepDifferencesProgressive)
{
    auto renderSaw = [](double drive) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceEnabled(MixerSource::Osc2, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);
        mixer.setSourceLevel(MixerSource::Osc2, 0.70);

        std::vector<double> output(512);
        for (int i = 0; i < 512; ++i) {
            const double saw    = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
            const double square = (i < 256) ? 1.0 : -1.0;
            output[static_cast<size_t>(i)] = mixer.processSample(saw, square, 0.0, 0.0, 0.0);
        }
        return output;
    };

    const auto d0 = renderSaw(0.0);
    const auto d1 = renderSaw(1.0);
    const auto d2 = renderSaw(2.0);
    const auto d3 = renderSaw(3.0);

    auto avgDiff = [](const std::vector<double>& a, const std::vector<double>& b) {
        double s = 0.0;
        for (size_t i = 0; i < a.size(); ++i) s += std::abs(a[i] - b[i]);
        return s / static_cast<double>(a.size());
    };

    const double diff01 = avgDiff(d0, d1);
    const double diff12 = avgDiff(d1, d2);
    const double diff23 = avgDiff(d2, d3);

    // V4.1 target: stronger step differences (V4 was 0.025/0.035/0.020).
    EXPECT_GT(diff01, 0.030) << "Drive 0→1 step must have audible character";
    EXPECT_GT(diff12, 0.040) << "Drive 1→2 step must have audible character";
    EXPECT_GT(diff23, 0.022) << "Drive 2→3 step must have audible character";
}

// The x² asymmetry term introduces slight DC offset.  Verify it stays controlled at
// the Mixer level (the OutputStage DC blocker removes residual DC before final output).
TEST(MixerTest, MainDriveDCOffsetControlled)
{
    for (double drive : {1.0, 2.0, 3.0}) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);

        // Render exactly one saw cycle (512 samples = one period) so mean = 0 for clean signal.
        double dcSum = 0.0;
        for (int i = 0; i < 512; ++i) {
            const double saw = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
            dcSum += mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        }
        const double dc = dcSum / 512.0;
        EXPECT_NEAR(dc, 0.0, 0.05)
            << "Drive " << drive << " must not produce more than 5% DC at Mixer output";
    }
}

// V4.1 minimum character target: Drive 3 single-osc diff vs clean must exceed the
// V4 baseline.  Anchored at V4's measured value of 0.3588.
TEST(MixerTest, MainDriveV41HasStrongerCharacterThanV4)
{
    auto renderSaw = [](double drive) {
        Mixer mixer;
        mixer.setDrive(drive);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceLevel(MixerSource::Osc1, 1.0);

        std::vector<double> buf(512);
        for (int i = 0; i < 512; ++i) {
            const double saw = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
            buf[static_cast<size_t>(i)] = mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        }
        return buf;
    };

    const auto d0 = renderSaw(0.0);
    const auto d3 = renderSaw(3.0);

    double diff = 0.0;
    for (size_t i = 0; i < d0.size(); ++i)
        diff += std::abs(d3[i] - d0[i]);
    diff /= static_cast<double>(d0.size());

    // V4 measured 0.3588 on the same single-saw patch.  V4.1 must clearly exceed it.
    EXPECT_GT(diff, 0.36) << "V4.1 Drive 3 must have stronger character than V4's 0.3588";
}

// Verify driveMix opens quickly: at Drive 1 the blend must be meaningfully wet.
TEST(MixerTest, MainDriveMixOpensByDriveOne)
{
    // Render two cycles of a saw: one with Drive 0 (clean), one at Drive 0 but with
    // the drive knob snapped to 1 mid-render to check the dry/wet blend path is active.
    // Simpler: compare the driven/clean RMS ratio to infer blend.
    Mixer mixerClean, mixerDrive1;
    mixerClean.setDrive(0.0);
    mixerDrive1.setDrive(1.0);
    for (auto* m : {&mixerClean, &mixerDrive1}) {
        m->setSourceEnabled(MixerSource::Osc1, true);
        m->setSourceLevel(MixerSource::Osc1, 1.0);
    }

    double rmsClean = 0.0, rmsDrive1 = 0.0;
    for (int i = 0; i < 512; ++i) {
        const double saw = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
        const double c   = mixerClean.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        const double d   = mixerDrive1.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        rmsClean  += c * c;
        rmsDrive1 += d * d;
    }
    rmsClean  = std::sqrt(rmsClean  / 512.0);
    rmsDrive1 = std::sqrt(rmsDrive1 / 512.0);

    // At Drive 1 the mix must be at least 30% wet (RMS meaningfully above clean).
    EXPECT_GT(rmsDrive1, rmsClean * 1.15)
        << "Drive 1 driveMix must be open enough to produce clearly audible output boost";
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
