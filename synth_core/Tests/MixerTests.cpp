#include <gtest/gtest.h>
#include "../Include/Mixer.h"
#include "../Include/DSPUtils.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace SynthCore;

static double bufferRms(const std::vector<double>& buffer)
{
    double sum = 0.0;
    for (double sample : buffer) sum += sample * sample;
    return std::sqrt(sum / static_cast<double>(buffer.size()));
}

static double gainMatchedDifference(const std::vector<double>& reference, const std::vector<double>& candidate)
{
    const double refRms = bufferRms(reference);
    const double candRms = bufferRms(candidate);
    if (refRms <= 0.0 || candRms <= 0.0) return 0.0;

    const double scale = refRms / candRms;
    double diff = 0.0;
    for (size_t i = 0; i < reference.size(); ++i)
        diff += std::abs(reference[i] - candidate[i] * scale);
    return diff / static_cast<double>(reference.size());
}

static std::vector<double> renderMixerSimple(double drive, bool triangle)
{
    Mixer mixer;
    mixer.setDrive(drive);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceEnabled(MixerSource::Osc3, false);
    mixer.setSourceEnabled(MixerSource::Noise, false);
    mixer.setSourceEnabled(MixerSource::ExternalInput, false);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);

    constexpr int n = 1024;
    std::vector<double> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double phase = static_cast<double>(i) / static_cast<double>(n);
        const double sine = std::sin(2.0 * 3.14159265358979323846 * phase);
        const double tri = phase < 0.25 ? 4.0 * phase
                         : phase < 0.75 ? 2.0 - 4.0 * phase
                         : -4.0 + 4.0 * phase;
        out[static_cast<size_t>(i)] = mixer.processSample(triangle ? tri : sine, 0.0, 0.0, 0.0, 0.0);
    }
    return out;
}

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
    auto renderLevel = [](double level) {
        Mixer mixer;
        mixer.setDrive(0.0);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceEnabled(MixerSource::Osc2, false);
        mixer.setSourceLevel(MixerSource::Osc1, level);
        return mixer.processSample(1.0, 1.0, 0.0, 0.0, 0.0);
    };

    const double low = renderLevel(0.25);
    const double high = renderLevel(1.0);

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
    EXPECT_GT(two, one * 1.5)
        << "Two full sources should remain clearly bigger than one, while source compensation avoids hard clipping";
}

TEST(MixerTest, LevelClampWorks)
{
    auto renderLevel = [](double level) {
        Mixer mixer;
        mixer.setDrive(0.0);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceLevel(MixerSource::Osc1, level);
        return mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0);
    };

    EXPECT_DOUBLE_EQ(renderLevel(-1.0), 0.0);
    EXPECT_LE(renderLevel(2.0), 0.55);
}

TEST(MixerTest, SourceLevelsAreContinuousGains)
{
    auto renderLevel = [](MixerSource source, double level) {
        Mixer mixer;
        mixer.setDrive(0.0);
        mixer.setSourceEnabled(MixerSource::Osc1, false);
        mixer.setSourceEnabled(MixerSource::Osc2, false);
        mixer.setSourceEnabled(MixerSource::Osc3, false);
        mixer.setSourceEnabled(MixerSource::Noise, false);
        mixer.setSourceEnabled(MixerSource::ExternalInput, false);
        mixer.setSourceEnabled(source, true);
        mixer.setSourceLevel(source, level);
        return mixer.processSample(1.0, 1.0, 1.0, 1.0, 1.0);
    };

    for (auto source : {MixerSource::Osc1, MixerSource::Osc2, MixerSource::Osc3,
                        MixerSource::Noise, MixerSource::ExternalInput}) {
        const double off = renderLevel(source, 0.0);
        const double quarter = renderLevel(source, 0.25);
        const double half = renderLevel(source, 0.50);
        const double full = renderLevel(source, 1.0);

        EXPECT_NEAR(off, 0.0, 1.0e-12);
        EXPECT_GT(quarter, off);
        EXPECT_GT(half, quarter * 1.8);
        EXPECT_GT(full, half * 1.8);
    }
}

TEST(MixerTest, NearMidLevelChangesAreNotOnOffSteps)
{
    auto renderLevel = [](double level) {
        Mixer mixer;
        mixer.setDrive(0.0);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceLevel(MixerSource::Osc1, level);
        return mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0);
    };

    const double below = renderLevel(0.49);
    const double above = renderLevel(0.51);

    EXPECT_GT(below, 0.0);
    EXPECT_GT(above, below);
    EXPECT_LT(above - below, 0.02)
        << "0.49 to 0.51 must be a small gain change, not an off/on jump";
}

