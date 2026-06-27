#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
#include <cmath>
#include <vector>
#include <algorithm>

using namespace SynthCore;

static std::vector<float> renderN(SynthEngine& e, int n)
{
    std::vector<float> out(static_cast<size_t>(n));
    for (auto& s : out) s = e.processSample();
    return out;
}

static double peakToPeak(const std::vector<float>& buf)
{
    float mn = buf[0], mx = buf[0];
    for (float s : buf) { mn = std::min(mn, s); mx = std::max(mx, s); }
    return static_cast<double>(mx - mn);
}

static double maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0.0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        d = std::max(d, std::abs(static_cast<double>(a[i] - b[i])));
    return d;
}

// ── Helper: build a note-sustaining engine with given LFO settings ──────────
static SynthEngine makeSustainEngine(bool lfoOn, float rate, float amount, float dest,
                                     float mwAmount = 0.0f)
{
    SynthEngine e;
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::AmpAttack,  0.001f);
    e.setParameter(ParamId::AmpDecay,   0.01f);
    e.setParameter(ParamId::AmpSustain, 1.0f);
    e.setParameter(ParamId::LfoEnabled,     lfoOn ? 1.0f : 0.0f);
    e.setParameter(ParamId::LfoRate,        rate);
    e.setParameter(ParamId::LfoAmount,      amount);
    e.setParameter(ParamId::LfoDestination, dest);
    e.setParameter(ParamId::ModWheelAmount, mwAmount);
    e.noteOn(60, 100.0f);
    renderN(e, 2048); // skip attack
    return e;
}

// ── LFO off: output must be finite and non-zero ──────────────────────────────
TEST(LfoTest, OffOutputIsFiniteAndAudible)
{
    SynthEngine e = makeSustainEngine(false, 5.0f, 1.0f, 0.0f);
    const auto buf = renderN(e, 4410);
    bool finite = true, hasAudio = false;
    for (float s : buf) {
        if (!std::isfinite(s)) finite = false;
        if (std::abs(s) > 0.001f) hasAudio = true;
    }
    EXPECT_TRUE(finite);
    EXPECT_TRUE(hasAudio);
}

// ── LFO off must not change output relative to baseline with no LFO ─────────
TEST(LfoTest, OffDoesNotModulateOutput)
{
    // Two identical engines — only difference is LfoEnabled=0 vs LfoAmount=0.
    // They should produce identical audio since the LFO does nothing.
    SynthEngine a = makeSustainEngine(false, 5.0f, 1.0f, 0.0f);
    SynthEngine b = makeSustainEngine(false, 5.0f, 0.0f, 0.0f);
    const auto bufA = renderN(a, 4410);
    const auto bufB = renderN(b, 4410);
    EXPECT_LT(maxAbsDiff(bufA, bufB), 1e-5f);
}

// ── Pitch destination clearly changes output ─────────────────────────────────
TEST(LfoTest, PitchDestinationModulatesOutput)
{
    // LFO on, pitch dest, high amount at 5 Hz — must produce different audio
    // from same engine with LFO off.
    SynthEngine on  = makeSustainEngine(true,  5.0f, 1.0f, 0.0f);
    SynthEngine off = makeSustainEngine(false, 5.0f, 1.0f, 0.0f);

    const auto withLfo    = renderN(on,  44100);
    const auto withoutLfo = renderN(off, 44100);

    EXPECT_GT(maxAbsDiff(withLfo, withoutLfo), 0.01)
        << "Pitch LFO must produce audibly different output";
}

// ── Filter destination clearly changes output ─────────────────────────────────
TEST(LfoTest, FilterDestinationModulatesOutput)
{
    // LFO on, filter dest, high amount — cutoff swings so output differs from LFO-off.
    SynthEngine on  = makeSustainEngine(true,  3.0f, 1.0f, 1.0f);
    SynthEngine off = makeSustainEngine(false, 3.0f, 1.0f, 1.0f);

    const auto withLfo    = renderN(on,  44100);
    const auto withoutLfo = renderN(off, 44100);

    EXPECT_GT(maxAbsDiff(withLfo, withoutLfo), 0.01)
        << "Filter LFO must produce audibly different output";
}

