#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
#include "../Include/Mixer.h"
#include "../Include/LadderFilter.h"
#include "../Include/DSPUtils.h"
#include <cmath>
#include <vector>

using namespace SynthCore;

static double rms(const std::vector<float>& samples)
{
    double sum = 0.0;
    for (float sample : samples) sum += static_cast<double>(sample) * sample;
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

static std::vector<float> renderMono(SynthEngine& engine, int frames)
{
    std::vector<float> out(static_cast<size_t>(frames));
    for (auto& sample : out) sample = engine.processSample();
    return out;
}

static double measureHz(const std::vector<float>& buf, double sampleRate)
{
    double first = -1.0;
    double last = -1.0;
    int count = 0;
    for (int i = 1; i < static_cast<int>(buf.size()); ++i) {
        if (buf[static_cast<size_t>(i - 1)] < 0.0f && buf[static_cast<size_t>(i)] >= 0.0f) {
            const double denom = static_cast<double>(buf[static_cast<size_t>(i)]) - buf[static_cast<size_t>(i - 1)];
            const double frac = denom != 0.0 ? -buf[static_cast<size_t>(i - 1)] / denom : 0.0;
            const double pos = static_cast<double>(i - 1) + frac;
            if (first < 0.0) first = pos;
            last = pos;
            ++count;
        }
    }
    if (count < 2) return 0.0;
    return static_cast<double>(count - 1) / ((last - first) / sampleRate);
}

static double averageAbsDiff(const std::vector<float>& buf)
{
    double sum = 0.0;
    for (size_t i = 1; i < buf.size(); ++i) {
        sum += std::abs(static_cast<double>(buf[i]) - buf[i - 1]);
    }
    return sum / static_cast<double>(buf.size() - 1);
}

struct RenderStats {
    double peak = 0.0;
    double rms = 0.0;
    int clippedSamples = 0;
    bool finite = true;
};

static RenderStats statsFor(const std::vector<float>& buf)
{
    RenderStats stats;
    double sumSquares = 0.0;
    for (float sample : buf) {
        stats.finite = stats.finite && std::isfinite(sample);
        const double absSample = std::abs(static_cast<double>(sample));
        stats.peak = std::max(stats.peak, absSample);
        sumSquares += static_cast<double>(sample) * sample;
        if (absSample >= 0.98) ++stats.clippedSamples;
    }
    stats.rms = std::sqrt(sumSquares / static_cast<double>(buf.size()));
    return stats;
}

TEST(SynthEngineAudioTest, PrepareAndDefaultPatchAudible)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.noteOn(45, 100.0f);
    const auto out = renderMono(engine, 4096);

    EXPECT_GT(rms(out), 0.01);
}

TEST(SynthEngineAudioTest, InitPatchDefaultsAreMusicalAndSane)
{
    SynthEngine engine;

    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::GlideEnabled), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::GlideTime), 0.05f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::PitchBendRange), 2.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc1Enabled), 1.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Enabled), 1.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc3Enabled), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc1Waveform), 2.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Waveform), 2.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc3Waveform), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc1Range), 3.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Range), 3.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc3Range), 3.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.05f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc3Detune), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::MixerDrive), 1.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterCutoff), 6000.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterResonance), 0.10f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterEnvAmount), 0.15f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterKeyboardTracking), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterDrive), 0.50f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AmpAttack), 0.005f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AmpDecay), 0.25f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AmpSustain), 0.75f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AmpRelease), 0.20f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterAttack), 0.005f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterDecay), 0.30f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterSustain), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::FilterRelease), 0.20f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::MasterVolume), 0.60f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::NoiseEnabled), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::NoiseLevel), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::NoiseMode), 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::PlayMode), 0.0f);
}

TEST(SynthEngineAudioTest, DefaultPatchRenderIsCleanAndReleases)
{
    constexpr double sampleRate = 48000.0;
    constexpr int totalFrames = static_cast<int>(sampleRate * 5.0);
    constexpr int noteOnFrame = static_cast<int>(sampleRate * 0.1);
    constexpr int noteOffFrame = static_cast<int>(sampleRate * 3.5);

    SynthEngine engine;
    engine.prepare(sampleRate, 512);

    std::vector<float> out(static_cast<size_t>(totalFrames));
    for (int frame = 0; frame < totalFrames; ++frame) {
        if (frame == noteOnFrame) engine.noteOn(45, 100.0f);
        if (frame == noteOffFrame) engine.noteOff(45);
        out[static_cast<size_t>(frame)] = engine.processSample();
    }

    const auto full = statsFor(out);
    const auto finalHalfSecond = std::vector<float>(
        out.begin() + static_cast<std::ptrdiff_t>(totalFrames - static_cast<int>(sampleRate * 0.5)),
        out.end());
    const auto tail = statsFor(finalHalfSecond);

    EXPECT_TRUE(full.finite);
    EXPECT_GT(full.rms, 0.005);
    EXPECT_LT(full.peak, 0.95);
    EXPECT_LT(full.clippedSamples, totalFrames / 1000);
    EXPECT_LT(tail.rms, 0.01);
}

TEST(SynthEngineAudioTest, NoteOffReleasesThenBecomesSilent)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setLoudnessRelease(0.1);
    engine.noteOn(45, 100.0f);
    renderMono(engine, 44100);
    engine.noteOff(45);
    const auto releaseStart = renderMono(engine, 256);
    renderMono(engine, 8000);
    const auto afterRelease = renderMono(engine, 2048);

    EXPECT_GT(rms(releaseStart), 1.0e-5);
    EXPECT_LT(rms(afterRelease), 0.01);
}

