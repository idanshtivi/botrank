#include <gtest/gtest.h>
#include "../Include/Oscillator.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <limits>

using namespace SynthCore;

// ─── helpers ──────────────────────────────────────────────────────────────────

static constexpr double kTwoPi = 6.283185307179586476925;

// Compute magnitude of a single DFT bin k over N real samples.
// O(N) — used only in tests, not in the audio path.
static double dftBinMagnitude(const std::vector<float>& buf, int k)
{
    double re = 0.0, im = 0.0;
    int N = static_cast<int>(buf.size());
    for (int n = 0; n < N; ++n) {
        double angle = kTwoPi * k * n / N;
        re += buf[static_cast<size_t>(n)] * std::cos(angle);
        im -= buf[static_cast<size_t>(n)] * std::sin(angle);
    }
    return std::sqrt(re * re + im * im) / N;
}

// Return the bin index with the highest magnitude in [1, N/2).
static int dominantBin(const std::vector<float>& buf)
{
    int N = static_cast<int>(buf.size());
    double peak = 0.0;
    int    best = 1;
    for (int k = 1; k < N / 2; ++k) {
        double m = dftBinMagnitude(buf, k);
        if (m > peak) { peak = m; best = k; }
    }
    return best;
}

// Render n samples into a vector, discarding an initial warmup block.
static std::vector<float> render(Oscillator& osc, int warmup, int n)
{
    for (int i = 0; i < warmup; ++i) osc.process();
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) out[static_cast<size_t>(i)] = osc.process();
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Frequency accuracy
// ═══════════════════════════════════════════════════════════════════════════════

// The dominant DFT bin must correspond to the set frequency within ±1 bin.
TEST(OscillatorTest, FrequencyAccuracy440Hz)
{
    constexpr double fs  = 44100.0;
    constexpr double f   = 440.0;
    constexpr int    N   = 4096;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(f);

    auto buf  = render(osc, 0, N);
    int  bin  = dominantBin(buf);

    // Expected bin = round(f * N / fs)
    int  expected = static_cast<int>(std::round(f * N / fs));
    EXPECT_EQ(bin, expected)
        << "dominant bin " << bin << " != expected " << expected;
}

