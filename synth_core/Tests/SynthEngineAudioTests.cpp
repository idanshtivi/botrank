#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
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

TEST(SynthEngineAudioTest, LongRenderFiniteAndResetDeterministic)
{
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
        EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
    }
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
    SynthEngine low;
    low.prepare(44100.0, 512);
    low.setFilterCutoffHz(300.0);
    low.noteOn(45, 100.0f);
    const auto lowOut = renderMono(low, 44100);

    SynthEngine high;
    high.prepare(44100.0, 512);
    high.setFilterCutoffHz(8000.0);
    high.noteOn(45, 100.0f);
    const auto highOut = renderMono(high, 44100);

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