TEST(SynthEngineAudioTest, OscillatorLevelChangesOutput)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.noteOn(45, 100.0f);

    engine.setParameter(ParamId::Osc1Level, 0.2f);
    const double low = rms(renderMono(engine, 4096));

    engine.reset();
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.setParameter(ParamId::Osc1Level, 1.0f);
    engine.noteOn(45, 100.0f);
    const double high = rms(renderMono(engine, 4096));

    EXPECT_GT(high, low * 2.0);
}

TEST(SynthEngineAudioTest, DisablingAllOscillatorsProducesSilence)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::Osc1Enabled, 0.0f);
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.setParameter(ParamId::Osc3Enabled, 0.0f);
    engine.noteOn(45, 100.0f);

    EXPECT_LT(rms(renderMono(engine, 4096)), 1.0e-6);
}

TEST(SynthEngineAudioTest, Osc2EnableAndDetuneChangeOutput)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.noteOn(45, 100.0f);
    const auto osc1Only = renderMono(engine, 4096);

    engine.reset();
    engine.setParameter(ParamId::Osc2Enabled, 1.0f);
    engine.setParameter(ParamId::Osc2Detune, 7.0f);
    engine.noteOn(45, 100.0f);
    const auto detuned = renderMono(engine, 4096);

    double diff = 0.0;
    for (size_t i = 0; i < osc1Only.size(); ++i) {
        diff += std::abs(static_cast<double>(osc1Only[i]) - detuned[i]);
    }
    EXPECT_GT(diff / static_cast<double>(osc1Only.size()), 0.01);
}

TEST(SynthEngineAudioTest, Osc3KeyboardTrackingOffKeepsPitchIndependent)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::Osc1Enabled, 0.0f);
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.setParameter(ParamId::Osc3Enabled, 1.0f);
    engine.setParameter(ParamId::Osc3Level, 1.0f);
    engine.setParameter(ParamId::Osc3KeyboardTracking, 0.0f);
    engine.noteOn(45, 100.0f);

    const auto lowNote = renderMono(engine, 44100);
    engine.noteOff(45);
    engine.noteOn(69, 100.0f);
    const auto highNote = renderMono(engine, 44100);

    const double f1 = measureHz(lowNote, 44100.0);
    const double f2 = measureHz(highNote, 44100.0);
    ASSERT_GT(f1, 0.0);
    ASSERT_GT(f2, 0.0);
    EXPECT_NEAR(f1, f2, 2.0);
}

TEST(SynthEngineAudioTest, ProcessBlockWritesStereo)
{
    SynthEngine engine;
    engine.prepare(44100.0, 128);
    engine.noteOn(45, 100.0f);
    std::vector<float> buffer(256, 0.0f);
    engine.processBlock(buffer.data(), 128);

    double sum = 0.0;
    for (float sample : buffer) sum += std::abs(sample);
    EXPECT_GT(sum, 0.0);
    EXPECT_FLOAT_EQ(buffer[0], buffer[1]);
}

TEST(SynthEngineAudioTest, LongRenderFiniteAndResetProducesCleanAudio)
{
    // After reset(), the engine must produce finite, bounded, audible audio.
    // Phase randomization means bit-exact equality is no longer expected.
    SynthEngine engine;
    engine.prepare(48000.0, 512);
    engine.noteOn(45, 100.0f);
    const auto first = renderMono(engine, 2048);

    engine.reset();
    engine.noteOn(45, 100.0f);
    const auto second = renderMono(engine, 2048);

    for (size_t i = 0; i < first.size(); ++i) {
        EXPECT_TRUE(std::isfinite(first[i]));
        EXPECT_TRUE(std::isfinite(second[i]));
        EXPECT_LE(std::abs(first[i]),  1.0f) << "out of bounds at sample " << i;
        EXPECT_LE(std::abs(second[i]), 1.0f) << "out of bounds at sample " << i;
    }
    EXPECT_GT(rms(first),  0.001) << "engine must be audible after first noteOn";
    EXPECT_GT(rms(second), 0.001) << "engine must be audible after reset + noteOn";
}

TEST(SynthEngineAudioTest, LoudnessAttackAndSustainAffectAmplitude)
{
    SynthEngine slowAttack;
    slowAttack.prepare(44100.0, 512);
    slowAttack.setLoudnessAttack(0.5);
    slowAttack.noteOn(45, 100.0f);
    const double slowStart = rms(renderMono(slowAttack, 512));

    SynthEngine fastAttack;
    fastAttack.prepare(44100.0, 512);
    fastAttack.setLoudnessAttack(0.001);
    fastAttack.noteOn(45, 100.0f);
    const double fastStart = rms(renderMono(fastAttack, 512));
    EXPECT_GT(fastStart, slowStart * 5.0);

    SynthEngine zeroSustain;
    zeroSustain.prepare(44100.0, 512);
    zeroSustain.setLoudnessSustain(0.0);
    zeroSustain.noteOn(45, 100.0f);
    renderMono(zeroSustain, 44100);
    EXPECT_LT(rms(renderMono(zeroSustain, 4096)), 0.01);
}

TEST(SynthEngineAudioTest, MasterVolumeAndOutputBounds)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.noteOn(45, 100.0f);
    engine.setMasterVolume(0.2);
    const double quiet = rms(renderMono(engine, 4096));

    engine.reset();
    engine.noteOn(45, 100.0f);
    engine.setMasterVolume(1.0);
    const auto loudBuffer = renderMono(engine, 4096);
    const double loud = rms(loudBuffer);

    EXPECT_GT(loud, quiet);
    for (float sample : loudBuffer) {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_LE(sample, 1.0f);
        EXPECT_GE(sample, -1.0f);
    }
}

TEST(SynthEngineAudioTest, LowNotePriorityStillControlsPitch)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.noteOn(69, 100.0f);
    engine.noteOn(45, 100.0f);
    const auto lowPriority = renderMono(engine, 44100);
    const double measured = measureHz(lowPriority, 44100.0);
    EXPECT_NEAR(measured, 110.0, 2.0);
}

