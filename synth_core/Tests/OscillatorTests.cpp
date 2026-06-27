#include <gtest/gtest.h>
#include "../Include/Oscillator.h"
#include "../Include/DSPUtils.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

static constexpr double kPi = 3.14159265358979323846;

using namespace SynthCore;

// ── Helpers ──────────────────────────────────────────────────────────────────

// Render N mono samples from osc into buf (caller pre-sizes buf).
static void render(Oscillator& osc, std::vector<float>& buf)
{
    for (auto& s : buf) s = osc.process();
}

// Measure frequency via sub-sample-accurate rising zero crossings.
// Returns 0 if fewer than two crossings are found (can't measure a period).
static double measureHz(const std::vector<float>& buf, double sampleRate)
{
    double firstPos = -1.0, lastPos = -1.0;
    int count = 0;

    for (int i = 1; i < static_cast<int>(buf.size()); ++i) {
        if (buf[i - 1] < 0.0f && buf[i] >= 0.0f) {
            // Linear interpolation for sub-sample position
            double frac = static_cast<double>(-buf[i - 1])
                        / static_cast<double>(buf[i] - buf[i - 1]);
            double pos  = static_cast<double>(i - 1) + frac;
            if (firstPos < 0.0) firstPos = pos;
            lastPos = pos;
            ++count;
        }
    }

    if (count < 2) return 0.0;
    double spanSec = (lastPos - firstPos) / sampleRate;
    return static_cast<double>(count - 1) / spanSec;
}

// Convert Hz error to cents: 1200 * log2(measured / expected)
static double toCents(double measured, double expected)
{
    return 1200.0 * std::log2(measured / expected);
}

// Compute the DFT magnitude at a single frequency bin (normalised by N).
static double dftMagnitude(const std::vector<float>& buf,
                           double freqHz, double sampleRate)
{
    double re = 0.0, im = 0.0;
    int    N  = static_cast<int>(buf.size());
    for (int n = 0; n < N; ++n) {
        double angle = 2.0 * kPi * freqHz * n / sampleRate;
        re += static_cast<double>(buf[n]) * std::cos(angle);
        im -= static_cast<double>(buf[n]) * std::sin(angle);
    }
    return std::sqrt(re * re + im * im) / N;
}

static double maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0.0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        d = std::max(d, std::abs(static_cast<double>(a[i] - b[i])));
    return d;
}

// ── Non-silence ───────────────────────────────────────────────────────────────

TEST(OscillatorTest, SawtoothIsNotSilent)
{
    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(440.0);
    osc.setWaveform(OscWaveform::Sawtooth);

    std::vector<float> buf(1024);
    render(osc, buf);

    float peak = *std::max_element(buf.begin(), buf.end(),
        [](float a, float b){ return std::abs(a) < std::abs(b); });
    EXPECT_GT(std::abs(peak), 0.1f);
}

// ── Amplitude bounds ──────────────────────────────────────────────────────────

TEST(OscillatorTest, SawtoothBoundedToUnity)
{
    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setWaveform(OscWaveform::Sawtooth);

    // Test several frequencies across the musical range
    for (int midiNote : {21, 36, 57, 69, 84, 108}) {
        osc.setFrequency(Math::midiNoteToFrequency(midiNote));
        osc.reset();

        std::vector<float> buf(8192);
        render(osc, buf);

        for (int i = 0; i < static_cast<int>(buf.size()); ++i) {
            EXPECT_LE(buf[i],  1.05f) << "MIDI " << midiNote << " sample " << i << " exceeds +1";
            EXPECT_GE(buf[i], -1.05f) << "MIDI " << midiNote << " sample " << i << " below -1";
        }
    }
}

// ── Frequency accuracy: MIDI note range (< 1 cent) ────────────────────────────

struct FreqTestCase {
    int    midiNote;
    double expectedHz;
};

class OscFreqAccuracyTest : public ::testing::TestWithParam<FreqTestCase> {};

TEST_P(OscFreqAccuracyTest, WithinOneCent)
{
    const auto& tc = GetParam();

    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(tc.expectedHz);
    osc.setWaveform(OscWaveform::Sawtooth);
    osc.reset();

    // Render 3 seconds – enough cycles for < 0.05 cent measurement error
    constexpr double kSR = 44100.0;
    int numSamples = static_cast<int>(kSR * 3.0);
    std::vector<float> buf(static_cast<size_t>(numSamples));
    render(osc, buf);

    double measured = measureHz(buf, kSR);
    ASSERT_GT(measured, 0.0) << "No zero crossings found for MIDI " << tc.midiNote;

    double cents = std::abs(toCents(measured, tc.expectedHz));
    EXPECT_LT(cents, 1.0)
        << "MIDI " << tc.midiNote
        << " (" << tc.expectedHz << " Hz): measured " << measured
        << " Hz, error " << cents << " cents";
}

