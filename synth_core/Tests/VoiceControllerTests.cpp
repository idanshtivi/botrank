#include <gtest/gtest.h>
#include "../Include/VoiceController.h"
#include <cmath>

using namespace SynthCore;

TEST(VoiceControllerBehaviorTest, SingleNoteAndA4Pitch)
{
    VoiceController voice;
    voice.setSampleRate(44100.0);
    voice.noteOn(69, 100.0f);
    const auto state = voice.process();

    EXPECT_TRUE(state.gateOn);
    EXPECT_NEAR(voice.getCurrentMidiNote(), 69.0, 0.001);
    EXPECT_NEAR(state.pitchHz, 440.0, 0.01);
    EXPECT_NEAR(voice.getCurrentFrequencyHz(), 440.0, 0.01);
}

TEST(VoiceControllerBehaviorTest, TriggerCanBeConsumedOnce)
{
    VoiceController voice;
    voice.noteOn(60, 100.0f);
    EXPECT_TRUE(voice.consumeTrigger());
    EXPECT_FALSE(voice.consumeTrigger());

    voice.noteOn(64, 100.0f);
    EXPECT_TRUE(voice.consumeTrigger());
}

TEST(VoiceControllerBehaviorTest, PitchBendRangeAffectsFrequency)
{
    VoiceController voice;
    voice.setSampleRate(44100.0);
    voice.noteOn(69, 100.0f);
    voice.process();

    voice.setPitchBendRange(12.0);
    voice.setPitchBend(12.0);
    const auto bent = voice.process();
    EXPECT_NEAR(bent.pitchHz, 880.0, 0.1);

    voice.setPitchBendRange(2.0);
    voice.setPitchBend(12.0);
    const auto clamped = voice.process();
    EXPECT_NEAR(clamped.pitchHz, 440.0 * std::pow(2.0, 2.0 / 12.0), 0.1);
}

TEST(VoiceControllerBehaviorTest, GlideDisabledChangesImmediately)
{
    VoiceController voice;
    voice.setSampleRate(44100.0);
    voice.setGlideEnabled(false);
    voice.setGlideTimeSeconds(1.0);
    voice.noteOn(60, 100.0f);
    voice.process();
    voice.noteOn(72, 100.0f);
    const auto state = voice.process();

    EXPECT_NEAR(state.pitchHz, 261.625565, 0.1);
}

TEST(VoiceControllerBehaviorTest, GlideEnabledChangesGradually)
{
    VoiceController voice;
    voice.setSampleRate(44100.0);
    voice.setGlideEnabled(true);
    voice.setGlideTimeSeconds(0.5);
    voice.noteOn(72, 100.0f);
    const auto first = voice.process();

    EXPECT_LT(first.pitchHz, 523.251);
    EXPECT_TRUE(std::isfinite(first.pitchHz));
}

TEST(VoiceControllerBehaviorTest, ResetClearsState)
{
    VoiceController voice;
    voice.noteOn(60, 100.0f);
    ASSERT_TRUE(voice.consumeTrigger());
    voice.reset();

    EXPECT_FALSE(voice.isGateHigh());
    EXPECT_FALSE(voice.consumeTrigger());
    EXPECT_NEAR(voice.getCurrentMidiNote(), 69.0, 0.001);
    EXPECT_NEAR(voice.getCurrentFrequencyHz(), 440.0, 0.01);
}

TEST(VoiceControllerBehaviorTest, LongProcessingRemainsFinite)
{
    VoiceController voice;
    voice.setSampleRate(48000.0);
    voice.setGlideEnabled(true);
    voice.setGlideTimeSeconds(0.25);
    voice.noteOn(36, 100.0f);

    for (int i = 0; i < 48000; ++i) {
        const auto state = voice.process();
        EXPECT_TRUE(std::isfinite(state.pitchHz));
        EXPECT_TRUE(std::isfinite(state.glideCv));
    }
}