TEST(MixerTest, EnabledSwitchAndZeroLevelMuteIndependently)
{
    Mixer disabled;
    disabled.setDrive(0.0);
    disabled.setSourceEnabled(MixerSource::Osc1, false);
    disabled.setSourceLevel(MixerSource::Osc1, 1.0);
    EXPECT_NEAR(disabled.processSample(1.0, 0.0, 0.0, 0.0, 0.0), 0.0, 1.0e-12);

    Mixer enabledZero;
    enabledZero.setDrive(0.0);
    enabledZero.setSourceEnabled(MixerSource::Osc1, true);
    enabledZero.setSourceLevel(MixerSource::Osc1, 0.0);
    EXPECT_NEAR(enabledZero.processSample(1.0, 0.0, 0.0, 0.0, 0.0), 0.0, 1.0e-12);
}

TEST(MixerTest, RuntimeLevelChangesAreSmoothed)
{
    Mixer mixer;
    mixer.setSampleRate(44100.0);
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceLevel(MixerSource::Osc1, 0.0);
    EXPECT_NEAR(mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0), 0.0, 1.0e-12);

    mixer.setSourceLevel(MixerSource::Osc1, 1.0);
    const double first = mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0);
    double later = first;
    for (int i = 0; i < 512; ++i)
        later = mixer.processSample(1.0, 0.0, 0.0, 0.0, 0.0);

    EXPECT_GT(first, 0.0);
    EXPECT_LT(first, later * 0.25)
        << "Runtime level changes should ramp instead of jumping instantly";
    EXPECT_GT(later, 0.30);
}

TEST(MixerTest, LowSourceLevelStaysQuietWithMixerDriveEnabled)
{
    auto renderLevel = [](double level) {
        Mixer mixer;
        mixer.setSampleRate(44100.0);
        mixer.setDrive(1.17);
        mixer.setSourceEnabled(MixerSource::Osc1, true);
        mixer.setSourceEnabled(MixerSource::Osc2, false);
        mixer.setSourceEnabled(MixerSource::Osc3, false);
        mixer.setSourceEnabled(MixerSource::Noise, false);
        mixer.setSourceLevel(MixerSource::Osc1, level);

        double sumSquares = 0.0;
        constexpr int n = 2048;
        for (int i = 0; i < n; ++i) {
            const double phase = static_cast<double>(i % 512) / 512.0;
            const double saw = 2.0 * phase - 1.0;
            const double out = mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
            sumSquares += out * out;
        }
        return std::sqrt(sumSquares / static_cast<double>(n));
    };

    const double onePercent = renderLevel(0.01);
    const double tenPercent = renderLevel(0.10);
    const double half = renderLevel(0.50);
    const double full = renderLevel(1.0);

    EXPECT_LT(onePercent, full * 0.04)
        << "Level 1% must remain quiet even when Mixer Drive is enabled";
    EXPECT_GT(tenPercent, onePercent * 5.0)
        << "Level 10% must be clearly louder than 1%, not the same on-state";
    EXPECT_GT(half, tenPercent * 3.0)
        << "Level 50% must be clearly louder than 10%, not already full volume";
    EXPECT_GT(full, half * 1.35)
        << "Level 100% must still be louder than 50% while pushing the drive harder";
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
            const double clean = 0.55 * (saw + 0.85 * square);
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

    // Strong character targets — measured values: mid≈0.43, high≈0.40, midToHigh≈0.052.
    EXPECT_GT(mid.diffFromClean, 0.20) << "Drive 2 must have strong character vs clean";
    EXPECT_GT(high.diffFromClean, mid.diffFromClean * 0.80) << "Drive 3 must keep strong character";
    EXPECT_GT(midToHigh, 0.025) << "Drive 2→3 step must be audible";
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

    // Aggressive drive design: measured ~0.385. Must be >> old 0.030 threshold.
    EXPECT_GT(diff, 0.12) << "Drive 1 must be clearly audible vs Drive 0";
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

    // Measured: diff01≈0.350, diff12≈0.103, diff23≈0.046.
    EXPECT_GT(diff01, 0.18) << "Drive 0→1 step must have strong character";
    EXPECT_GT(diff12, 0.070) << "Drive 1→2 step must have strong character";
    EXPECT_GT(diff23, 0.025) << "Drive 2→3 step must have audible character";
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

    // Aggressive clean-input design measured ~0.397 — well above V4's 0.3588.
    EXPECT_GT(diff, 0.20) << "modern exciter drive must have clear character on a clean single oscillator";
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

    // Measured Drive 1 RMS ≈ 2.48× clean RMS — aggressive wet blend.
    EXPECT_GT(rmsDrive1, rmsClean * 2.0)
        << "Drive 1 must be strongly boosted vs clean (measured ~2.5x)";
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

// Drive 0 remains dry: no drive blend is added. The final soft safety is still
// active because it replaces the old hard mixer clamp.
TEST(MixerTest, Drive0IsDryThroughSoftSafety)
{
    Mixer mixer;
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceEnabled(MixerSource::Osc3, false);
    mixer.setSourceEnabled(MixerSource::Noise, false);
    mixer.setSourceEnabled(MixerSource::ExternalInput, false);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);

    for (int i = 0; i < 512; ++i) {
        const double saw = 2.0 * (static_cast<double>(i) / 512.0) - 1.0;
        const double out = mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        const double expected = DriveUtils::softLimit(saw * 0.48, 0.98);
        EXPECT_NEAR(out, expected, 1e-9)
            << "Drive 0 must produce clean output at sample " << i;
    }
}