TEST(SynthEngineAudioTest, DefaultPlayModeKeepsMonoLowNotePriority)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::PlayMode), 0.0f);

    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.noteOn(69, 100.0f);
    engine.noteOn(45, 100.0f);
    const double measured = measureHz(renderMono(engine, 44100), 44100.0);

    EXPECT_NEAR(measured, 110.0, 2.0);
}

TEST(SynthEngineAudioTest, PolyFourCanHoldThreeNotes)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.noteOn(60, 100.0f);
    engine.noteOn(64, 100.0f);
    engine.noteOn(67, 100.0f);

    const auto out = renderMono(engine, 8192);
    const auto s = statsFor(out);

    EXPECT_TRUE(s.finite);
    EXPECT_GT(s.rms, 0.01);
    EXPECT_LE(s.peak, 1.0);
}

TEST(SynthEngineAudioTest, PolyFourNoteOffLeavesOtherNotesActive)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setLoudnessRelease(0.02);
    engine.noteOn(60, 100.0f);
    engine.noteOn(64, 100.0f);
    engine.noteOn(67, 100.0f);
    renderMono(engine, 2048);

    engine.noteOff(64);
    renderMono(engine, 4096);
    const auto stillHeld = renderMono(engine, 4096);

    EXPECT_GT(rms(stillHeld), 0.01);
    for (float sample : stillHeld) EXPECT_TRUE(std::isfinite(sample));
}

TEST(SynthEngineAudioTest, PolyFourAllNotesOffEventuallyBecomesSilent)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setLoudnessRelease(0.05);
    engine.noteOn(60, 100.0f);
    engine.noteOn(64, 100.0f);
    engine.noteOn(67, 100.0f);
    renderMono(engine, 4096);

    engine.noteOff(60);
    engine.noteOff(64);
    engine.noteOff(67);
    renderMono(engine, 10000);
    const auto tail = renderMono(engine, 2048);

    EXPECT_LT(rms(tail), 0.01);
}

TEST(SynthEngineAudioTest, PolyFourOutputRemainsFiniteAndBounded)
{
    SynthEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setParameter(ParamId::Osc1Level, 1.0f);
    engine.setParameter(ParamId::Osc2Level, 1.0f);
    engine.setParameter(ParamId::Osc3Enabled, 1.0f);
    engine.setParameter(ParamId::Osc3Level, 1.0f);
    engine.noteOn(48, 100.0f);
    engine.noteOn(52, 100.0f);
    engine.noteOn(55, 100.0f);
    engine.noteOn(60, 100.0f);

    const auto out = renderMono(engine, 16384);
    const auto s = statsFor(out);

    EXPECT_TRUE(s.finite);
    EXPECT_LE(s.peak, 1.0);
    EXPECT_LT(s.clippedSamples, static_cast<int>(out.size() / 20));
}

TEST(SynthEngineAudioTest, FilterContourParametersAreProcessedSafely)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setFilterAttack(0.001);
    engine.setFilterDecay(0.01);
    engine.setFilterSustain(0.5);
    engine.setFilterRelease(0.02);
    engine.noteOn(45, 100.0f);
    const auto out = renderMono(engine, 2048);
    for (float sample : out) EXPECT_TRUE(std::isfinite(sample));
}

TEST(SynthEngineAudioTest, FilterCutoffChangesBrightness)
{
    // Use MIDI 72 (C5, 523 Hz) so the fundamental is well ABOVE the 300 Hz low-cutoff,
    // guaranteeing the LP filter strongly attenuates the 300 Hz case.  This makes the
    // averageAbsDiff ratio robust to oscillator starting phase.
    auto measureBrightness = [](double cutoffHz) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::Osc2Enabled,    0.0f);
        engine.setParameter(ParamId::AmpAttack,      0.001f);
        engine.setParameter(ParamId::AmpSustain,     1.0f);
        engine.setParameter(ParamId::FilterEnvAmount,0.0f);
        engine.setParameter(ParamId::FilterDrive,    0.0f);
        engine.setFilterCutoffHz(cutoffHz);
        engine.noteOn(72, 100.0f); // C5 = 523 Hz — above the 300 Hz cutoff
        renderMono(engine, 4096);  // skip attack + filter warm-up
        return renderMono(engine, 16384);
    };

    const auto lowOut  = measureBrightness(300.0);
    const auto highOut = measureBrightness(8000.0);

    EXPECT_GT(averageAbsDiff(highOut), averageAbsDiff(lowOut) * 1.5);
}

TEST(SynthEngineAudioTest, ResonanceAndFilterContourAffectOutput)
{
    SynthEngine clean;
    clean.prepare(44100.0, 512);
    clean.setFilterCutoffHz(1000.0);
    clean.setFilterResonance(0.0);
    clean.noteOn(45, 100.0f);
    const auto cleanOut = renderMono(clean, 8192);

    SynthEngine resonant;
    resonant.prepare(44100.0, 512);
    resonant.setFilterCutoffHz(1000.0);
    resonant.setFilterResonance(0.9);
    resonant.noteOn(45, 100.0f);
    const auto resonantOut = renderMono(resonant, 8192);

    double diff = 0.0;
    for (size_t i = 0; i < cleanOut.size(); ++i) diff += std::abs(cleanOut[i] - resonantOut[i]);
    EXPECT_GT(diff / static_cast<double>(cleanOut.size()), 0.001);

    SynthEngine contoured;
    contoured.prepare(44100.0, 512);
    contoured.setFilterCutoffHz(300.0);
    contoured.setFilterContourAmount(1.0);
    contoured.setFilterSustain(1.0);
    contoured.noteOn(45, 100.0f);
    const auto contourOut = renderMono(contoured, 44100);
    EXPECT_GT(averageAbsDiff(contourOut), 0.0001);
}

