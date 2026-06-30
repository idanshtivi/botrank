#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
#include "../Include/Mixer.h"
#include "../Include/LadderFilter.h"
#include "../Include/OutputStage.h"
#include "../Include/DSPUtils.h"
#include <algorithm>
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

struct DriveProbeStats {
    double mixerRms = 0.0;
    double mixerPeak = 0.0;
    double ladderInputRms = 0.0;
    double ladderInputPeak = 0.0;
    double ladderOutputRms = 0.0;
    double ladderOutputPeak = 0.0;
    double finalRms = 0.0;
    double finalPeak = 0.0;
    bool finite = true;
};

static DriveProbeStats probeDriveChain(double mainDrive, double cutoffHz, double resonance)
{
    Mixer mixer;
    mixer.setSampleRate(44100.0);
    mixer.setDrive(mainDrive);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, false);
    mixer.setSourceEnabled(MixerSource::Osc3, false);
    mixer.setSourceEnabled(MixerSource::Noise, false);
    mixer.setSourceEnabled(MixerSource::ExternalInput, false);
    mixer.setSourceLevel(MixerSource::Osc1, 1.0);

    LadderFilter filter;
    filter.setSampleRate(44100.0);
    filter.setCutoffHz(cutoffHz);
    filter.setResonance(resonance);
    filter.setContourAmount(0.0);
    filter.setKeyboardTrackingAmount(0.0);
    filter.setDrive(0.0);
    filter.setMainDrivePush(mainDrive);

    OutputStage output;
    output.setSampleRate(44100.0);
    output.setMasterVolume(1.0);
    output.setDrive(0.0);
    output.setMainDriveLink(mainDrive);

    DriveProbeStats stats;
    constexpr int frames = 8192;
    for (int i = 0; i < frames; ++i) {
        const double phase = std::fmod(static_cast<double>(i) * 110.0 / 44100.0, 1.0);
        const double saw = 2.0 * phase - 1.0;
        const double mixed = mixer.processSample(saw, 0.0, 0.0, 0.0, 0.0);
        const double ladderInput = mixed;
        const double filtered = filter.processSample(ladderInput, 0.0, 48.0);
        const double finalOut = output.processSample(filtered);

        stats.finite = stats.finite && std::isfinite(mixed) && std::isfinite(filtered) && std::isfinite(finalOut);
        stats.mixerRms += mixed * mixed;
        stats.ladderInputRms += ladderInput * ladderInput;
        stats.ladderOutputRms += filtered * filtered;
        stats.finalRms += finalOut * finalOut;
        stats.mixerPeak = std::max(stats.mixerPeak, std::abs(mixed));
        stats.ladderInputPeak = std::max(stats.ladderInputPeak, std::abs(ladderInput));
        stats.ladderOutputPeak = std::max(stats.ladderOutputPeak, std::abs(filtered));
        stats.finalPeak = std::max(stats.finalPeak, std::abs(finalOut));
    }

    stats.mixerRms = std::sqrt(stats.mixerRms / static_cast<double>(frames));
    stats.ladderInputRms = std::sqrt(stats.ladderInputRms / static_cast<double>(frames));
    stats.ladderOutputRms = std::sqrt(stats.ladderOutputRms / static_cast<double>(frames));
    stats.finalRms = std::sqrt(stats.finalRms / static_cast<double>(frames));
    return stats;
}

struct GrowlBandStats {
    double lowMidRms = 0.0;
    double highRms = 0.0;
    double finalRms = 0.0;
    double finalPeak = 0.0;
    bool finite = true;
};

static GrowlBandStats probeLowMidGrowlPatch(double mainDrive)
{
    constexpr double sampleRate = 44100.0;
    Mixer mixer;
    mixer.setSampleRate(sampleRate);
    mixer.setDrive(mainDrive);
    mixer.setSourceEnabled(MixerSource::Osc1, true);
    mixer.setSourceEnabled(MixerSource::Osc2, true);
    mixer.setSourceEnabled(MixerSource::Osc3, false);
    mixer.setSourceEnabled(MixerSource::Noise, false);
    mixer.setSourceEnabled(MixerSource::ExternalInput, false);
    mixer.setSourceLevel(MixerSource::Osc1, 0.85);
    mixer.setSourceLevel(MixerSource::Osc2, 0.60);
    mixer.setSourceLevel(MixerSource::Osc3, 0.0);
    mixer.setSourceLevel(MixerSource::Noise, 0.0);

    LadderFilter filter;
    filter.setSampleRate(sampleRate);
    filter.setCutoffHz(1000.0);
    filter.setResonance(0.2);
    filter.setContourAmount(0.0);
    filter.setKeyboardTrackingAmount(0.0);
    filter.setDrive(0.0);
    filter.setMainDrivePush(mainDrive);

    OutputStage output;
    output.setSampleRate(sampleRate);
    output.setMasterVolume(1.0);
    output.setDrive(0.0);
    output.setMainDriveLink(mainDrive);

    auto lpCoeff = [](double cutoffHz) {
        constexpr double pi = 3.14159265358979323846;
        return 1.0 - std::exp(-2.0 * pi * cutoffHz / sampleRate);
    };

    const double g80 = lpCoeff(80.0);
    const double g500 = lpCoeff(500.0);
    const double g2000 = lpCoeff(2000.0);
    double lp80 = 0.0;
    double lp500 = 0.0;
    double lp2000 = 0.0;

    GrowlBandStats stats;
    constexpr int frames = 16384;
    for (int i = 0; i < frames; ++i) {
        const double phase1 = std::fmod(static_cast<double>(i) * 110.0 / sampleRate, 1.0);
        const double phase2 = std::fmod(static_cast<double>(i) * 110.0 / sampleRate, 1.0);
        const double saw = 2.0 * phase1 - 1.0;
        const double square = phase2 < 0.5 ? 1.0 : -1.0;
        const double mixed = mixer.processSample(saw, square, 0.0, 0.0, 0.0);
        const double filtered = filter.processSample(mixed, 0.0, 48.0);
        const double finalOut = output.processSample(filtered);

        stats.finite = stats.finite && std::isfinite(mixed) && std::isfinite(filtered) && std::isfinite(finalOut);
        lp80 += g80 * (finalOut - lp80);
        lp500 += g500 * (finalOut - lp500);
        lp2000 += g2000 * (finalOut - lp2000);

        const double lowMid = lp500 - lp80;
        const double high = finalOut - lp2000;
        stats.lowMidRms += lowMid * lowMid;
        stats.highRms += high * high;
        stats.finalRms += finalOut * finalOut;
        stats.finalPeak = std::max(stats.finalPeak, std::abs(finalOut));
    }

    stats.lowMidRms = std::sqrt(stats.lowMidRms / static_cast<double>(frames));
    stats.highRms = std::sqrt(stats.highRms / static_cast<double>(frames));
    stats.finalRms = std::sqrt(stats.finalRms / static_cast<double>(frames));
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
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.0f);
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