INSTANTIATE_TEST_SUITE_P(MidiRange, OscFreqAccuracyTest, ::testing::Values(
    // A0 = lowest piano note
    FreqTestCase{21,  Math::midiNoteToFrequency(21) },
    // A2
    FreqTestCase{45,  Math::midiNoteToFrequency(45) },
    // A3
    FreqTestCase{57,  Math::midiNoteToFrequency(57) },
    // A4 (concert pitch reference)
    FreqTestCase{69,  440.0                          },
    // A5
    FreqTestCase{81,  Math::midiNoteToFrequency(81) },
    // A6 (high frequency, aliasing risk)
    FreqTestCase{93,  Math::midiNoteToFrequency(93) },
    // C8 = MIDI 108 (highest common piano note)
    FreqTestCase{108, Math::midiNoteToFrequency(108)}
));

// ── Octave transposition accuracy ─────────────────────────────────────────────

TEST(OscillatorTest, OctaveUpDoublesFrequency)
{
    constexpr double kSR = 44100.0;
    constexpr double kBase = 220.0;  // A3

    auto measure = [&](int semitoneOffset) {
        Oscillator osc;
        osc.setSampleRate(kSR);
        osc.setFrequency(kBase);
        osc.setOctave(semitoneOffset);
        osc.setWaveform(OscWaveform::Sawtooth);
        osc.reset();
        std::vector<float> buf(static_cast<size_t>(kSR * 3.0));
        render(osc, buf);
        return measureHz(buf, kSR);
    };

    double f0   = measure(0);
    double fUp  = measure(12);   // +1 octave → should be kBase * 2
    double fDown = measure(-12); // -1 octave → should be kBase * 0.5

    ASSERT_GT(f0,   0.0);
    ASSERT_GT(fUp,  0.0);
    ASSERT_GT(fDown,0.0);

    double centUp   = std::abs(toCents(fUp,   kBase * 2.0));
    double centDown = std::abs(toCents(fDown, kBase * 0.5));

    EXPECT_LT(centUp,   1.0) << "Octave up: measured " << fUp << " Hz (expected " << kBase*2 << ")";
    EXPECT_LT(centDown, 1.0) << "Octave down: measured " << fDown << " Hz (expected " << kBase*0.5 << ")";
}

TEST(OscillatorTest, SemitoneTranspositionAccuracy)
{
    // A4 (440 Hz) + 7 semitones = E5 = 440 * 2^(7/12) ≈ 659.26 Hz
    constexpr double kSR      = 44100.0;
    constexpr double kBase    = 440.0;
    constexpr int    kSemis   = 7;
    const double     kExpected = kBase * std::pow(2.0, kSemis / 12.0);

    Oscillator osc;
    osc.setSampleRate(kSR);
    osc.setFrequency(kBase);
    osc.setOctave(kSemis);
    osc.setWaveform(OscWaveform::Sawtooth);
    osc.reset();

    std::vector<float> buf(static_cast<size_t>(kSR * 3.0));
    render(osc, buf);

    double measured = measureHz(buf, kSR);
    ASSERT_GT(measured, 0.0);
    EXPECT_LT(std::abs(toCents(measured, kExpected)), 1.0)
        << "Expected " << kExpected << " Hz, measured " << measured << " Hz";
}

// ── Sample-rate independence ──────────────────────────────────────────────────

// The same MIDI note played at two different sample rates must land within
// 1 cent of each other (pitch is sample-rate–independent).
TEST(OscillatorTest, SampleRateSwitchMaintainsPitch)
{
    constexpr double kFreq  = 440.0;  // A4

    auto measureAt = [&](double sr) {
        Oscillator osc;
        osc.setSampleRate(sr);
        osc.setFrequency(kFreq);
        osc.setWaveform(OscWaveform::Sawtooth);
        osc.reset();
        int N = static_cast<int>(sr * 3.0);
        std::vector<float> buf(static_cast<size_t>(N));
        render(osc, buf);
        return measureHz(buf, sr);
    };

    double f44 = measureAt(44100.0);
    double f96 = measureAt(96000.0);

    ASSERT_GT(f44, 0.0);
    ASSERT_GT(f96, 0.0);
    EXPECT_LT(std::abs(toCents(f44, kFreq)), 1.0) << "44.1 kHz: " << f44 << " Hz";
    EXPECT_LT(std::abs(toCents(f96, kFreq)), 1.0) << "96 kHz: "   << f96 << " Hz";
    EXPECT_LT(std::abs(toCents(f44, f96)),   1.0) << "Cross-rate: " << f44 << " vs " << f96 << " Hz";
}

// Switching sample rate mid-oscillator life must update pitch correctly.
TEST(OscillatorTest, MidLifeSampleRateChangeUpdatesPhaseInc)
{
    constexpr double kFreq = 220.0;

    Oscillator osc;
    osc.setFrequency(kFreq);
    osc.setWaveform(OscWaveform::Sawtooth);

    // Run 1 second at 44100
    osc.setSampleRate(44100.0);
    osc.reset();
    {
        std::vector<float> buf(44100);
        render(osc, buf);
        double f = measureHz(buf, 44100.0);
        EXPECT_LT(std::abs(toCents(f, kFreq)), 1.0) << "44.1 kHz phase: " << f;
    }

    // Switch to 96000 and verify pitch is still correct
    osc.setSampleRate(96000.0);
    osc.reset();
    {
        std::vector<float> buf(96000);
        render(osc, buf);
        double f = measureHz(buf, 96000.0);
        EXPECT_LT(std::abs(toCents(f, kFreq)), 1.0) << "96 kHz phase: " << f;
    }
}