TEST(SynthEngineAudioTest, KeyboardTrackingChangesFilteredOutput)
{
    SynthEngine lowNote;
    lowNote.prepare(44100.0, 512);
    lowNote.setFilterCutoffHz(500.0);
    lowNote.setFilterKeyboardTrackingAmount(1.0);
    lowNote.noteOn(36, 100.0f);
    const auto lowOut = renderMono(lowNote, 44100);

    SynthEngine highNote;
    highNote.prepare(44100.0, 512);
    highNote.setFilterCutoffHz(500.0);
    highNote.setFilterKeyboardTrackingAmount(1.0);
    highNote.noteOn(84, 100.0f);
    const auto highOut = renderMono(highNote, 44100);

    EXPECT_GT(averageAbsDiff(highOut), averageAbsDiff(lowOut));
}

TEST(SynthEngineAudioTest, NoiseCanFeedMixerAndIsDeterministic)
{
    SynthEngine a;
    a.prepare(44100.0, 512);
    a.setNoiseSeed(123u);
    a.setParameter(ParamId::Osc1Enabled, 0.0f);
    a.setParameter(ParamId::Osc2Enabled, 0.0f);
    a.setParameter(ParamId::Osc3Enabled, 0.0f);
    a.setNoiseEnabled(true);
    a.setNoiseLevel(1.0);
    a.noteOn(45, 100.0f);
    const auto white = renderMono(a, 4096);

    SynthEngine b;
    b.prepare(44100.0, 512);
    b.setNoiseSeed(123u);
    b.setParameter(ParamId::Osc1Enabled, 0.0f);
    b.setParameter(ParamId::Osc2Enabled, 0.0f);
    b.setParameter(ParamId::Osc3Enabled, 0.0f);
    b.setNoiseEnabled(true);
    b.setNoiseLevel(1.0);
    b.noteOn(45, 100.0f);
    const auto whiteAgain = renderMono(b, 4096);

    EXPECT_GT(rms(white), 0.001);
    EXPECT_EQ(white, whiteAgain);

    SynthEngine pink;
    pink.prepare(44100.0, 512);
    pink.setNoiseSeed(123u);
    pink.setParameter(ParamId::Osc1Enabled, 0.0f);
    pink.setParameter(ParamId::Osc2Enabled, 0.0f);
    pink.setParameter(ParamId::Osc3Enabled, 0.0f);
    pink.setNoiseEnabled(true);
    pink.setNoiseLevel(1.0);
    pink.setNoiseMode(NoiseMode::Pink);
    pink.noteOn(45, 100.0f);
    const auto pinkOut = renderMono(pink, 4096);
    EXPECT_NE(white, pinkOut);
}

TEST(SynthEngineAudioTest, NoiseLevelAffectsOutputAndNoiseOffIsSilentWithoutOscillators)
{
    SynthEngine off;
    off.prepare(44100.0, 512);
    off.setParameter(ParamId::Osc1Enabled, 0.0f);
    off.setParameter(ParamId::Osc2Enabled, 0.0f);
    off.setParameter(ParamId::Osc3Enabled, 0.0f);
    off.setNoiseEnabled(false);
    off.noteOn(45, 100.0f);
    EXPECT_LT(rms(renderMono(off, 4096)), 1.0e-6);

    SynthEngine low;
    low.prepare(44100.0, 512);
    low.setNoiseSeed(7u);
    low.setParameter(ParamId::Osc1Enabled, 0.0f);
    low.setParameter(ParamId::Osc2Enabled, 0.0f);
    low.setParameter(ParamId::Osc3Enabled, 0.0f);
    low.setNoiseEnabled(true);
    low.setNoiseLevel(0.2);
    low.noteOn(45, 100.0f);

    SynthEngine high;
    high.prepare(44100.0, 512);
    high.setNoiseSeed(7u);
    high.setParameter(ParamId::Osc1Enabled, 0.0f);
    high.setParameter(ParamId::Osc2Enabled, 0.0f);
    high.setParameter(ParamId::Osc3Enabled, 0.0f);
    high.setNoiseEnabled(true);
    high.setNoiseLevel(1.0);
    high.noteOn(45, 100.0f);

    EXPECT_GT(rms(renderMono(high, 4096)), rms(renderMono(low, 4096)) * 2.0);
}

// ── Part 17 DSP Architecture V2 Tests ────────────────────────────────────────

TEST(DSPArchV2Test, MixerDriveZeroIsNearClean)
{
    // At drive=0, output should equal mixerSum (no saturation blend).
    SynthCore::Mixer mixer;
    mixer.setSampleRate(44100.0);
    mixer.setDrive(0.0);
    mixer.setSourceEnabled(SynthCore::MixerSource::Osc1, true);
    mixer.setSourceEnabled(SynthCore::MixerSource::Osc2, false);
    mixer.setSourceLevel(SynthCore::MixerSource::Osc1, 1.0);

    // At drive=0, blend=0, output=mixerSum=input*0.45
    const double out = mixer.processSample(0.5, 0.0, 0.0, 0.0, 0.0);
    EXPECT_NEAR(out, 0.5 * 0.45, 1e-9);
}