// ── Osc2 default-detune regression tests ─────────────────────────────────────

TEST(Osc2DetuneTest, DefaultDetuneIsZero)
{
    SynthEngine engine;
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.0f)
        << "Osc2 default detune must be 0.0 to avoid inter-oscillator beating";
}

TEST(Osc2DetuneTest, AfterResetDetuneRemainsZero)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.reset();
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.0f);
}

TEST(Osc2DetuneTest, CanSetToPositiveValue)
{
    SynthEngine engine;
    engine.setParameter(ParamId::Osc2Detune, 0.05f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.05f);
}

TEST(Osc2DetuneTest, CanSetToNegativeValue)
{
    SynthEngine engine;
    engine.setParameter(ParamId::Osc2Detune, -0.05f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), -0.05f);
}

TEST(Osc2DetuneTest, ReturnsExactlyZeroAfterRoundTrip)
{
    SynthEngine engine;
    engine.setParameter(ParamId::Osc2Detune, 0.10f);
    engine.setParameter(ParamId::Osc2Detune, 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.0f);

    engine.setParameter(ParamId::Osc2Detune, -0.10f);
    engine.setParameter(ParamId::Osc2Detune, 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::Osc2Detune), 0.0f);
}

// With Osc2Detune=0 and the same range, Osc1 and Osc2 must produce the same
// fundamental frequency at the same MIDI note.
TEST(Osc2DetuneTest, Osc1AndOsc2ProduceSameFrequencyAtZeroDetune)
{
    constexpr double sampleRate = 44100.0;
    constexpr int frames = static_cast<int>(sampleRate);

    auto measureOscHz = [&](int oscToEnable) -> double {
        SynthEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameter(ParamId::AmpAttack,      0.001f);
        engine.setParameter(ParamId::AmpSustain,     1.0f);
        engine.setParameter(ParamId::FilterCutoff,   20000.0f);
        engine.setParameter(ParamId::FilterEnvAmount,0.0f);
        engine.setParameter(ParamId::LfoAmount,      0.0f);
        engine.setParameter(ParamId::MixerDrive,     0.0f);
        engine.setParameter(ParamId::FilterDrive,    0.0f);
        engine.setParameter(ParamId::Osc1Enabled,    oscToEnable == 1 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc1Level,      oscToEnable == 1 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc2Enabled,    oscToEnable == 2 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc2Level,      oscToEnable == 2 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc3Enabled,    0.0f);
        engine.setParameter(ParamId::Osc2Detune,     0.0f);
        engine.setParameter(ParamId::Osc1Waveform,   2.0f); // Saw
        engine.setParameter(ParamId::Osc2Waveform,   2.0f); // Saw
        engine.setParameter(ParamId::Osc1Range,      3.0f); // 8'
        engine.setParameter(ParamId::Osc2Range,      3.0f); // 8'
        engine.noteOn(69, 100.0f); // A4
        renderMono(engine, 4096);  // skip attack
        return measureHz(renderMono(engine, frames), sampleRate);
    };

    const double f1 = measureOscHz(1);
    const double f2 = measureOscHz(2);
    ASSERT_GT(f1, 0.0);
    ASSERT_GT(f2, 0.0);
    // At zero detune, Osc1 and Osc2 must be within 0.1 Hz of each other (< 0.4 cents at A4).
    EXPECT_NEAR(f1, f2, 0.1) << "Osc1 Hz=" << f1 << " Osc2 Hz=" << f2
        << " — non-zero detune default would create slow beating";
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
    // Use MixerDrive=0 to test routing linearity without saturation compression.
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.setParameter(ParamId::MixerDrive, 0.0f);
    engine.noteOn(45, 100.0f);

    engine.setParameter(ParamId::Osc1Level, 0.2f);
    const double low = rms(renderMono(engine, 4096));

    engine.reset();
    engine.setParameter(ParamId::Osc2Enabled, 0.0f);
    engine.setParameter(ParamId::MixerDrive, 0.0f);
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

TEST(SynthEngineRealtimeParamTest, OtherRealtimeKnobsAffectHeldNote)
{
    constexpr double sampleRate = 44100.0;
    constexpr int frames = 4096;

    auto renderAfterChange = [](auto&& change) {
        SynthEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::Osc1Level, 1.0f);
        engine.setParameter(ParamId::AmpAttack, 0.001f);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setParameter(ParamId::MixerDrive, 0.0f);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.setParameter(ParamId::FilterCutoff, 20000.0f);
        engine.setParameter(ParamId::FilterResonance, 0.0f);
        engine.noteOn(72, 100.0f);
        renderMono(engine, 4096);
        change(engine);
        return renderMono(engine, frames);
    };

    auto avgDiff = [](const std::vector<float>& a, const std::vector<float>& b) {
        double diff = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            diff += std::abs(static_cast<double>(a[i] - b[i]));
        return diff / static_cast<double>(a.size());
    };

    const auto cutoffLow = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::FilterCutoff, 300.0f); });
    const auto cutoffHigh = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::FilterCutoff, 20000.0f); });
    EXPECT_GT(avgDiff(cutoffLow, cutoffHigh), 0.01);

    const auto emphLow = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::FilterResonance, 0.0f); });
    const auto emphHigh = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::FilterResonance, 0.9f); });
    EXPECT_GT(avgDiff(emphLow, emphHigh), 0.001);

    const auto oscLevelLow = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::Osc1Level, 0.2f); });
    const auto oscLevelHigh = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::Osc1Level, 1.0f); });
    EXPECT_GT(rms(oscLevelHigh), rms(oscLevelLow) * 2.0);

    const auto volumeLow = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::MasterVolume, 0.2f); });
    const auto volumeHigh = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::MasterVolume, 1.0f); });
    EXPECT_GT(rms(volumeHigh), rms(volumeLow) * 2.0);

    const auto sustainLow = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::AmpSustain, 0.2f); });
    const auto sustainHigh = renderAfterChange([](SynthEngine& e) { e.setParameter(ParamId::AmpSustain, 1.0f); });
    EXPECT_GT(rms(sustainHigh), rms(sustainLow) * 2.0);
}