// ── Pitch LFO causes periodic variation in instantaneous frequency ───────────
TEST(LfoTest, PitchLfoCreatesPeriodicVariation)
{
    // At 5 Hz LFO, one full cycle is 44100/5 = 8820 samples.
    // First half-cycle and second half-cycle of a sine wave produce mirror-image
    // pitch deviations. The running sums over each half must differ.
    SynthEngine e = makeSustainEngine(true, 5.0f, 1.0f, 0.0f);

    const int halfCycle = 44100 / 5 / 2; // ~4410 samples
    double sum1 = 0.0, sum2 = 0.0;
    for (int i = 0; i < halfCycle; ++i) sum1 += e.processSample();
    for (int i = 0; i < halfCycle; ++i) sum2 += e.processSample();

    EXPECT_NE(sum1, sum2) << "LFO pitch must create asymmetric periods";
    // Also verify the difference is substantial, not floating-point noise
    EXPECT_GT(std::abs(sum1 - sum2), 1.0)
        << "Half-cycle sums should differ meaningfully with 1.0 amount";
}

// ── Pulse width destination: output differs from LFO-off when using Square ──
TEST(LfoTest, PulseWidthDestinationModulatesSquareWave)
{
    auto makeSquareEngine = [](bool lfoOn) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpAttack,   0.001f);
        e.setParameter(ParamId::AmpDecay,    0.01f);
        e.setParameter(ParamId::AmpSustain,  1.0f);
        e.setParameter(ParamId::Osc1Waveform, 4.0f); // Square
        e.setParameter(ParamId::Osc2Enabled,  0.0f);
        e.setParameter(ParamId::LfoEnabled,     lfoOn ? 1.0f : 0.0f);
        e.setParameter(ParamId::LfoRate,        3.0f);
        e.setParameter(ParamId::LfoAmount,      1.0f);
        e.setParameter(ParamId::LfoDestination, 2.0f); // Pulse Width
        e.noteOn(60, 100.0f);
        renderN(e, 2048);
        return e;
    };

    auto on  = makeSquareEngine(true);
    auto off = makeSquareEngine(false);

    const auto withLfo    = renderN(on,  44100);
    const auto withoutLfo = renderN(off, 44100);

    EXPECT_GT(maxAbsDiff(withLfo, withoutLfo), 0.001)
        << "Pulse width LFO must change square wave output";
}

// ── Mod wheel adds to base amount — does not block LFO ───────────────────────
TEST(LfoTest, ModWheelAmountAddsToBaseAmount)
{
    // LFO on, amount=0.5, modWheelAmount=0.5, modWheel=1.0 → total depth=1.0
    // Must equal: amount=1.0, modWheelAmount=0, modWheel=0.0 → total depth=1.0
    // Both wheels must be set BEFORE noteOn so phase histories are identical.
    auto makeEngine = [](float amount, float mwAmount, double wheelPos) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpAttack,  0.001f);
        e.setParameter(ParamId::AmpDecay,   0.01f);
        e.setParameter(ParamId::AmpSustain, 1.0f);
        e.setParameter(ParamId::LfoEnabled,     1.0f);
        e.setParameter(ParamId::LfoRate,        5.0f);
        e.setParameter(ParamId::LfoAmount,      amount);
        e.setParameter(ParamId::ModWheelAmount, mwAmount);
        e.setModWheel(wheelPos);
        e.noteOn(60, 100.0f);
        renderN(e, 2048);
        return e;
    };

    auto withWheel = makeEngine(0.5f, 0.5f, 1.0);  // 0.5 + 0.5*1.0 = 1.0
    auto noWheel   = makeEngine(1.0f, 0.0f, 0.0);  // 1.0 + 0.0*0.0 = 1.0

    const auto bufWheel   = renderN(withWheel, 4410);
    const auto bufNoWheel = renderN(noWheel,   4410);
    EXPECT_LT(maxAbsDiff(bufWheel, bufNoWheel), 0.02)
        << "Mod wheel at max + base 0.5 must equal base 1.0 with no wheel";
}

TEST(LfoTest, ModWheelAtZeroDoesNotBlockBaseLfo)
{
    // LFO on, amount=1.0, modWheel=0 → LFO must still modulate (wheel doesn't mute)
    SynthEngine e   = makeSustainEngine(true,  5.0f, 1.0f, 0.0f, 0.5f);
    e.setModWheel(0.0);
    SynthEngine off = makeSustainEngine(false, 5.0f, 1.0f, 0.0f, 0.5f);

    EXPECT_GT(maxAbsDiff(renderN(e, 44100), renderN(off, 44100)), 0.01)
        << "Mod wheel at 0 must not mute the base LFO amount";
}