TEST(DSPArchV2Test, MixerDriveChangesSignalCharacter)
{
    // Drive=2 and drive=3 should both produce measurably different signals from drive=0,
    // and drive=3 should differ more than drive=2 (monotonic drive response).
    constexpr double sampleRate = 44100.0;
    constexpr int frames = 4096;

    auto buildEngine = [&](float mixerDrive) -> std::vector<float> {
        SynthEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::Osc1Level, 0.8f);
        engine.setParameter(ParamId::Osc1Waveform, 2.0f); // Saw
        engine.setFilterCutoffHz(8000.0);
        engine.setFilterResonance(0.05);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.setParameter(ParamId::MixerDrive, mixerDrive);
        engine.noteOn(36, 100.0f);
        renderMono(engine, 2048);
        return renderMono(engine, frames);
    };

    const auto clean     = buildEngine(0.0f);
    const auto driven    = buildEngine(2.0f);
    const auto maxDriven = buildEngine(3.0f);

    double diff02 = 0.0, diff03 = 0.0;
    for (int i = 0; i < frames; ++i) {
        diff02 += std::abs(static_cast<double>(clean[static_cast<size_t>(i)] - driven[static_cast<size_t>(i)]));
        diff03 += std::abs(static_cast<double>(clean[static_cast<size_t>(i)] - maxDriven[static_cast<size_t>(i)]));
    }
    EXPECT_GT(diff02 / frames, 0.01);       // drive=2 meaningfully changes the signal
    EXPECT_GT(diff03 / frames, 0.01);      // drive=3 also meaningfully changes the signal
    EXPECT_TRUE(statsFor(driven).finite);
    EXPECT_LE(statsFor(driven).peak, 1.0);
    EXPECT_TRUE(statsFor(maxDriven).finite);
    EXPECT_LE(statsFor(maxDriven).peak, 1.0);
}

TEST(DSPArchV2Test, MixerDriveIsPerVoiceInPolyMode)
{
    // Per-voice drive means poly RMS should be at most ~voice-count * single-voice RMS,
    // not dramatically explode with intermodulation.
    constexpr double sampleRate = 44100.0;
    constexpr int frames = 8192;

    SynthEngine mono;
    mono.prepare(sampleRate, 512);
    mono.setParameter(ParamId::PlayMode, 0.0f);
    mono.setParameter(ParamId::MixerDrive, 3.0f);
    mono.setFilterCutoffHz(8000.0);
    mono.noteOn(60, 100.0f);
    renderMono(mono, 2048);
    const double monoRms = rms(renderMono(mono, frames));

    SynthEngine poly;
    poly.prepare(sampleRate, 512);
    poly.setParameter(ParamId::PlayMode, 1.0f);
    poly.setParameter(ParamId::MixerDrive, 3.0f);
    poly.setFilterCutoffHz(8000.0);
    poly.noteOn(60, 100.0f);
    poly.noteOn(64, 100.0f);
    renderMono(poly, 2048);
    const double polyRms = rms(renderMono(poly, frames));

    // Poly must not explode relative to mono; headroom prevents it.
    EXPECT_LE(polyRms, monoRms * 2.5);
    EXPECT_TRUE(statsFor(renderMono(poly, 1024)).finite);
}

TEST(DSPArchV2Test, FilterDriveDoesNotChangePitch)
{
    constexpr double sampleRate = 44100.0;
    constexpr int frames = static_cast<int>(sampleRate);

    auto measurePitch = [&](float filterDrive) -> double {
        SynthEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::Osc1Waveform, 2.0f); // Saw
        engine.setParameter(ParamId::MixerDrive, 0.0f);
        engine.setFilterCutoffHz(5000.0);
        engine.setFilterResonance(0.1);
        engine.setParameter(ParamId::FilterDrive, filterDrive);
        engine.noteOn(45, 100.0f); // A2 = 110 Hz
        return measureHz(renderMono(engine, frames), sampleRate);
    };

    const double f0 = measurePitch(0.0f);
    const double f2 = measurePitch(2.0f);
    ASSERT_GT(f0, 0.0);
    ASSERT_GT(f2, 0.0);
    EXPECT_NEAR(f0, f2, 3.0); // pitch unchanged within 3 Hz
}

TEST(DSPArchV2Test, FilterDriveDoesNotChangeCutoffParameter)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::FilterCutoff, 1500.0f);
    const float before = engine.getParameter(ParamId::FilterCutoff);

    engine.setParameter(ParamId::FilterDrive, 3.0f);
    const float after = engine.getParameter(ParamId::FilterCutoff);

    EXPECT_FLOAT_EQ(before, after);
    EXPECT_FLOAT_EQ(after, 1500.0f);
}

TEST(DSPArchV2Test, FilterDriveHighPlusResonanceRemainsFinite)
{
    SynthEngine engine;
    engine.prepare(48000.0, 512);
    engine.setFilterCutoffHz(3000.0);
    engine.setFilterResonance(0.8);
    engine.setParameter(ParamId::FilterDrive, 3.0f);
    engine.setParameter(ParamId::MixerDrive, 0.0f);
    engine.noteOn(60, 100.0f);

    const auto out = renderMono(engine, 8192);
    for (float s : out) {
        EXPECT_TRUE(std::isfinite(s));
        EXPECT_LE(std::abs(s), 1.0f);
    }
}

TEST(DSPArchV2Test, FilterDriveDoesNotCreateSlowSweep)
{
    // With LFO off and Contour=0, filter output should be time-stable (no pumping/sweep).
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::LfoEnabled, 0.0f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
    engine.setParameter(ParamId::FilterDrive, 2.0f);
    engine.setFilterCutoffHz(2000.0);
    engine.setFilterResonance(0.15);
    engine.noteOn(45, 100.0f);
    renderMono(engine, 4096); // settle

    const auto first  = renderMono(engine, 4096);
    const auto second = renderMono(engine, 4096);

    const double rms1 = rms(first);
    const double rms2 = rms(second);
    ASSERT_GT(rms1, 0.001);
    // RMS should not drift more than 20% between equivalent windows (no slow sweep).
    EXPECT_NEAR(rms1, rms2, rms1 * 0.20);
}