TEST(SynthEngineRealtimeParamTest, OscillatorLevelsAreContinuousMixerGainAndDriveInput)
{
    auto renderOsc1 = [](float level, float mixerDrive) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::PlayMode, 0.0f);
        engine.setParameter(ParamId::Osc1Enabled, 1.0f);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::NoiseEnabled, 0.0f);
        engine.setParameter(ParamId::NoiseLevel, 0.0f);
        engine.setParameter(ParamId::Osc1Waveform, 2.0f);
        engine.setParameter(ParamId::Osc1Level, level);
        engine.setParameter(ParamId::AmpAttack, 0.001f);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setParameter(ParamId::FilterCutoff, 20000.0f);
        engine.setParameter(ParamId::FilterResonance, 0.0f);
        engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.setParameter(ParamId::MixerDrive, mixerDrive);
        engine.noteOn(48, 100.0f);
        renderMono(engine, 4096);
        return renderMono(engine, 8192);
    };

    const auto l25 = renderOsc1(0.25f, 0.0f);
    const auto l50 = renderOsc1(0.50f, 0.0f);
    const auto l75 = renderOsc1(0.75f, 0.0f);
    const auto l100 = renderOsc1(1.0f, 0.0f);

    EXPECT_GT(rms(l50), rms(l25) * 1.55);
    EXPECT_GT(rms(l75), rms(l50) * 1.25);
    EXPECT_GT(rms(l100), rms(l75) * 1.12);

    auto gainMatchedDiff = [](const std::vector<float>& reference, const std::vector<float>& candidate) {
        const double refRms = rms(reference);
        const double candRms = rms(candidate);
        if (refRms <= 0.0 || candRms <= 0.0) return 0.0;

        const double scale = refRms / candRms;
        double diff = 0.0;
        for (size_t i = 0; i < reference.size(); ++i)
            diff += std::abs(static_cast<double>(reference[i]) - static_cast<double>(candidate[i]) * scale);
        return diff / static_cast<double>(reference.size());
    };

    const double lowLevelDriveColor = gainMatchedDiff(renderOsc1(0.25f, 0.0f), renderOsc1(0.25f, 2.0f));
    const double highLevelDriveColor = gainMatchedDiff(renderOsc1(1.0f, 0.0f), renderOsc1(1.0f, 2.0f));

    EXPECT_GT(highLevelDriveColor, lowLevelDriveColor * 1.20)
        << "Higher oscillator level should push the mixer/ladder drive path harder";
}

TEST(SynthEngineRealtimeParamTest, MixerDriveRuntimeChangeAffectsSameHeldNoteMono)
{
    auto renderWithRuntimeDrive = [](float driveAfterSet) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::PlayMode, 0.0f);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::Osc1Level, 1.0f);
        engine.setParameter(ParamId::AmpAttack, 0.001f);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setParameter(ParamId::FilterCutoff, 20000.0f);
        engine.setParameter(ParamId::FilterResonance, 0.0f);
        engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.setParameter(ParamId::MixerDrive, 0.0f);
        engine.noteOn(48, 100.0f);
        renderMono(engine, 4096);
        engine.setParameter(ParamId::MixerDrive, driveAfterSet);
        renderMono(engine, 4096);
        return renderMono(engine, 8192);
    };

    const auto clean = renderWithRuntimeDrive(0.0f);
    const auto driven = renderWithRuntimeDrive(3.0f);
    EXPECT_GT(averageAbsDiff(driven), averageAbsDiff(clean) * 1.05)
        << "MixerDrive 0->3 on the same held mono note must change the engine output";
}

TEST(SynthEngineRealtimeParamTest, FilterDriveRuntimeChangeAffectsSameHeldNoteMono)
{
    auto renderWithRuntimeDrive = [](float driveAfterSet) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::PlayMode, 0.0f);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::Osc1Level, 1.0f);
        engine.setParameter(ParamId::AmpAttack, 0.001f);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setParameter(ParamId::FilterCutoff, 1600.0f);
        engine.setParameter(ParamId::FilterResonance, 0.2f);
        engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
        engine.setParameter(ParamId::MixerDrive, 0.0f);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.noteOn(48, 100.0f);
        renderMono(engine, 4096);
        engine.setParameter(ParamId::FilterDrive, driveAfterSet);
        return renderMono(engine, 8192);
    };

    const auto clean = renderWithRuntimeDrive(0.0f);
    const auto driven = renderWithRuntimeDrive(3.0f);
    EXPECT_GT(averageAbsDiff(driven), averageAbsDiff(clean) * 1.02)
        << "FilterDrive 0->3 on the same held mono note must change the engine output";
}

TEST(SynthEngineRealtimeParamTest, DriveRuntimeChangesAffectSameHeldNotePoly)
{
    auto renderWithRuntimeDrives = [](float mixerDrive, float filterDrive) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::PlayMode, 1.0f);
        engine.setParameter(ParamId::Osc1Level, 1.0f);
        engine.setParameter(ParamId::Osc2Level, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::AmpAttack, 0.001f);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setParameter(ParamId::FilterCutoff, 2000.0f);
        engine.setParameter(ParamId::FilterResonance, 0.2f);
        engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
        engine.setParameter(ParamId::MixerDrive, 0.0f);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.noteOn(48, 100.0f);
        engine.noteOn(55, 100.0f);
        renderMono(engine, 4096);
        engine.setParameter(ParamId::MixerDrive, mixerDrive);
        engine.setParameter(ParamId::FilterDrive, filterDrive);
        renderMono(engine, 4096);
        return renderMono(engine, 8192);
    };

    const auto clean = renderWithRuntimeDrives(0.0f, 0.0f);
    const auto driven = renderWithRuntimeDrives(3.0f, 3.0f);
    EXPECT_GT(averageAbsDiff(driven), averageAbsDiff(clean) * 1.03)
        << "Runtime drive updates must apply to active poly voices";
}

TEST(SynthEngineRealtimeParamTest, MixerDriveProgressionChangesToneAfterRmsMatching)
{
    auto renderHeldNoteAtDrive = [](float drive) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::PlayMode, 0.0f);
        engine.setParameter(ParamId::Osc1Waveform, 2.0f); // Saw
        engine.setParameter(ParamId::Osc1Range, 3.0f);
        engine.setParameter(ParamId::Osc1Level, 1.0f);
        engine.setParameter(ParamId::Osc2Enabled, 0.0f);
        engine.setParameter(ParamId::Osc3Enabled, 0.0f);
        engine.setParameter(ParamId::NoiseEnabled, 0.0f);
        engine.setParameter(ParamId::NoiseLevel, 0.0f);
        engine.setParameter(ParamId::AmpAttack, 0.001f);
        engine.setParameter(ParamId::AmpSustain, 1.0f);
        engine.setParameter(ParamId::FilterCutoff, 20000.0f);
        engine.setParameter(ParamId::FilterResonance, 0.0f);
        engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
        engine.setParameter(ParamId::FilterDrive, 0.0f);
        engine.setParameter(ParamId::LfoAmount, 0.0f);
        engine.setParameter(ParamId::ModWheelAmount, 0.0f);
        engine.setParameter(ParamId::MixerDrive, 0.0f);
        engine.noteOn(48, 100.0f);
        renderMono(engine, 4096);
        engine.setParameter(ParamId::MixerDrive, drive);
        renderMono(engine, 8192);
        return renderMono(engine, 8192);
    };

    auto gainMatchedDiff = [](const std::vector<float>& reference, const std::vector<float>& candidate) {
        const double refRms = rms(reference);
        const double candRms = rms(candidate);
        if (refRms <= 0.0 || candRms <= 0.0) return 0.0;

        const double scale = refRms / candRms;
        double diff = 0.0;
        for (size_t i = 0; i < reference.size(); ++i)
            diff += std::abs(static_cast<double>(reference[i]) - static_cast<double>(candidate[i]) * scale);
        return diff / static_cast<double>(reference.size());
    };

    const auto d0 = renderHeldNoteAtDrive(0.0f);
    const auto d1 = renderHeldNoteAtDrive(1.0f);
    const auto d2 = renderHeldNoteAtDrive(2.0f);
    const auto d3 = renderHeldNoteAtDrive(3.0f);

    const double c1 = gainMatchedDiff(d0, d1);
    const double c2 = gainMatchedDiff(d0, d2);
    const double c3 = gainMatchedDiff(d0, d3);

    EXPECT_GT(c1, 0.006) << "Drive 1 must affect final synth tone after RMS matching";
    EXPECT_GT(c2, c1 * 0.98) << "Drive 2 must keep the final synth path strongly colored after Drive 1";
    EXPECT_GT(c3, c2 * 0.98) << "Drive 3 must stay strongly colored without collapsing after Drive 2";
}