// ── Phase continuity ──────────────────────────────────────────────────────────

// reset() must produce the same deterministic sequence each call.
TEST(OscillatorTest, ResetGivesDeterministicOutput)
{
    Oscillator osc;
    osc.setSampleRate(44100.0);
    osc.setFrequency(440.0);
    osc.setWaveform(OscWaveform::Sawtooth);

    constexpr int kFrames = 512;
    std::vector<float> run1(kFrames), run2(kFrames);

    osc.reset();
    render(osc, run1);

    osc.reset();
    render(osc, run2);

    for (int i = 0; i < kFrames; ++i)
        EXPECT_FLOAT_EQ(run1[i], run2[i]) << "Diverged at sample " << i;
}

// ── Spectral / aliasing sanity ────────────────────────────────────────────────

// For A2 (110 Hz) at 44100 Hz the fundamental DFT bin must carry significantly
// more energy than a high alias bin (PolyBLEP knocks down alias content).
TEST(OscillatorTest, FundamentalDominatesOverAliasContent)
{
    constexpr double kSR   = 44100.0;
    constexpr double kFreq = 110.0;    // A2

    Oscillator osc;
    osc.setSampleRate(kSR);
    osc.setFrequency(kFreq);
    osc.setWaveform(OscWaveform::Sawtooth);
    osc.reset();

    // 0.5-second window — long enough for good spectral resolution
    constexpr int kN = static_cast<int>(kSR * 0.5);
    std::vector<float> buf(kN);
    render(osc, buf);

    double magFundamental = dftMagnitude(buf, kFreq, kSR);

    // The first alias of a naive saw at 110 Hz sits at sampleRate - 110 = 43990 Hz,
    // which folds to 44100 - 43990 = 110 Hz... not helpful.
    // Use the 201st harmonic alias: 201*110 = 22110 Hz > Nyquist (22050 Hz),
    // folds back to 44100 - 22110 = 21990 Hz. With PolyBLEP it should be near zero.
    double magAlias = dftMagnitude(buf, 21990.0, kSR);

    EXPECT_GT(magFundamental, 0.1)
        << "Fundamental at " << kFreq << " Hz is unexpectedly weak: " << magFundamental;
    EXPECT_GT(magFundamental, magAlias * 20.0)
        << "Alias at 21990 Hz not sufficiently suppressed. "
        << "Fundamental=" << magFundamental << " Alias=" << magAlias;
}

// At A5 (880 Hz) the PolyBLEP-corrected saw must still have a strong fundamental.
TEST(OscillatorTest, HighFrequencyFundamentalPresent)
{
    constexpr double kSR   = 44100.0;
    constexpr double kFreq = 880.0;  // A5

    Oscillator osc;
    osc.setSampleRate(kSR);
    osc.setFrequency(kFreq);
    osc.setWaveform(OscWaveform::Sawtooth);
    osc.reset();

    constexpr int kN = static_cast<int>(kSR * 0.5);
    std::vector<float> buf(kN);
    render(osc, buf);

    double mag = dftMagnitude(buf, kFreq, kSR);
    EXPECT_GT(mag, 0.1) << "880 Hz fundamental weak: " << mag;
}

TEST(OscillatorTest, PulseWidthAffectsOnlyPulseCapableWaveforms)
{
    auto renderWithPulseWidth = [](Waveform waveform, double pulseWidth) {
        Oscillator osc;
        osc.setSampleRate(44100.0);
        osc.setFrequency(220.0);
        osc.setWaveform(waveform);
        osc.setPulseWidth(pulseWidth);
        osc.reset();

        std::vector<float> buf(4096);
        render(osc, buf);
        return buf;
    };

    for (auto waveform : {Waveform::Square, Waveform::WidePulse, Waveform::NarrowPulse}) {
        const auto narrow = renderWithPulseWidth(waveform, 0.10);
        const auto wide = renderWithPulseWidth(waveform, 0.90);
        EXPECT_GT(maxAbsDiff(narrow, wide), 0.05)
            << "PW must affect pulse-capable waveforms";
    }

    for (auto waveform : {Waveform::Triangle, Waveform::TriangleSaw, Waveform::Saw, Waveform::ReverseSaw}) {
        const auto narrow = renderWithPulseWidth(waveform, 0.10);
        const auto wide = renderWithPulseWidth(waveform, 0.90);
        EXPECT_LT(maxAbsDiff(narrow, wide), 1.0e-6)
            << "PW must not affect non-pulse waveforms";
    }
}