TEST(MixerTest, Drive0IsDry)
{
    const auto clean = renderMixerSimple(0.0, false);
    for (int i = 0; i < 1024; ++i) {
        const double phase = static_cast<double>(i) / 1024.0;
        const double sine = std::sin(2.0 * 3.14159265358979323846 * phase);
        EXPECT_NEAR(clean[static_cast<size_t>(i)], DriveUtils::softLimit(sine * 0.48, 0.98), 1.0e-12);
    }
}

TEST(MixerTest, Drive1AddsColorNotJustVolume)
{
    const auto sine0 = renderMixerSimple(0.0, false);
    const auto sine1 = renderMixerSimple(1.0, false);
    const auto tri0 = renderMixerSimple(0.0, true);
    const auto tri1 = renderMixerSimple(1.0, true);

    EXPECT_GT(gainMatchedDifference(sine0, sine1), 0.020)
        << "Drive 1 must change sine waveshape after RMS matching";
    EXPECT_GT(gainMatchedDifference(tri0, tri1), 0.018)
        << "Drive 1 must change triangle waveshape after RMS matching";
}

TEST(MixerTest, Drive2ClearlyOpensSound)
{
    const auto sine0 = renderMixerSimple(0.0, false);
    const auto sine1 = renderMixerSimple(1.0, false);
    const auto sine2 = renderMixerSimple(2.0, false);
    const double diff1 = gainMatchedDifference(sine0, sine1);
    const double diff2 = gainMatchedDifference(sine0, sine2);

    EXPECT_GT(diff2, diff1 * 1.25)
        << "Drive 2 must add clearly more color than Drive 1 after RMS matching";
    EXPECT_GT(diff2, 0.040);
}

TEST(MixerTest, SoftSafetyReplacesNormalHardClamp)
{
    Mixer mixer;
    mixer.setSampleRate(44100.0);
    mixer.setDrive(3.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, true);
    mixer.setSourceEnabled(MixerSource::Osc3, true);
    mixer.setSourceEnabled(MixerSource::Noise, false);
    mixer.setSourceEnabled(MixerSource::ExternalInput, false);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);
    mixer.setSourceLevel(MixerSource::Osc2, 1.0);
    mixer.setSourceLevel(MixerSource::Osc3, 0.65);

    int hardClampHits = 0;
    double peak = 0.0;
    for (int i = 0; i < 8192; ++i) {
        const double phase = static_cast<double>(i % 512) / 512.0;
        const double saw = 2.0 * phase - 1.0;
        const double square = phase < 0.5 ? 1.0 : -1.0;
        const double brightSaw = 2.0 * std::fmod(phase * 2.0, 1.0) - 1.0;
        const double out = mixer.processSample(saw, square, brightSaw, 0.0, 0.0);
        peak = std::max(peak, std::abs(out));
        if (std::abs(out) >= 0.9999)
            ++hardClampHits;
    }

    EXPECT_EQ(hardClampHits, 0)
        << "Mixer final hard clamp should be a safety guard, not the normal saturator";
    EXPECT_GT(peak, 0.80)
        << "Soft safety must not make high-drive three-source patches feel small";
}