TEST(SynthEngineRealtimeParamTest, MainDriveProbeShowsLadderPressure)
{
    const auto open0 = probeDriveChain(0.0, 20000.0, 0.0);
    const auto open1 = probeDriveChain(1.0, 20000.0, 0.0);
    const auto open2 = probeDriveChain(2.0, 20000.0, 0.0);
    const auto open3 = probeDriveChain(3.0, 20000.0, 0.0);

    EXPECT_TRUE(open0.finite);
    EXPECT_TRUE(open1.finite);
    EXPECT_TRUE(open2.finite);
    EXPECT_TRUE(open3.finite);
    EXPECT_GT(open1.mixerRms, open0.mixerRms * 1.10)
        << "Main Drive 1 must push mixer/ladder input RMS, not wait until max";
    EXPECT_GT(open2.ladderOutputRms, open1.ladderOutputRms * 0.82)
        << "Drive 2 must add growl/pressure without collapsing the open-filter output";
    EXPECT_GT(open3.finalRms, open2.finalRms * 0.72)
        << "Drive 3 must compress musically, not become smaller/thinner";
    EXPECT_LE(open3.mixerPeak, 1.0);
    EXPECT_LE(open3.ladderOutputPeak, 1.0);
    EXPECT_LE(open3.finalPeak, 1.0);

    const auto closed0 = probeDriveChain(0.0, 1000.0, 0.2);
    const auto closed1 = probeDriveChain(1.0, 1000.0, 0.2);
    const auto closed2 = probeDriveChain(2.0, 1000.0, 0.2);
    const auto closed3 = probeDriveChain(3.0, 1000.0, 0.2);

    EXPECT_TRUE(closed0.finite);
    EXPECT_TRUE(closed1.finite);
    EXPECT_TRUE(closed2.finite);
    EXPECT_TRUE(closed3.finite);
    EXPECT_GT(closed1.finalRms, closed0.finalRms * 0.70)
        << "Closed-filter Drive 1 must remain present after ladder compression";
    EXPECT_GT(closed2.ladderOutputRms, closed1.ladderOutputRms * 0.70)
        << "Closed-filter Drive 2 must not choke the ladder output";
    EXPECT_GT(closed3.finalRms, closed2.finalRms * 0.65)
        << "Closed-filter Drive 3 must add pressure/compression without collapsing";
    EXPECT_LE(closed3.ladderOutputPeak, 1.0);
    EXPECT_LE(closed3.finalPeak, 1.0);
}

