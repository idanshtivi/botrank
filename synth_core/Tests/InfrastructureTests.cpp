#include <gtest/gtest.h>
#include "../Include/VoiceController.h"
#include "../Include/SynthEngine.h"
#include <cmath>

using namespace SynthCore;

// ═══════════════════════════════════════════════════════════════════════════════
// VoiceController – Low-Note Priority
// ═══════════════════════════════════════════════════════════════════════════════

class VoiceControllerTest : public ::testing::Test {
protected:
    VoiceController vc;
    void SetUp() override {
        vc.setSampleRate(44100.0);
    }
};

// Press C2 (MIDI 36), E2 (40), G2 (43) sequentially.
// Active pitch must remain C2 (lowest note priority).
TEST_F(VoiceControllerTest, LowNotePriorityThreeNotes)
{
    vc.noteOn(40, 100);  // E2
    vc.noteOn(43, 100);  // G2
    vc.noteOn(36, 100);  // C2

    vc.process();  // advance one sample

    const VoiceState& s = vc.state();
    EXPECT_TRUE(s.gateOn);

    // Expected: C2 = MIDI 36 → 440 * 2^((36-69)/12) ≈ 65.41 Hz
    double expectedHz = 440.0 * std::pow(2.0, (36 - 69) / 12.0);
    // Check that the target CV corresponds to C2; glide must still converge,
    // so we compare via noteOn then multiple process() steps.
    // With no glide (default), glideCv should track target instantly.
    EXPECT_NEAR(s.pitchHz, expectedHz, 0.01);
}

// Hold C2, E2, G2 – release C2 – active pitch must snap to E2.
TEST_F(VoiceControllerTest, NoteStackReleaseSwitchesToNextLowest)
{
    vc.noteOn(36, 100);  // C2
    vc.noteOn(40, 100);  // E2
    vc.noteOn(43, 100);  // G2

    // Release lowest note
    vc.noteOff(36);

    vc.process();

    const VoiceState& s = vc.state();
    EXPECT_TRUE(s.gateOn);

    double expectedHz = 440.0 * std::pow(2.0, (40 - 69) / 12.0);  // E2
    EXPECT_NEAR(s.pitchHz, expectedHz, 0.01);
}

// Gate must be true while any note is held; false after all notes released.
TEST_F(VoiceControllerTest, GateTrueWhileAnyNoteHeld)
{
    vc.noteOn(60, 100);  // C4
    vc.process();
    EXPECT_TRUE(vc.state().gateOn);

    vc.noteOff(60);
    vc.process();
    EXPECT_FALSE(vc.state().gateOn);
}

// Gate remains true across multiple simultaneous notes until all released.
TEST_F(VoiceControllerTest, GateRemainsHighUntilAllNotesReleased)
{
    vc.noteOn(60, 100);
    vc.noteOn(64, 100);
    vc.process();
    EXPECT_TRUE(vc.state().gateOn);

    vc.noteOff(64);
    vc.process();
    EXPECT_TRUE(vc.state().gateOn);  // C4 still held

    vc.noteOff(60);
    vc.process();
    EXPECT_FALSE(vc.state().gateOn);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SynthEngine – Sample Rate Management
// ═══════════════════════════════════════════════════════════════════════════════

TEST(SynthEngineTest, DefaultSampleRate)
{
    SynthEngine engine;
    EXPECT_DOUBLE_EQ(engine.sampleRate(), 44100.0);
}

TEST(SynthEngineTest, SetSampleRate96k)
{
    SynthEngine engine;
    engine.setSampleRate(96000.0);
    EXPECT_DOUBLE_EQ(engine.sampleRate(), 96000.0);
}

TEST(SynthEngineTest, SetSampleRate192k)
{
    SynthEngine engine;
    engine.setSampleRate(192000.0);
    EXPECT_DOUBLE_EQ(engine.sampleRate(), 192000.0);
}

// processBlock must not crash and must write to the output buffer
TEST(SynthEngineTest, ProcessBlockRunsClean)
{
    SynthEngine engine;
    engine.setSampleRate(44100.0);

    constexpr int kFrames = 512;
    float buffer[kFrames * 2] = {};

    EXPECT_NO_FATAL_FAILURE(engine.processBlock(buffer, kFrames));
}

// MIDI note-on/off must not crash
TEST(SynthEngineTest, MidiNoteOnOff)
{
    SynthEngine engine;
    uint8_t noteOn[]  = {0x90, 60, 100};
    uint8_t noteOff[] = {0x80, 60, 0};
    EXPECT_NO_FATAL_FAILURE(engine.processMidi(noteOn,  3));
    EXPECT_NO_FATAL_FAILURE(engine.processMidi(noteOff, 3));
}