TEST(OscillatorTest, FrequencyAccuracy110Hz)
{
    constexpr double fs = 44100.0;
    constexpr double f  = 110.0;
    constexpr int    N  = 8192;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(f);

    auto buf     = render(osc, 0, N);
    int  bin     = dominantBin(buf);
    int  expected = static_cast<int>(std::round(f * N / fs));
    EXPECT_EQ(bin, expected);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phase continuity
// ═══════════════════════════════════════════════════════════════════════════════

// PolyBLEP distributes the 2.0 naive discontinuity across exactly two samples.
// The worst-case per-sample step is ~1.5 (wrap lands midway through the
// correction window). This test asserts two separate properties:
//   1. All samples stay within [-1.1, 1.1] — no hard clipping beyond saw range.
//   2. The maximum inter-sample jump is strictly less than 2.0 — confirming
//      that PolyBLEP is active and has improved on the naive discontinuity.
TEST(OscillatorTest, PhaseContinuityPolyBlepActive)
{
    constexpr double fs = 44100.0;
    constexpr double f  = 440.0;
    constexpr int    N  = 44100; // one second

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(f);

    double maxJump = 0.0;
    float  prev    = osc.process();
    for (int i = 1; i < N; ++i) {
        float cur = osc.process();

        ASSERT_GE(cur, -1.1f) << "sample below range at index " << i;
        ASSERT_LE(cur,  1.1f) << "sample above range at index " << i;

        double d = std::abs(static_cast<double>(cur) - static_cast<double>(prev));
        if (d > maxJump) maxJump = d;
        prev = cur;
    }

    // Naive saw has a 2.0 jump; PolyBLEP must reduce this.
    EXPECT_LT(maxJump, 2.0)
        << "PolyBLEP not reducing discontinuity: maxJump=" << maxJump;
    // And it must not eliminate all dynamics (would mean silent or DC output).
    EXPECT_GT(maxJump, 0.01)
        << "suspiciously small jump — oscillator may be silent";
}

// ═══════════════════════════════════════════════════════════════════════════════
// No NaN / Inf
// ═══════════════════════════════════════════════════════════════════════════════

TEST(OscillatorTest, NoNanOrInf)
{
    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(440.0);

    for (int i = 0; i < 88200; ++i) {
        float s = osc.process();
        ASSERT_FALSE(std::isnan(s)) << "NaN at sample " << i;
        ASSERT_FALSE(std::isinf(s)) << "Inf at sample " << i;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sample-rate switching
// ═══════════════════════════════════════════════════════════════════════════════

// Switching sample rate mid-stream must not produce NaN/Inf and must keep
// output in [-1.1, 1.1] (small PolyBLEP overshoot is acceptable).
TEST(OscillatorTest, SampleRateSwitchNoCrash)
{
    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(440.0);

    // Render 1000 samples at 44.1 kHz
    for (int i = 0; i < 1000; ++i) {
        float s = osc.process();
        ASSERT_FALSE(std::isnan(s));
    }

    // Switch to 96 kHz
    osc.setSampleRate(96000.0);
    for (int i = 0; i < 1000; ++i) {
        float s = osc.process();
        ASSERT_FALSE(std::isnan(s)) << "NaN after SR switch at sample " << i;
        ASSERT_GE(s, -1.1f);
        ASSERT_LE(s,  1.1f);
    }
}

// After a sample-rate change the oscillator must track the same frequency.
TEST(OscillatorTest, SampleRateSwitchFrequencyTracking)
{
    constexpr double f  = 220.0;
    constexpr int    N  = 8192;

    auto checkFreq = [&](double fs) {
        Oscillator osc;
        osc.setSampleRate(fs);
        osc.setFrequency(f);
        auto buf     = render(osc, 0, N);
        int  bin     = dominantBin(buf);
        int  expected = static_cast<int>(std::round(f * N / fs));
        EXPECT_EQ(bin, expected)
            << "@ fs=" << fs << " dominant bin=" << bin
            << " expected=" << expected;
    };

    checkFreq(44100.0);
    checkFreq(48000.0);
    checkFreq(96000.0);
    checkFreq(192000.0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Octave accuracy
// ═══════════════════════════════════════════════════════════════════════════════

// setOctave(+12) must produce exactly double the base frequency in the DFT.
// Each bin is compared to its own independently-computed expected value so that
// DFT bin-quantisation rounding does not chain across the octave ratio.
TEST(OscillatorTest, OctaveUpDoubleFrequency)
{
    constexpr double fs   = 44100.0;
    constexpr double base = 110.0;
    constexpr int    N    = 8192;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(base);

    osc.setOctave(0);
    auto bufBase   = render(osc, 0, N);
    int  binBase   = dominantBin(bufBase);
    int  expBase   = static_cast<int>(std::round(base * N / fs));
    EXPECT_EQ(binBase, expBase)
        << "base bin mismatch: got=" << binBase << " expected=" << expBase;

    osc.reset();
    osc.setOctave(12);
    auto bufOctave = render(osc, 0, N);
    int  binOctave = dominantBin(bufOctave);
    int  expOctave = static_cast<int>(std::round(base * 2.0 * N / fs));
    EXPECT_EQ(binOctave, expOctave)
        << "octave-up bin mismatch: got=" << binOctave << " expected=" << expOctave;
}

// setOctave(-12) must produce half the base frequency.
TEST(OscillatorTest, OctaveDownHalfFrequency)
{
    constexpr double fs   = 44100.0;
    constexpr double base = 220.0;
    constexpr int    N    = 8192;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(base);

    osc.setOctave(0);
    auto bufBase = render(osc, 0, N);
    int  binBase = dominantBin(bufBase);
    int  expBase = static_cast<int>(std::round(base * N / fs));
    EXPECT_EQ(binBase, expBase)
        << "base bin mismatch: got=" << binBase << " expected=" << expBase;

    osc.reset();
    osc.setOctave(-12);
    auto bufDown = render(osc, 0, N);
    int  binDown = dominantBin(bufDown);
    int  expDown = static_cast<int>(std::round(base * 0.5 * N / fs));
    EXPECT_EQ(binDown, expDown)
        << "octave-down bin mismatch: got=" << binDown << " expected=" << expDown;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FFT / spectral verification
// ═══════════════════════════════════════════════════════════════════════════════

// A sawtooth has odd + even harmonics with amplitudes 1/k.
// Verify that the first three harmonics are present and follow the 1/k law
// within a 3 dB tolerance.
TEST(OscillatorTest, SawtoothHarmonicsFollow1OverK)
{
    constexpr double fs   = 44100.0;
    constexpr double f    = 110.0;
    constexpr int    N    = 16384;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(f);

    auto buf = render(osc, static_cast<int>(fs / f * 2), N); // 2-cycle warmup

    int bin1 = static_cast<int>(std::round(f     * N / fs));
    int bin2 = static_cast<int>(std::round(f * 2 * N / fs));
    int bin3 = static_cast<int>(std::round(f * 3 * N / fs));

    double m1 = dftBinMagnitude(buf, bin1);
    double m2 = dftBinMagnitude(buf, bin2);
    double m3 = dftBinMagnitude(buf, bin3);

    ASSERT_GT(m1, 0.05) << "fundamental too weak: " << m1;

    // Ratio m1/m2 should be ~2 (±3 dB = factor of 1.414)
    double r12 = m1 / m2;
    EXPECT_GT(r12, 2.0 / 1.5) << "2nd harmonic too strong: ratio=" << r12;
    EXPECT_LT(r12, 2.0 * 1.5) << "2nd harmonic too weak:   ratio=" << r12;

    // Ratio m1/m3 should be ~3
    double r13 = m1 / m3;
    EXPECT_GT(r13, 3.0 / 1.5) << "3rd harmonic too strong: ratio=" << r13;
    EXPECT_LT(r13, 3.0 * 1.5) << "3rd harmonic too weak:   ratio=" << r13;
}

// The fundamental must dominate: its magnitude must be greater than any
// aliased image above Nyquist/2 that PolyBLEP should have suppressed.
TEST(OscillatorTest, FundamentalDominatesAliasRegion)
{
    constexpr double fs = 44100.0;
    constexpr double f  = 440.0;
    constexpr int    N  = 8192;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(f);

    auto buf = render(osc, 0, N);

    int    fundBin = static_cast<int>(std::round(f * N / fs));
    double fundMag = dftBinMagnitude(buf, fundBin);

    // Scan upper half of spectrum (above fs/4) for alias energy
    double maxAlias = 0.0;
    for (int k = N / 4; k < N / 2; ++k) {
        double m = dftBinMagnitude(buf, k);
        if (m > maxAlias) maxAlias = m;
    }

    // Fundamental must be at least 20 dB above any alias image
    EXPECT_GT(fundMag / maxAlias, 10.0)
        << "alias ratio " << fundMag / maxAlias
        << " (fund=" << fundMag << " maxAlias=" << maxAlias << ")";
}

// ═══════════════════════════════════════════════════════════════════════════════
// No phase drift over long runs
// ═══════════════════════════════════════════════════════════════════════════════

// Run the oscillator for 10 seconds. The phase must never leave [0, 1) and
// must not accumulate floating-point drift (phase stays normalised on each step).
TEST(OscillatorTest, NoPhaseAccumulationDrift)
{
    constexpr double fs  = 44100.0;
    constexpr int    sec = 10;
    constexpr int    N   = static_cast<int>(fs) * sec;

    Oscillator osc;
    osc.setSampleRate(fs);
    osc.setFrequency(440.0);

    for (int i = 0; i < N; ++i) {
        osc.process();
        double p = osc.phase();
        ASSERT_GE(p, 0.0) << "phase went negative at sample " << i;
        ASSERT_LT(p, 1.0) << "phase >= 1.0 at sample " << i;
    }
}