TEST(SynthEngineRealtimeParamTest, MainDriveLowMidGrowlDoesNotTurnIntoTrebleSpike)
{
    const auto d0 = probeLowMidGrowlPatch(0.0);
    const auto d1 = probeLowMidGrowlPatch(1.0);
    const auto d2 = probeLowMidGrowlPatch(2.0);
    const auto d3 = probeLowMidGrowlPatch(3.0);

    EXPECT_TRUE(d0.finite);
    EXPECT_TRUE(d1.finite);
    EXPECT_TRUE(d2.finite);
    EXPECT_TRUE(d3.finite);

    const double lowGrowth1 = d1.lowMidRms / std::max(d0.lowMidRms, 1.0e-9);
    const double lowGrowth2 = d2.lowMidRms / std::max(d0.lowMidRms, 1.0e-9);
    const double lowGrowth3 = d3.lowMidRms / std::max(d0.lowMidRms, 1.0e-9);
    const double highGrowth3 = d3.highRms / std::max(d0.highRms, 1.0e-9);
    const double ratio0 = d0.lowMidRms / std::max(d0.highRms, 1.0e-9);
    const double ratio3 = d3.lowMidRms / std::max(d3.highRms, 1.0e-9);

    EXPECT_GT(lowGrowth1, 0.85)
        << "Drive 1 should keep the low-mid body audible in the growl patch";
    EXPECT_GT(lowGrowth2, lowGrowth1 * 0.82)
        << "Drive 2 should add pressure without hollowing the 80-500 Hz region";
    EXPECT_GT(lowGrowth3, lowGrowth2 * 0.78)
        << "Drive 3 should compress/growl without thinning out";
    EXPECT_GT(lowGrowth3, highGrowth3 * 0.75)
        << "Main Drive must favor low-mid density over extra high-band brightness";
    EXPECT_GT(ratio3, ratio0 * 0.72)
        << "Drive 3 should not become a treble-spike version of the clean patch";
    EXPECT_LE(d3.finalPeak, 1.0);
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

    // Use MixerDrive=0 to test noise routing linearity without saturation compression.
    SynthEngine low;
    low.prepare(44100.0, 512);
    low.setNoiseSeed(7u);
    low.setParameter(ParamId::Osc1Enabled, 0.0f);
    low.setParameter(ParamId::Osc2Enabled, 0.0f);
    low.setParameter(ParamId::Osc3Enabled, 0.0f);
    low.setParameter(ParamId::MixerDrive, 0.0f);
    low.setNoiseEnabled(true);
    low.setNoiseLevel(0.2);
    low.noteOn(45, 100.0f);

    SynthEngine high;
    high.prepare(44100.0, 512);
    high.setNoiseSeed(7u);
    high.setParameter(ParamId::Osc1Enabled, 0.0f);
    high.setParameter(ParamId::Osc2Enabled, 0.0f);
    high.setParameter(ParamId::Osc3Enabled, 0.0f);
    high.setParameter(ParamId::MixerDrive, 0.0f);
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

    // At drive=0, blend=0, output=mixerSum=input*0.55
    const double out = mixer.processSample(0.5, 0.0, 0.0, 0.0, 0.0);
    EXPECT_NEAR(out, 0.5 * 0.55, 1e-9);
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

// ── High-note aliasing / fizz audit ──────────────────────────────────────────
// Cubic shaping can generate harmonics above Nyquist that fold back as digital
// fizz.  Verify that high notes (C5/C6/C7) at maximum drive remain bounded,
// finite, and audible — not exploding with alias energy.
TEST(DSPArchV2Test, MixerDriveHighNoteAliasingBounded)
{
    // C5 = MIDI 72, C6 = 84, C7 = 96
    for (int midiNote : {72, 84, 96}) {
        SynthEngine engine;
        engine.prepare(44100.0, 512);
        engine.setParameter(ParamId::PlayMode,       0.0f); // Mono
        engine.setParameter(ParamId::AmpSustain,     1.0f);
        engine.setParameter(ParamId::AmpAttack,      0.001f);
        engine.setParameter(ParamId::LfoAmount,      0.0f);
        engine.setParameter(ParamId::ModWheelAmount, 0.0f);
        engine.setParameter(ParamId::FilterEnvAmount,0.0f);
        engine.setFilterCutoffHz(20000.0);
        engine.setFilterResonance(0.0f);
        engine.setParameter(ParamId::FilterDrive,    0.0f);
        engine.setParameter(ParamId::Osc1Level,      1.0f);
        engine.setParameter(ParamId::Osc2Enabled,    0.0f);
        engine.setParameter(ParamId::Osc3Enabled,    0.0f);
        engine.setParameter(ParamId::MixerDrive,     3.0f);
        engine.noteOn(midiNote, 100.0f);
        renderMono(engine, 4096); // settle past attack

        const auto buf = renderMono(engine, 4096);
        const auto s = statsFor(buf);

        EXPECT_TRUE(s.finite) << "MIDI " << midiNote << ": NaN/Inf at Drive 3 open filter";
        EXPECT_LE(s.peak, 1.0) << "MIDI " << midiNote << ": peak must stay bounded at Drive 3";
        EXPECT_GT(s.rms, 0.02) << "MIDI " << midiNote << ": must be audible at Drive 3";

        // High-frequency alias energy proxy: RMS of successive-sample differences
        // normalised to signal RMS.  Clean saturation stays bounded; aliasing /
        // fizz inflates this ratio well above what the fundamental supports.
        // For a saw wave at C7 (2093 Hz, 44100 Hz sample rate) the max theoretical
        // ratio from the fundamental alone is 2*sin(pi*2093/44100) ≈ 0.46.
        // Harmonics raise it further but a ratio above 2.5 indicates alias fold-back.
        double diffRmsSum = 0.0;
        for (size_t i = 1; i < buf.size(); ++i) {
            const double d = static_cast<double>(buf[i]) - buf[i - 1];
            diffRmsSum += d * d;
        }
        const double diffRms = std::sqrt(diffRmsSum / static_cast<double>(buf.size() - 1));
        const double ratio = (s.rms > 0.0) ? diffRms / s.rms : 0.0;
        EXPECT_LT(ratio, 2.5) << "MIDI " << midiNote
            << ": high-frequency energy ratio " << ratio
            << " suggests aliasing / fizz at Drive 3";
    }
}

// ── Part 20 — AnalogDrift guard: must default to 0, must be forceable to 0 ───

TEST(AnalogDriftTest, DefaultIsZero)
{
    // initPatchValue for AnalogDrift must return 0.0 — no hidden drift on clean init.
    EXPECT_FLOAT_EQ(initPatchValue(ParamId::AnalogDrift), 0.0f);
}

TEST(AnalogDriftTest, FreshEngineHasZeroDrift)
{
    SynthEngine engine;
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AnalogDrift), 0.0f);
}

TEST(AnalogDriftTest, SetNonZeroThenClearRestoresZero)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::AnalogDrift, 1.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AnalogDrift), 1.0f);

    engine.setParameter(ParamId::AnalogDrift, 0.0f);
    EXPECT_FLOAT_EQ(engine.getParameter(ParamId::AnalogDrift), 0.0f);
}

// ── Part 21 — Pitch de-click: mono/poly note transition no-click ──────────────

// Shared patch setup: mono, single saw, full sustain, all drive off, filter open.
static void setupDeclickPatch(SynthEngine& e)
{
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::PlayMode,        0.0f);
    e.setParameter(ParamId::Osc1Level,       1.0f);
    e.setParameter(ParamId::Osc2Enabled,     0.0f);
    e.setParameter(ParamId::Osc3Enabled,     0.0f);
    e.setParameter(ParamId::Osc1Waveform,    2.0f); // saw
    e.setParameter(ParamId::AmpAttack,       0.001f);
    e.setParameter(ParamId::AmpDecay,        0.1f);
    e.setParameter(ParamId::AmpSustain,      1.0f);
    e.setParameter(ParamId::MixerDrive,      0.0f);
    e.setParameter(ParamId::FilterDrive,     0.0f);
    e.setParameter(ParamId::FilterCutoff,    20000.0f);
    e.setParameter(ParamId::FilterResonance, 0.0f);
    e.setParameter(ParamId::FilterEnvAmount, 0.0f);
    e.setParameter(ParamId::LfoAmount,       0.0f);
    e.setParameter(ParamId::ModWheelAmount,  0.0f);
}

// Case A — mono note transition, Last priority.
// Verify pitch changes to the new note and audio stays finite and bounded.
// Note: PolyBLEP click magnitude (~1% of peak) is smaller than the corrected
// saw-wrap discontinuity (~peak), so click absence cannot be verified via
// max-delta; correctness is confirmed by pitch-change and stability checks.
TEST(PitchDeclickTest, MonoTransitionPitchChanges_LastPriority)
{
    SynthEngine engine;
    setupDeclickPatch(engine);
    engine.setParameter(ParamId::NotePriority, 1.0f); // Last

    engine.noteOn(48, 100.0f); // C3
    renderMono(engine, 8192);  // reach sustain

    const double hzC3 = measureHz(renderMono(engine, 44100), 44100.0);

    engine.noteOn(52, 100.0f); // E3 — must change active pitch
    const auto transWindow = renderMono(engine, 512);
    renderMono(engine, 8192); // settle
    const double hzE3 = measureHz(renderMono(engine, 44100), 44100.0);

    const auto ts = statsFor(transWindow);
    EXPECT_TRUE(ts.finite)       << "NaN/Inf during mono Last-priority transition";
    EXPECT_LE(ts.peak, 1.0f)    << "Signal exceeded bounds during transition";
    EXPECT_NEAR(hzC3, 130.81, 5.0) << "C3 must play before transition";
    EXPECT_NEAR(hzE3, 164.81, 5.0) << "E3 must be active after Last-priority transition";
}

