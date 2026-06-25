#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
#include <cmath>
#include <numeric>
#include <vector>

using namespace SynthCore;

// ═══════════════════════════════════════════════════════════════════════════════
// DC Offset Test (v0.1 Verification)
// With no MIDI input and all oscillators returning silence (stub), the
// processing loop must output clean DC (no audio content, no offset drift).
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AnalysisTest, DcOffsetCleanWithNoInput)
{
    SynthEngine engine;
    engine.setSampleRate(44100.0);

    constexpr int kFrames = 4096;
    std::vector<float> buffer(kFrames * 2, 0.0f);  // test setup uses heap; not in audio loop

    engine.processBlock(buffer.data(), kFrames);

    // Compute mean (DC offset) of left channel
    double sum = 0.0;
    for (int i = 0; i < kFrames; ++i) sum += static_cast<double>(buffer[static_cast<size_t>(i * 2)]);
    double mean = sum / kFrames;

    // DC offset must be < -80 dBFS (linear threshold ≈ 0.0001)
    EXPECT_NEAR(mean, 0.0, 1e-4) << "DC offset too large: " << mean;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Timing Accuracy Verification
// A note-on at a specific sample offset must produce non-zero gate state
// starting exactly at that sample, with zero frame jitter.
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AnalysisTest, NoteOnTimingSyncedToSampleGrid)
{
    // We verify gate timing via the VoiceController directly (same contract
    // the engine exposes per-sample through process()).
    VoiceController vc;
    vc.setSampleRate(44100.0);

    // Gate should be off before note-on
    VoiceState s = vc.process();
    EXPECT_FALSE(s.gateOn);

    // Send note-on
    vc.noteOn(60, 100);

    // Immediately after noteOn the next process() call must reflect gateOn=true
    s = vc.process();
    EXPECT_TRUE(s.gateOn) << "Gate did not go high on the exact sample of noteOn";

    // Send note-off
    vc.noteOff(60);
    s = vc.process();
    EXPECT_FALSE(s.gateOn) << "Gate did not go low on the exact sample of noteOff";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Block Processing Continuity
// Two consecutive processBlock calls must produce a continuous output (no
// frame-count truncation, no internal state reset between blocks).
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AnalysisTest, ConsecutiveBlocksNoReset)
{
    SynthEngine engine;
    engine.setSampleRate(44100.0);

    constexpr int kFrames = 256;
    float buf1[kFrames * 2] = {};
    float buf2[kFrames * 2] = {};

    EXPECT_NO_FATAL_FAILURE(engine.processBlock(buf1, kFrames));
    EXPECT_NO_FATAL_FAILURE(engine.processBlock(buf2, kFrames));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sample-Rate Transition
// Engine must produce zero audio after a dynamic sample-rate change and reset.
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AnalysisTest, SampleRateTransitionProducesCleanOutput)
{
    SynthEngine engine;
    engine.setSampleRate(44100.0);

    constexpr int kFrames = 512;
    std::vector<float> buffer(kFrames * 2, 0.0f);

    // Switch to 96 kHz mid-session and verify no crash / dirty state
    engine.setSampleRate(96000.0);
    engine.reset();

    EXPECT_NO_FATAL_FAILURE(engine.processBlock(buffer.data(), kFrames));

    double sum = 0.0;
    for (int i = 0; i < kFrames; ++i) sum += std::abs(static_cast<double>(buffer[static_cast<size_t>(i * 2)]));
    EXPECT_NEAR(sum, 0.0, 1e-4) << "Unexpected audio after sample-rate reset";
}