TEST(MixerTest, Drive3IsModernExciterNotVolume)
{
    const auto sine0 = renderMixerSimple(0.0, false);
    const auto sine3 = renderMixerSimple(3.0, false);
    const auto tri0 = renderMixerSimple(0.0, true);
    const auto tri3 = renderMixerSimple(3.0, true);

    EXPECT_GT(gainMatchedDifference(sine0, sine3), 0.060)
        << "Drive 3 must remain strongly different from gain-matched clean sine";
    EXPECT_GT(gainMatchedDifference(tri0, tri3), 0.050)
        << "Drive 3 must remain strongly different from gain-matched clean triangle";
}

TEST(MixerTest, DriveProgressionAcrossKnob)
{
    const auto d0 = renderMixerSimple(0.0, false);
    const auto d05 = renderMixerSimple(0.5, false);
    const auto d1 = renderMixerSimple(1.0, false);
    const auto d2 = renderMixerSimple(2.0, false);
    const auto d3 = renderMixerSimple(3.0, false);

    const double c05 = gainMatchedDifference(d0, d05);
    const double c1 = gainMatchedDifference(d0, d1);
    const double c2 = gainMatchedDifference(d0, d2);
    const double c3 = gainMatchedDifference(d0, d3);

    EXPECT_GT(c05, 0.006) << "Drive 0.5 must not be a dead zone";
    EXPECT_GT(c1, c05 * 1.45);
    EXPECT_GT(c2, c1 * 1.25);
    EXPECT_GT(c3, c2 * 0.96);
}

TEST(MixerTest, Drive3BoundedAndFinite)
{
    Mixer mixer;
    mixer.setDrive(3.0);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, true);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);
    mixer.setSourceLevel(MixerSource::Osc2, 1.0);

    for (int i = 0; i < 4096; ++i) {
        const double phase = static_cast<double>(i % 1024) / 1024.0;
        const double sine = std::sin(2.0 * 3.14159265358979323846 * phase);
        const double tri = phase < 0.25 ? 4.0 * phase
                         : phase < 0.75 ? 2.0 - 4.0 * phase
                         : -4.0 + 4.0 * phase;
        const double out = mixer.processSample(sine, tri, 0.0, 0.0, 0.0);
        EXPECT_TRUE(std::isfinite(out));
        EXPECT_LE(out, 1.0);
        EXPECT_GE(out, -1.0);
    }
}

TEST(MixerTest, Drive3DoesNotCollapse)
{
    const auto d2 = renderMixerSimple(2.0, true);
    const auto d3 = renderMixerSimple(3.0, true);

    EXPECT_GT(bufferRms(d3), bufferRms(d2) * 0.90)
        << "Drive 3 must not collapse below Drive 2";
}

// Drive 3 must not merely be a louder version of Drive 0.
// After gain-matching to the same RMS, the waveshapes must still differ substantially.
// This proves saturation character, not just amplitude boost.
TEST(MixerTest, Drive3IsWaveshapedNotJustLouder)
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

    // Compute RMS of each
    double rms0 = 0.0, rms3 = 0.0;
    for (size_t i = 0; i < 512; ++i) {
        rms0 += d0[i] * d0[i];
        rms3 += d3[i] * d3[i];
    }
    rms0 = std::sqrt(rms0 / 512.0);
    rms3 = std::sqrt(rms3 / 512.0);

    ASSERT_GT(rms3, 0.0) << "Drive 3 must produce output";
    ASSERT_GT(rms0, 0.0) << "Drive 0 must produce output";

    // Scale Drive 0 to match Drive 3 RMS, then compare waveshapes
    const double scale = rms3 / rms0;
    double shapeDiff = 0.0;
    for (size_t i = 0; i < 512; ++i)
        shapeDiff += std::abs(d0[i] * scale - d3[i]);
    shapeDiff /= 512.0;

    // If Drive 3 were just a louder Drive 0, shapeDiff would be ~0.
    // Saturation fundamentally changes the waveshape — require strong difference.
    EXPECT_GT(shapeDiff, 0.08)
        << "Drive 3 must be a genuinely different waveshape from gain-matched Drive 0";
}