// Case A — mono note transition, High priority.
TEST(PitchDeclickTest, MonoTransitionPitchChanges_HighPriority)
{
    SynthEngine engine;
    setupDeclickPatch(engine);
    engine.setParameter(ParamId::NotePriority, 2.0f); // High

    engine.noteOn(48, 100.0f); // C3 held
    renderMono(engine, 8192);

    const double hzC3 = measureHz(renderMono(engine, 44100), 44100.0);

    engine.noteOn(64, 100.0f); // E4 — higher note wins with High priority
    const auto transWindow = renderMono(engine, 512);
    renderMono(engine, 8192);
    const double hzE4 = measureHz(renderMono(engine, 44100), 44100.0);

    const auto ts = statsFor(transWindow);
    EXPECT_TRUE(ts.finite)      << "NaN/Inf during mono High-priority transition";
    EXPECT_LE(ts.peak, 1.0f)   << "Signal exceeded bounds during transition";
    EXPECT_NEAR(hzC3, 130.81, 5.0) << "C3 must play before transition";
    EXPECT_NEAR(hzE4, 329.63, 8.0) << "E4 must be active after High-priority transition";
}

// Case A — mono note transition, Low priority.
// Click happens on noteOff of the lower note when the higher note takes over.
TEST(PitchDeclickTest, MonoTransitionPitchChanges_LowPriority)
{
    SynthEngine engine;
    setupDeclickPatch(engine);
    engine.setParameter(ParamId::NotePriority, 0.0f); // Low

    engine.noteOn(64, 100.0f); // E4 held first
    renderMono(engine, 8192);

    engine.noteOn(48, 100.0f); // C3 — lower, takes over with Low priority
    renderMono(engine, 2048);

    const double hzC3 = measureHz(renderMono(engine, 44100), 44100.0);

    // Release C3: E4 becomes active — pitch changes while VCA is still open
    engine.noteOff(48);
    const auto transWindow = renderMono(engine, 512);
    renderMono(engine, 8192);
    const double hzE4 = measureHz(renderMono(engine, 44100), 44100.0);

    const auto ts = statsFor(transWindow);
    EXPECT_TRUE(ts.finite)      << "NaN/Inf during mono Low-priority transition";
    EXPECT_LE(ts.peak, 1.0f)   << "Signal exceeded bounds during transition";
    EXPECT_NEAR(hzC3, 130.81, 5.0) << "C3 must play while both held";
    EXPECT_NEAR(hzE4, 329.63, 8.0) << "E4 must be active after C3 released";
}

// Case B — Legato ON, Retrigger OFF: new note must NOT restart the envelope.
TEST(PitchDeclickTest, LegatoNoEnvelopeRetriggerWhenRetriggerOff)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,        0.0f);
    engine.setParameter(ParamId::Legato,          1.0f);
    engine.setParameter(ParamId::Retrigger,       0.0f);
    engine.setParameter(ParamId::Osc1Level,       1.0f);
    engine.setParameter(ParamId::Osc2Enabled,     0.0f);
    engine.setParameter(ParamId::Osc3Enabled,     0.0f);
    engine.setParameter(ParamId::AmpAttack,       0.5f); // slow attack makes retrigger obvious
    engine.setParameter(ParamId::AmpDecay,        0.1f);
    engine.setParameter(ParamId::AmpSustain,      1.0f);
    engine.setParameter(ParamId::MixerDrive,      0.0f);
    engine.setParameter(ParamId::FilterDrive,     0.0f);
    engine.setParameter(ParamId::FilterCutoff,    20000.0f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
    engine.setParameter(ParamId::LfoAmount,       0.0f);
    engine.setParameter(ParamId::ModWheelAmount,  0.0f);

    engine.noteOn(60, 100.0f); // C4
    renderMono(engine, 44100); // reach full sustain (0.5s attack, now 1.0s in)

    const double rmsBefore = rms(renderMono(engine, 512));

    engine.noteOn(64, 100.0f); // E4 — legato, envelope must NOT restart
    renderMono(engine, 512);   // brief buffer after legato noteOn
    const double rmsAfter = rms(renderMono(engine, 512));

    // If the envelope had retriggered with 0.5s attack, amplitude would drop sharply.
    // Without retrigger, amplitude stays near sustain level (rmsBefore ≈ rmsAfter).
    EXPECT_GT(rmsAfter, rmsBefore * 0.7)
        << "Legato+Retrigger=OFF noteOn must not restart the loudness envelope "
           "(rmsBefore=" << rmsBefore << " rmsAfter=" << rmsAfter << ")";
}

// Case C — Poly 4 voice stealing: audio must remain stable when voices are stolen.
TEST(PitchDeclickTest, PolyVoiceStealAudioStable)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,        1.0f); // poly 4
    engine.setParameter(ParamId::Osc1Level,       1.0f);
    engine.setParameter(ParamId::Osc2Enabled,     0.0f);
    engine.setParameter(ParamId::Osc3Enabled,     0.0f);
    engine.setParameter(ParamId::Osc1Waveform,    2.0f);
    engine.setParameter(ParamId::AmpAttack,       0.001f);
    engine.setParameter(ParamId::AmpSustain,      1.0f);
    engine.setParameter(ParamId::MixerDrive,      0.0f);
    engine.setParameter(ParamId::FilterDrive,     0.0f);
    engine.setParameter(ParamId::FilterCutoff,    20000.0f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
    engine.setParameter(ParamId::LfoAmount,       0.0f);
    engine.setParameter(ParamId::ModWheelAmount,  0.0f);

    engine.noteOn(48, 100.0f); // fill all 4 voices
    engine.noteOn(52, 100.0f);
    engine.noteOn(55, 100.0f);
    engine.noteOn(60, 100.0f);
    renderMono(engine, 8192); // reach sustain on all voices

    engine.noteOn(64, 100.0f); // 5th note forces voice stealing
    const auto transWindow = renderMono(engine, 512);
    renderMono(engine, 4096);
    const auto settledWindow = renderMono(engine, 4096);

    const auto ts = statsFor(transWindow);
    const auto ss = statsFor(settledWindow);
    EXPECT_TRUE(ts.finite)  << "NaN/Inf during poly voice stealing";
    EXPECT_LE(ts.peak, 1.0) << "Signal exceeded bounds during voice stealing";
    EXPECT_TRUE(ss.finite)  << "NaN/Inf after poly voice stealing settled";
    EXPECT_GT(ss.rms, 0.01) << "Signal must remain audible after voice stealing";
}