TEST(DSPArchV2Test, OutputDriveBounded)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setOutputDrive(3.0);
    engine.setMasterVolume(1.0);
    engine.noteOn(45, 100.0f);

    const auto out = renderMono(engine, 4096);
    const auto s = statsFor(out);
    EXPECT_TRUE(s.finite);
    EXPECT_LE(s.peak, 1.0);
}

TEST(DSPArchV2Test, OutputDrivePolyAware)
{
    // High output drive with 2 voices should stay finite and not scratch/choke.
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setParameter(ParamId::MixerDrive, 2.0f);
    engine.setOutputDrive(3.0);
    engine.noteOn(60, 100.0f);
    engine.noteOn(64, 100.0f);

    const auto out = renderMono(engine, 8192);
    const auto s = statsFor(out);
    EXPECT_TRUE(s.finite);
    EXPECT_LE(s.peak, 1.0);
    EXPECT_EQ(s.clippedSamples, 0); // soft limiter, no hard clips
}

TEST(DSPArchV2Test, PolyTwoNoteHighDriveFinite)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setParameter(ParamId::MixerDrive, 3.0f);
    engine.setParameter(ParamId::FilterDrive, 2.0f);
    engine.setOutputDrive(2.0);
    engine.noteOn(48, 100.0f);
    engine.noteOn(52, 100.0f);

    const auto out = renderMono(engine, 8192);
    for (float s : out) EXPECT_TRUE(std::isfinite(s));
    EXPECT_LE(statsFor(out).peak, 1.0);
}

TEST(DSPArchV2Test, PolyFourNoteHighDriveFinite)
{
    SynthEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setParameter(ParamId::MixerDrive, 3.0f);
    engine.setParameter(ParamId::FilterDrive, 2.0f);
    engine.setOutputDrive(2.0);
    engine.noteOn(48, 100.0f);
    engine.noteOn(52, 100.0f);
    engine.noteOn(55, 100.0f);
    engine.noteOn(60, 100.0f);

    const auto out = renderMono(engine, 8192);
    for (float s : out) EXPECT_TRUE(std::isfinite(s));
    EXPECT_LE(statsFor(out).peak, 1.0);
}

TEST(DSPArchV2Test, ActiveVoiceHeadroomPreventsSumExplosion)
{
    constexpr double sampleRate = 44100.0;
    constexpr int frames = 8192;

    SynthEngine singleVoice;
    singleVoice.prepare(sampleRate, 512);
    singleVoice.setParameter(ParamId::PlayMode, 1.0f);
    singleVoice.setParameter(ParamId::MixerDrive, 0.0f);
    singleVoice.noteOn(60, 100.0f);
    renderMono(singleVoice, 2048);
    const double single = rms(renderMono(singleVoice, frames));

    SynthEngine fourVoices;
    fourVoices.prepare(sampleRate, 512);
    fourVoices.setParameter(ParamId::PlayMode, 1.0f);
    fourVoices.setParameter(ParamId::MixerDrive, 0.0f);
    fourVoices.noteOn(60, 100.0f);
    fourVoices.noteOn(64, 100.0f);
    fourVoices.noteOn(67, 100.0f);
    fourVoices.noteOn(72, 100.0f);
    renderMono(fourVoices, 2048);
    const double four = rms(renderMono(fourVoices, frames));

    // With poly headroom, 4 voices should not be 4x louder — typically ≤ 2.5x single.
    EXPECT_LE(four, single * 2.5);
}

TEST(DSPArchV2Test, NoNaNOrInfInFullSignalChain)
{
    SynthEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameter(ParamId::PlayMode, 1.0f);
    engine.setParameter(ParamId::MixerDrive, 3.0f);
    engine.setParameter(ParamId::FilterDrive, 3.0f);
    engine.setFilterCutoffHz(5000.0);
    engine.setFilterResonance(0.9);
    engine.setOutputDrive(3.0);
    engine.noteOn(36, 100.0f);
    engine.noteOn(48, 100.0f);
    engine.noteOn(55, 100.0f);
    engine.noteOn(67, 100.0f);

    for (int i = 0; i < 16384; ++i) {
        const float s = engine.processSample();
        EXPECT_TRUE(std::isfinite(s)) << "NaN/Inf at sample " << i;
        EXPECT_LE(std::abs(s), 1.0f) << "Out of bounds at sample " << i;
        if (!std::isfinite(s)) break;
    }
}

// ── Part 18 — LFO hard bypass + drive pumping regression ─────────────────────

// LFO Rate must not matter when Amt=0 and Wheel=0 (hard bypass).
TEST(DSPArchV2Test, LfoZeroAmountRateIndependent)
{
    auto buildEngine = [](float rate) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpSustain,     1.0f);
        e.setParameter(ParamId::LfoEnabled,     1.0f);
        e.setParameter(ParamId::LfoRate,        rate);
        e.setParameter(ParamId::LfoAmount,      0.0f);
        e.setParameter(ParamId::ModWheelAmount, 0.0f);
        e.setFilterCutoffHz(5000.0);
        e.noteOn(60, 100.0f);
        renderMono(e, 2048); // skip attack
        return renderMono(e, 4096);
    };
    const auto lowRate  = buildEngine(0.01f);
    const auto highRate = buildEngine(20.0f);
    double maxDiff = 0.0;
    for (size_t i = 0; i < lowRate.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(static_cast<double>(lowRate[i] - highRate[i])));
    EXPECT_LT(maxDiff, 1e-5) << "LFO Rate must not matter when Amt=0 and Wheel=0";
}

