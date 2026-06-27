#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace SynthCore;

// Verify that rendering a held note produces non-silent output across all
// DSP stages: Oscillator → Mixer → Filter (passthrough) → Envelope → VCA.
TEST(AudioOutputTest, RenderedNoteIsNotSilent)
{
    SynthEngine engine;
    engine.setSampleRate(44100.0);

    // A2 = MIDI 45 = 110 Hz
    uint8_t noteOn[] = { 0x90u, 45u, 100u };
    engine.processMidi(noteOn, 3);

    // Render 1 second. The envelope attack completes in ~10 ms (default 0.01 s),
    // and the param smoother for master volume converges within ~25 ms.
    // After those transients, the sustain level (default 0.7) is held.
    constexpr int kFrames = 44100;
    std::vector<float> buf(static_cast<size_t>(kFrames * 2), 0.0f);
    engine.processBlock(buf.data(), kFrames);

    float maxAbs = 0.0f;
    for (float s : buf)
        maxAbs = std::max(maxAbs, std::abs(s));

    EXPECT_GT(maxAbs, 0.01f)
        << "Rendered audio is silent; oscillator/envelope/mixer chain is broken.";
}

// Verify that silence is produced when no note is held.
TEST(AudioOutputTest, NoNoteProducesSilence)
{
    SynthEngine engine;
    engine.setSampleRate(44100.0);
    // No note-on sent.

    constexpr int kFrames = 4096;
    std::vector<float> buf(static_cast<size_t>(kFrames * 2), 0.0f);
    engine.processBlock(buf.data(), kFrames);

    float maxAbs = 0.0f;
    for (float s : buf)
        maxAbs = std::max(maxAbs, std::abs(s));

    // Gate is off → envelope stays at 0 → VCA output is 0.
    EXPECT_LT(maxAbs, 1e-6f)
        << "Expected silence with no active note, got signal: " << maxAbs;
}