TEST(LfoTest, VisibleModWheelAmountWorksWithoutMidiCc)
{
    // Standalone UI knob must be audible without an external MIDI mod wheel.
    SynthEngine on  = makeSustainEngine(true,  5.0f, 0.0f, 0.0f, 1.0f);
    SynthEngine off = makeSustainEngine(false, 5.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_GT(maxAbsDiff(renderN(on, 44100), renderN(off, 44100)), 0.01)
        << "Visible Mod Wheel/Wheel Depth knob must add LFO depth without MIDI CC1";
}

// ── Output always stays finite and bounded regardless of LFO settings ────────
TEST(LfoTest, OutputRemainsFiniteAndBoundedAllDestinations)
{
    for (int dest = 0; dest <= 2; ++dest) {
        SynthEngine e = makeSustainEngine(true, 10.0f, 1.0f, static_cast<float>(dest));
        const auto buf = renderN(e, 44100);
        for (size_t i = 0; i < buf.size(); ++i) {
            EXPECT_TRUE(std::isfinite(buf[i]))  << "dest=" << dest << " sample=" << i;
            EXPECT_LE(std::abs(buf[i]), 1.0f)   << "dest=" << dest << " sample=" << i;
        }
    }
}

// ── Mono mode: LFO pitch modulation is audible ───────────────────────────────
TEST(LfoTest, MonoModePitchLfoAudible)
{
    SynthEngine e;
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::PlayMode,       0.0f); // Mono
    e.setParameter(ParamId::AmpSustain,     1.0f);
    e.setParameter(ParamId::LfoEnabled,     1.0f);
    e.setParameter(ParamId::LfoRate,        5.0f);
    e.setParameter(ParamId::LfoAmount,      1.0f);
    e.setParameter(ParamId::LfoDestination, 0.0f); // Pitch
    e.noteOn(60, 100.0f);
    renderN(e, 2048);

    const double pp = peakToPeak(renderN(e, 44100));
    EXPECT_GT(pp, 0.01) << "Mono pitch LFO must produce audible variation";
}

// ── Poly 4 mode: LFO affects all active voices ───────────────────────────────
TEST(LfoTest, PolyModePitchLfoAudible)
{
    SynthEngine e;
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::PlayMode,       1.0f); // Poly 4
    e.setParameter(ParamId::AmpSustain,     1.0f);
    e.setParameter(ParamId::LfoEnabled,     1.0f);
    e.setParameter(ParamId::LfoRate,        5.0f);
    e.setParameter(ParamId::LfoAmount,      1.0f);
    e.setParameter(ParamId::LfoDestination, 0.0f); // Pitch
    e.noteOn(60, 100.0f);
    e.noteOn(64, 100.0f);
    e.noteOn(67, 100.0f);
    renderN(e, 2048);

    const auto buf = renderN(e, 44100);
    bool finite = true;
    for (float s : buf) { if (!std::isfinite(s) || std::abs(s) > 1.0f) { finite = false; break; } }
    EXPECT_TRUE(finite) << "Poly LFO output must stay finite and bounded";
    EXPECT_GT(peakToPeak(buf), 0.01) << "Poly pitch LFO must produce audible variation";
}

// ── Existing tests: Mono and Poly 4 still work without LFO ──────────────────
TEST(LfoTest, MonoStillWorksWithLfoOff)
{
    SynthEngine e;
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::PlayMode,   0.0f);
    e.setParameter(ParamId::LfoEnabled, 0.0f);
    e.noteOn(69, 100.0f);
    const auto buf = renderN(e, 8192);
    double s2 = 0.0;
    for (float s : buf) { EXPECT_TRUE(std::isfinite(s)); s2 += s * s; }
    EXPECT_GT(std::sqrt(s2 / buf.size()), 0.001);
}

TEST(LfoTest, PolyStillWorksWithLfoOff)
{
    SynthEngine e;
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::PlayMode,   1.0f);
    e.setParameter(ParamId::LfoEnabled, 0.0f);
    e.noteOn(60, 100.0f);
    e.noteOn(64, 100.0f);
    const auto buf = renderN(e, 8192);
    double s2 = 0.0;
    for (float s : buf) { EXPECT_TRUE(std::isfinite(s)); s2 += s * s; }
    EXPECT_GT(std::sqrt(s2 / buf.size()), 0.001);
}