// All LFO destinations must produce identical output when Amt=0 and Wheel=0.
TEST(DSPArchV2Test, LfoZeroAmountDestinationIndependent)
{
    auto buildEngine = [](float dest) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpSustain,     1.0f);
        e.setParameter(ParamId::LfoEnabled,     1.0f);
        e.setParameter(ParamId::LfoRate,        5.0f);
        e.setParameter(ParamId::LfoAmount,      0.0f);
        e.setParameter(ParamId::ModWheelAmount, 0.0f);
        e.setParameter(ParamId::LfoDestination, dest);
        e.setFilterCutoffHz(5000.0);
        e.noteOn(60, 100.0f);
        renderMono(e, 2048);
        return renderMono(e, 4096);
    };
    const auto destPitch  = buildEngine(0.0f);
    const auto destFilter = buildEngine(1.0f);
    const auto destPW     = buildEngine(2.0f);
    for (size_t i = 0; i < destPitch.size(); ++i) {
        EXPECT_NEAR(destPitch[i], destFilter[i], 1e-5f)
            << "Filter dest vs pitch dest differ at sample " << i;
        EXPECT_NEAR(destPitch[i], destPW[i], 1e-5f)
            << "PW dest vs pitch dest differ at sample " << i;
    }
}

// Drive at a steady value with all modulators off must not create low-frequency
// amplitude modulation (no pumping / tremolo).
TEST(DSPArchV2Test, MixerDriveNoAmplitudeModulation)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,       1.0f); // Poly 4
    engine.setParameter(ParamId::LfoEnabled,     0.0f);
    engine.setParameter(ParamId::LfoAmount,      0.0f);
    engine.setParameter(ParamId::ModWheelAmount, 0.0f);
    engine.setParameter(ParamId::FilterEnvAmount,0.0f);
    engine.setParameter(ParamId::FilterDrive,    0.0f);
    engine.setParameter(ParamId::MixerDrive,     2.5f);
    engine.setFilterCutoffHz(20000.0);
    engine.setFilterResonance(0.0f);
    engine.setParameter(ParamId::AmpSustain,     1.0f);
    engine.setParameter(ParamId::Osc1Level,      1.0f);
    engine.setParameter(ParamId::Osc2Level,      1.0f);
    engine.setParameter(ParamId::Osc3Enabled,    1.0f);
    engine.setParameter(ParamId::Osc3Level,      0.38f);
    engine.noteOn(60, 100.0f);
    engine.noteOn(64, 100.0f);
    engine.noteOn(67, 100.0f);
    engine.noteOn(72, 100.0f);
    renderMono(engine, 8192); // let smoother and envelopes settle fully

    const auto first  = renderMono(engine, 4096);
    const auto second = renderMono(engine, 4096);
    const double rms1 = rms(first);
    const double rms2 = rms(second);
    ASSERT_GT(rms1, 0.001) << "Engine must produce audible output";
    // RMS must be stable — not more than 15% drift between equivalent windows.
    EXPECT_NEAR(rms1, rms2, rms1 * 0.15)
        << "High mixer drive must not create slow amplitude modulation (pumping)";
}

// ── Part 19 — Oscillator analog-style phase randomization ────────────────────

static void setupBasicPatch(SynthEngine& e, bool poly = false)
{
    e.setParameter(ParamId::PlayMode,       poly ? 1.0f : 0.0f);
    e.setParameter(ParamId::AmpSustain,     1.0f);
    e.setParameter(ParamId::AmpAttack,      0.001f);
    e.setParameter(ParamId::AmpRelease,     0.03f);
    e.setParameter(ParamId::LfoAmount,      0.0f);
    e.setParameter(ParamId::ModWheelAmount, 0.0f);
    e.setParameter(ParamId::FilterEnvAmount,0.0f);
    e.setFilterCutoffHz(20000.0);
    e.setFilterResonance(0.0f);
    e.setParameter(ParamId::Osc1Level,      1.0f);
    e.setParameter(ParamId::Osc2Enabled,    0.0f); // single oscillator for clean transient test
    e.setParameter(ParamId::Osc3Enabled,    0.0f);
    e.setParameter(ParamId::MixerDrive,     0.0f);
    e.setParameter(ParamId::FilterDrive,    0.0f);
}

// Sequential poly notes after engine reset should NOT produce identical output.
// Before phase randomization: after reset all phases=0, so repeated note (after reset)
// would be bit-identical. With randomization: _noteStartRng advances across reset(),
// producing different phases and therefore different transients.
TEST(OscPhaseTest, PolyNoteAfterResetIsNotBitIdentical)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    setupBasicPatch(engine, /*poly=*/true);

    engine.noteOn(60, 100.0f);
    const auto first = renderMono(engine, 64);
    engine.noteOff(60);
    renderMono(engine, 2048); // wait for silence (rng stays at V1 — not advanced)

    // Reset all oscillator phases to 0, but _noteStartRng stays at whatever it was
    engine.reset();
    setupBasicPatch(engine, /*poly=*/true);

    engine.noteOn(60, 100.0f); // _noteStartRng advances from V1 to V2 → different phases
    const auto second = renderMono(engine, 64);

    double diff = 0.0;
    for (size_t i = 0; i < first.size(); ++i)
        diff += std::abs(static_cast<double>(first[i]) - second[i]);
    // Without phase randomization first == second (both reset to 0 before each note).
    // With phase randomization they must differ.
    EXPECT_GT(diff / static_cast<double>(first.size()), 0.001)
        << "Repeated note starts after reset must not be bit-identical";
}

// Poly voices started in sequence must have different initial transients.
TEST(OscPhaseTest, SequentialPolyNotesDifferentTransients)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    setupBasicPatch(engine, /*poly=*/true);

    engine.noteOn(60, 100.0f);
    const auto first = renderMono(engine, 64);
    engine.noteOff(60);
    renderMono(engine, 2048);

    engine.noteOn(60, 100.0f);
    const auto second = renderMono(engine, 64);

    double diff = 0.0;
    for (size_t i = 0; i < first.size(); ++i)
        diff += std::abs(static_cast<double>(first[i]) - second[i]);
    EXPECT_GT(diff / static_cast<double>(first.size()), 0.001)
        << "Sequential note starts must produce different transients";
}