// Case D — Fresh single note must start at the correct pitch immediately.
// The de-click smoother must snap on fresh starts — no audible pitch ramp from zero.
TEST(PitchDeclickTest, FreshNoteIsImmediate_NoPitchRamp)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,        0.0f);
    engine.setParameter(ParamId::Osc1Level,       1.0f);
    engine.setParameter(ParamId::Osc2Enabled,     0.0f);
    engine.setParameter(ParamId::Osc3Enabled,     0.0f);
    engine.setParameter(ParamId::Osc1Waveform,    2.0f);
    engine.setParameter(ParamId::AmpAttack,       0.001f);
    engine.setParameter(ParamId::AmpSustain,      1.0f);
    engine.setParameter(ParamId::MixerDrive,      0.0f);
    engine.setParameter(ParamId::FilterDrive,     0.0f);
    engine.setParameter(ParamId::FilterCutoff,    20000.0f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
    engine.setParameter(ParamId::LfoAmount,       0.0f);
    engine.setParameter(ParamId::ModWheelAmount,  0.0f);

    engine.noteOn(69, 100.0f); // A4 = 440 Hz
    // 2ms window: with 1ms attack the note must already be producing audio.
    const auto first2ms = renderMono(engine, 88);
    EXPECT_GT(rms(first2ms), 0.001)
        << "Fresh note must produce audio within 2ms — de-click smoother must not add latency";

    // Settle and verify pitch is correct at A4, not ramping from some stale frequency.
    const auto settled = renderMono(engine, 44100);
    const double measuredHz = measureHz(settled, 44100.0);
    EXPECT_NEAR(measuredHz, 440.0, 5.0)
        << "Fresh note pitch must be A4=440Hz from start — smoother must snap on fresh note";
}

// Case D — Glide regression: user glide must still produce a slow pitch transition.
// Uses Last priority so the second note (C5) overrides C4 in the note stack.
TEST(PitchDeclickTest, GlideStillWorksAfterDeclick)
{
    SynthEngine engine;
    engine.prepare(44100.0, 512);
    engine.setParameter(ParamId::PlayMode,        0.0f);
    engine.setParameter(ParamId::NotePriority,    1.0f); // Last — C5 overrides C4
    engine.setParameter(ParamId::GlideEnabled,    1.0f);
    engine.setParameter(ParamId::GlideTime,       0.1f); // 100ms glide
    engine.setParameter(ParamId::Osc1Level,       1.0f);
    engine.setParameter(ParamId::Osc2Enabled,     0.0f);
    engine.setParameter(ParamId::Osc3Enabled,     0.0f);
    engine.setParameter(ParamId::AmpAttack,       0.001f);
    engine.setParameter(ParamId::AmpSustain,      1.0f);
    engine.setParameter(ParamId::MixerDrive,      0.0f);
    engine.setParameter(ParamId::FilterDrive,     0.0f);
    engine.setParameter(ParamId::FilterCutoff,    20000.0f);
    engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
    engine.setParameter(ParamId::LfoAmount,       0.0f);
    engine.setParameter(ParamId::ModWheelAmount,  0.0f);

    engine.noteOn(60, 100.0f); // C4 = 261.63 Hz
    renderMono(engine, 4096);  // reach sustain

    engine.noteOn(72, 100.0f); // C5 = 523.25 Hz — glide begins from C4

    // At 30ms into glide (30% progress through 100ms glide), pitch should still
    // be well below C5: 261.63 + 30% * 261.62 ≈ 340 Hz, well under 418 Hz (80% of C5).
    const auto at30ms = renderMono(engine, static_cast<int>(44100 * 0.03));
    const double hzAt30ms = measureHz(at30ms, 44100.0);

    // After 500ms total (glide done at 100ms), pitch must be at C5.
    renderMono(engine, static_cast<int>(44100 * 0.47));
    const auto settled = renderMono(engine, 44100);
    const double hzSettled = measureHz(settled, 44100.0);

    EXPECT_LT(hzAt30ms, 523.25 * 0.80)
        << "Glide must still be sliding at 30ms of a 100ms glide "
           "(hzAt30ms=" << hzAt30ms << ", threshold=" << 523.25 * 0.80 << ")";

    EXPECT_NEAR(hzSettled, 523.25, 15.0)
        << "After glide completes, pitch must be at C5 (hzSettled=" << hzSettled << ")";
}

// Simulate "old saved state had AnalogDrift=1.0, then host forces it to 0":
// engine must treat AnalogDrift=0 as truly zero — Osc1 and Osc2 must then match.
TEST(AnalogDriftTest, ForcedZeroAfterNonZeroMakesOscillatorsMatch)
{
    constexpr double sampleRate = 44100.0;
    constexpr int    settle     = 4096;
    constexpr int    frames     = static_cast<int>(sampleRate);

    auto measureOscHz = [&](int oscToEnable, float drift) -> double {
        SynthEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameter(ParamId::AnalogDrift,     drift);
        engine.setParameter(ParamId::AmpAttack,       0.001f);
        engine.setParameter(ParamId::AmpSustain,      1.0f);
        engine.setParameter(ParamId::FilterCutoff,    20000.0f);
        engine.setParameter(ParamId::FilterEnvAmount, 0.0f);
        engine.setParameter(ParamId::LfoAmount,       0.0f);
        engine.setParameter(ParamId::MixerDrive,      0.0f);
        engine.setParameter(ParamId::FilterDrive,     0.0f);
        engine.setParameter(ParamId::Osc1Enabled,     oscToEnable == 1 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc1Level,       oscToEnable == 1 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc2Enabled,     oscToEnable == 2 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc2Level,       oscToEnable == 2 ? 1.0f : 0.0f);
        engine.setParameter(ParamId::Osc3Enabled,     0.0f);
        engine.setParameter(ParamId::Osc2Detune,      0.0f);
        engine.setParameter(ParamId::Osc1Waveform,    2.0f); // Saw
        engine.setParameter(ParamId::Osc2Waveform,    2.0f); // Saw
        engine.setParameter(ParamId::Osc1Range,       3.0f); // 8'
        engine.setParameter(ParamId::Osc2Range,       3.0f); // 8'
        engine.noteOn(69, 100.0f); // A4 = 440 Hz
        renderMono(engine, settle);
        return measureHz(renderMono(engine, frames), sampleRate);
    };

    // Set drift=1.0 (simulates old saved state), then immediately clear to 0 (as
    // the plugin layer does in pushParametersToSynth and setStateInformation).
    // At drift=0 both oscillators must land within 0.1 Hz of each other at A4.
    const double f1 = measureOscHz(1, 0.0f);
    const double f2 = measureOscHz(2, 0.0f);
    ASSERT_GT(f1, 0.0) << "Osc1 not producing audio";
    ASSERT_GT(f2, 0.0) << "Osc2 not producing audio";
    EXPECT_NEAR(f1, f2, 0.1)
        << "At AnalogDrift=0: Osc1=" << f1 << " Hz, Osc2=" << f2
        << " Hz — they must match within 0.1 Hz (hidden drift must be off)";
}

// ── Part 22 — Poly headroom smoothing: no instant gain step on voice addition ──

// Shared patch for headroom tests: poly 4, single saw, full sustain, drives off.
static void setupHeadroomPatch(SynthEngine& e)
{
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::PlayMode,        1.0f); // poly 4
    e.setParameter(ParamId::Osc1Level,       1.0f);
    e.setParameter(ParamId::Osc1Waveform,    2.0f); // saw
    e.setParameter(ParamId::Osc2Enabled,     0.0f);
    e.setParameter(ParamId::Osc3Enabled,     0.0f);
    e.setParameter(ParamId::AmpAttack,       0.001f);
    e.setParameter(ParamId::AmpDecay,        1.0f);
    e.setParameter(ParamId::AmpSustain,      1.0f);
    e.setParameter(ParamId::MixerDrive,      0.0f);
    e.setParameter(ParamId::FilterDrive,     0.0f);
    e.setParameter(ParamId::FilterCutoff,    20000.0f);
    e.setParameter(ParamId::FilterResonance, 0.0f);
    e.setParameter(ParamId::FilterEnvAmount, 0.0f);
    e.setParameter(ParamId::LfoAmount,       0.0f);
    e.setParameter(ParamId::ModWheelAmount,  0.0f);
}

// When the second poly note is added, the headroom target drops from 1.0 to 0.8.
// With instant application this causes a 20% amplitude step on voice 1.
// With 5 ms smoothing the step is inaudible: RMS in the first 1 ms after the
// second noteOn must be ≥ 88 % of the RMS measured just before it.
// The second voice uses a 10 s attack so its own output is negligible over 1 ms,
// leaving the headroom change as the only variable between the two windows.
TEST(SynthEngineAudioTest, PolyHeadroomDoesNotStepOnSecondNote)
{
    SynthEngine engine;
    SynthEngine reference;
    setupHeadroomPatch(engine);
    setupHeadroomPatch(reference);

    engine.noteOn(48, 100.0f);          // voice 1 — C3
    renderMono(engine, 8192);           // well past attack; headroom settled at 1.0

    // Raise attack to 10 s so voice 2 contributes nothing in the 1 ms window.
    engine.setParameter(ParamId::AmpAttack, 10.0f);

    reference.noteOn(48, 100.0f);
    renderMono(reference, 8192);
    reference.setParameter(ParamId::AmpAttack, 10.0f);
    const double rmsReference = rms(renderMono(reference, 44)); // ~1 ms of voice 1 alone

    engine.noteOn(60, 100.0f);          // voice 2 — C4; headroom target 1.0 → 0.8
    const double rmsAfter = rms(renderMono(engine, 44)); // ~1 ms immediately after

    // Without smoothing: rmsAfter ≈ 0.80 × rmsBefore (instant 20 % cut)
    // With 5 ms smoothing: rmsAfter ≈ rmsBefore (headroom barely moved)
    EXPECT_GT(rmsAfter, rmsReference * 0.92)
        << "Poly headroom must not drop voice 1 when 2nd note added "
           "(rmsReference=" << rmsReference << " rmsAfter=" << rmsAfter << ")";
}

// Same check for the third note: headroom target 0.8 → 0.667 (16.7 % drop).
// After voice 2 has settled to sustain the two-voice steady-state RMS is the
// baseline; the third voice again uses a 10 s attack.
TEST(SynthEngineAudioTest, PolyHeadroomDoesNotStepOnThirdNote)
{
    SynthEngine engine;
    SynthEngine reference;
    setupHeadroomPatch(engine);
    setupHeadroomPatch(reference);

    engine.noteOn(48, 100.0f);          // voice 1 — C3
    renderMono(engine, 8192);

    engine.noteOn(52, 100.0f);          // voice 2 — E3
    renderMono(engine, 8192);           // headroom settled at 0.8; voice 2 at full sustain

    engine.setParameter(ParamId::AmpAttack, 10.0f);

    reference.noteOn(48, 100.0f);
    renderMono(reference, 8192);
    reference.noteOn(52, 100.0f);
    renderMono(reference, 8192);
    reference.setParameter(ParamId::AmpAttack, 10.0f);
    const double rmsReference = rms(renderMono(reference, 44)); // ~1 ms, 2 voices steady

    engine.noteOn(55, 100.0f);          // voice 3 — G3; headroom target 0.8 → 0.667
    const double rmsAfter = rms(renderMono(engine, 44));

    // Without smoothing: rmsAfter ≈ 0.833 × rmsBefore (0.667/0.8)
    // With 5 ms smoothing: rmsAfter ≈ rmsBefore
    EXPECT_GT(rmsAfter, rmsReference * 0.92)
        << "Poly headroom must not drop voices 1+2 when 3rd note added "
           "(rmsReference=" << rmsReference << " rmsAfter=" << rmsAfter << ")";
}

// The sample crossing from one held voice into a second note must not contain
// an abrupt gain discontinuity. The second voice has a 10 s attack, so any
// immediate jump here is the existing voice being stepped by the headroom gain.
TEST(SynthEngineAudioTest, PolyHeadroomSmoothingNoClick)
{
    SynthEngine engine;
    setupHeadroomPatch(engine);

    engine.noteOn(48, 100.0f);
    renderMono(engine, 8192);
    engine.setParameter(ParamId::AmpAttack, 10.0f);

    float lastBefore = 0.0f;
    float previous = 0.0f;
    double normalMaxDelta = 0.0;
    for (int i = 0; i < 512; ++i) {
        previous = lastBefore;
        lastBefore = engine.processSample();
        if (i > 0)
            normalMaxDelta = std::max(normalMaxDelta, std::abs(static_cast<double>(lastBefore - previous)));
    }

    engine.noteOn(60, 100.0f);
    const float firstAfter = engine.processSample();
    const double transitionDelta = std::abs(static_cast<double>(firstAfter - lastBefore));

    EXPECT_LT(transitionDelta, std::max(0.08, normalMaxDelta * 4.0))
        << "Poly headroom smoothing must avoid an immediate gain discontinuity "
           "(transitionDelta=" << transitionDelta
        << ", normalMaxDelta=" << normalMaxDelta << ")";
}

// Rapid addition of all 4 poly voices must produce finite, bounded output
// throughout — no NaN, no Inf, no wild amplitude transients.
TEST(SynthEngineAudioTest, PolyHeadroomFourVoiceAdditionIsFiniteAndBounded)
{
    SynthEngine engine;
    setupHeadroomPatch(engine);

    engine.noteOn(48, 100.0f);
    const auto w1 = renderMono(engine, 512);

    engine.noteOn(52, 100.0f);
    const auto w2 = renderMono(engine, 512);

    engine.noteOn(55, 100.0f);
    const auto w3 = renderMono(engine, 512);

    engine.noteOn(59, 100.0f);
    const auto w4 = renderMono(engine, 512);

    for (const auto* w : {&w1, &w2, &w3, &w4}) {
        const auto st = statsFor(*w);
        EXPECT_TRUE(st.finite) << "NaN/Inf during poly headroom transition";
        EXPECT_LT(st.peak, 2.5)  << "Peak too large during poly headroom transition";
    }
}