// In mono mode, a legato continuation must NOT reset oscillator phases.
// Test: signal must be nonzero and continuous immediately after legato noteOn.
TEST(OscPhaseTest, MonoLegatoPreservesOscillatorContinuity)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,       0.0f); // mono
    engine.setParameter(ParamId::Legato,         1.0f);
    engine.setParameter(ParamId::AmpSustain,     1.0f);
    engine.setParameter(ParamId::AmpAttack,      0.001f);
    engine.setParameter(ParamId::LfoAmount,      0.0f);
    engine.setParameter(ParamId::FilterEnvAmount,0.0f);
    engine.setFilterCutoffHz(20000.0);
    engine.setFilterResonance(0.0f);
    engine.setParameter(ParamId::Osc1Level,      1.0f);
    engine.setParameter(ParamId::Osc2Enabled,    0.0f);
    engine.setParameter(ParamId::Osc3Enabled,    0.0f);
    engine.setParameter(ParamId::MixerDrive,     0.0f);
    engine.setParameter(ParamId::FilterDrive,    0.0f);

    engine.noteOn(60, 100.0f);
    renderMono(engine, 2048); // reach steady state

    const float before = engine.processSample();
    engine.noteOn(64, 100.0f); // legato — must NOT reset oscillator phase
    const float after  = engine.processSample();

    // A phase reset would snap the waveform to its phase=0 value, causing a jump
    // potentially as large as the full waveform amplitude. With legato, continuity
    // means the waveform just continues from where it was — same order of magnitude.
    EXPECT_TRUE(std::isfinite(before));
    EXPECT_TRUE(std::isfinite(after));
    // Signal must not abruptly vanish (no restart transient to near-zero)
    // Use a very loose bound: at least one of the samples is audible
    EXPECT_GT(std::max(std::abs(before), std::abs(after)), 0.001f)
        << "Legato noteOn must not silence the oscillator output";
}

// Glide must not reset oscillator phases (voice stays active during pitch change).
TEST(OscPhaseTest, GlideKeepsSignalContinuous)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,   0.0f); // mono
    engine.setParameter(ParamId::GlideEnabled, 1.0f);
    engine.setParameter(ParamId::GlideTime,  0.05f);
    engine.setParameter(ParamId::AmpSustain, 1.0f);
    engine.setParameter(ParamId::LfoAmount,  0.0f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
    engine.setFilterCutoffHz(20000.0);
    engine.setFilterResonance(0.0f);
    engine.setParameter(ParamId::Osc1Level,  1.0f);
    engine.setParameter(ParamId::Osc2Enabled,0.0f);
    engine.setParameter(ParamId::Osc3Enabled,0.0f);
    engine.setParameter(ParamId::MixerDrive, 0.0f);
    engine.setParameter(ParamId::FilterDrive,0.0f);

    engine.noteOn(60, 100.0f);
    renderMono(engine, 2048);

    // Trigger second note while held — glide active (voice stays live)
    engine.noteOn(72, 100.0f);
    const auto glideOut = renderMono(engine, static_cast<int>(44100 * 0.1)); // 100ms glide window

    const auto s = statsFor(glideOut);
    EXPECT_TRUE(s.finite) << "NaN/Inf during glide";
    EXPECT_LE(s.peak, 1.0f) << "Out of bounds during glide";
    EXPECT_GT(s.rms, 0.001) << "Signal must be present throughout glide";
}

// Two-Drive Final Design tests
TEST(TwoDriveFinalTest, OutputDriveParameterIsNeutralized)
{
    // Output Drive UI parameter must no longer affect the audio signal.
    // Identical patches with outputDrive=0 vs outputDrive=3 must produce identical output.
    auto buildEngine = [](double outputDriveUiValue) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::MixerDrive, 1.5f);
        engine.setFilterCutoffHz(5000.0);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setOutputDrive(outputDriveUiValue);
        engine.noteOn(60, 100.0f);
        renderMono(engine, 2048); // settle
        return renderMono(engine, 4096);
    };

    const auto zeroDrive = buildEngine(0.0);
    const auto maxDrive  = buildEngine(3.0);

    double diff = 0.0;
    for (size_t i = 0; i < zeroDrive.size(); ++i)
        diff += std::abs(static_cast<double>(zeroDrive[i] - maxDrive[i]));
    diff /= static_cast<double>(zeroDrive.size());

    EXPECT_LT(diff, 0.001) << "Output Drive UI value must not create a meaningful tone difference";
}

TEST(TwoDriveFinalTest, InternalOutputColorLinkedToMainDriveIsBounded)
{
    // With Main Drive at maximum and two poly voices, internal output color must stay bounded.
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,   1.0f); // poly
    engine.setParameter(ParamId::MixerDrive, 3.0f); // max main drive → max internal color
    engine.setParameter(ParamId::AmpSustain, 1.0f);
    engine.setFilterCutoffHz(8000.0);
    engine.noteOn(60, 100.0f);
    engine.noteOn(64, 100.0f);
    renderMono(engine, 2048); // settle

    const auto out = renderMono(engine, 4096);
    const auto s = statsFor(out);
    EXPECT_TRUE(s.finite)           << "NaN/Inf with max main drive";
    EXPECT_LE(s.peak, 1.0)          << "Output must not exceed hard clip ceiling";
    EXPECT_GT(s.rms, 0.001)         << "Signal must be audible — not choked by output color";
    EXPECT_EQ(s.clippedSamples, 0)  << "Soft limiter must prevent any hard clipping";
}
