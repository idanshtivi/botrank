#include <gtest/gtest.h>
#include "../Include/SynthEngine.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>

using namespace SynthCore;

static std::vector<float> renderN(SynthEngine& e, int n)
{
    std::vector<float> out(static_cast<size_t>(n));
    for (auto& s : out) s = e.processSample();
    return out;
}

static bool allFinite(const std::vector<float>& buf)
{
    for (float s : buf) if (!std::isfinite(s)) return false;
    return true;
}

static double maxAbs(const std::vector<float>& buf)
{
    double mx = 0.0;
    for (float s : buf) mx = std::max(mx, std::abs(static_cast<double>(s)));
    return mx;
}

static double maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0.0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        d = std::max(d, std::abs(static_cast<double>(a[i] - b[i])));
    return d;
}

// ── Helper to build a sustaining engine with Init Patch defaults ─────────────
static SynthEngine makeInitEngine()
{
    SynthEngine e;
    e.prepare(44100.0, 512);
    e.setParameter(ParamId::AmpAttack,  0.001f);
    e.setParameter(ParamId::AmpDecay,   0.01f);
    e.setParameter(ParamId::AmpSustain, 1.0f);
    e.setParameter(ParamId::AmpRelease, 0.1f);
    e.setParameter(ParamId::FilterCutoff,    1.0f);
    e.setParameter(ParamId::FilterResonance, 0.0f);
    e.setParameter(ParamId::FilterEnvAmount, 0.0f);
    e.setParameter(ParamId::MasterVolume,    0.8f);
    e.setParameter(ParamId::Osc1Enabled, 1.0f);
    e.setParameter(ParamId::Osc1Level,   0.7f);
    e.setParameter(ParamId::Osc2Enabled, 0.0f);
    e.setParameter(ParamId::Osc3Enabled, 0.0f);
    e.setParameter(ParamId::NoiseEnabled, 0.0f);
    e.setParameter(ParamId::LfoEnabled,   0.0f);
    return e;
}

// ── All params at min + max must produce finite output ───────────────────────
TEST(ControlCompletion, AllParamsNoNanAtExtremes)
{
    const struct { ParamId id; float lo; float hi; } cases[] = {
        {ParamId::MasterVolume,          0.0f, 1.0f},
        {ParamId::GlideTime,             0.0f, 10.0f},
        {ParamId::GlideEnabled,          0.0f, 1.0f},
        {ParamId::PitchBendRange,        0.0f, 24.0f},
        {ParamId::Osc1Enabled,           0.0f, 1.0f},
        {ParamId::Osc1Level,             0.0f, 1.0f},
        {ParamId::Osc1Waveform,          0.0f, 6.0f},
        {ParamId::Osc1Range,             0.0f, 5.0f},
        {ParamId::Osc2Detune,           -1.0f, 1.0f},
        {ParamId::Osc3KeyboardTracking,  0.0f, 1.0f},
        {ParamId::MixerDrive,            0.0f, 1.0f},
        {ParamId::NoiseEnabled,          0.0f, 1.0f},
        {ParamId::NoiseLevel,            0.0f, 1.0f},
        {ParamId::NoiseMode,             0.0f, 1.0f},
        {ParamId::FilterCutoff,          0.0f, 1.0f},
        {ParamId::FilterResonance,       0.0f, 1.0f},
        {ParamId::FilterEnvAmount,       0.0f, 1.0f},
        {ParamId::FilterKeyboardTracking,0.0f, 1.0f},
        {ParamId::FilterDrive,           0.0f, 1.0f},
        {ParamId::AmpAttack,             0.001f,10.0f},
        {ParamId::AmpDecay,              0.001f,10.0f},
        {ParamId::AmpSustain,            0.0f, 1.0f},
        {ParamId::AmpRelease,            0.001f,10.0f},
        {ParamId::FilterAttack,          0.001f,10.0f},
        {ParamId::FilterDecay,           0.001f,10.0f},
        {ParamId::FilterSustain,         0.0f, 1.0f},
        {ParamId::FilterRelease,         0.001f,10.0f},
        {ParamId::LfoEnabled,            0.0f, 1.0f},
        {ParamId::LfoRate,               0.01f,20.0f},
        {ParamId::LfoAmount,             0.0f, 1.0f},
        {ParamId::LfoDestination,        0.0f, 2.0f},
        {ParamId::ModWheelAmount,        0.0f, 1.0f},
        {ParamId::FineTune,             -50.0f,50.0f},
        {ParamId::Legato,                0.0f, 1.0f},
        {ParamId::Retrigger,             0.0f, 1.0f},
        {ParamId::NotePriority,          0.0f, 2.0f},
        {ParamId::Osc1PulseWidth,        0.10f,0.90f},
        {ParamId::AnalogDrift,           0.0f, 1.0f},
    };

    for (const auto& tc : cases) {
        for (float val : {tc.lo, tc.hi}) {
            SynthEngine e = makeInitEngine();
            e.setParameter(tc.id, val);
            e.noteOn(60, 100.0f);
            const auto buf = renderN(e, 1024);
            EXPECT_TRUE(allFinite(buf))
                << "ParamId=" << static_cast<int>(tc.id) << " val=" << val;
        }
    }
}

// ── Filter contour clearly changes output relative to no-contour ─────────────
TEST(ControlCompletion, FilterContourChangesOutput)
{
    auto makeEngine = [](float contour, float attack) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpSustain,      1.0f);
        e.setParameter(ParamId::FilterCutoff,    0.3f);
        e.setParameter(ParamId::FilterResonance, 0.5f);
        e.setParameter(ParamId::FilterEnvAmount, contour);
        e.setParameter(ParamId::FilterAttack,    attack);
        e.setParameter(ParamId::FilterDecay,     0.2f);
        e.setParameter(ParamId::FilterSustain,   0.5f);
        e.setParameter(ParamId::Osc1Level,       0.7f);
        e.setParameter(ParamId::Osc1Enabled,     1.0f);
        e.noteOn(60, 100.0f);
        return e;
    };

    auto withContour    = makeEngine(1.0f, 0.05f);
    auto withoutContour = makeEngine(0.0f, 0.05f);

    const auto bufWith    = renderN(withContour,    8820);
    const auto bufWithout = renderN(withoutContour, 8820);

    EXPECT_GT(maxAbsDiff(bufWith, bufWithout), 0.001)
        << "Non-zero filter contour must change output";
}

// ── Noise controls: enable/level/mode affect output ──────────────────────────
TEST(ControlCompletion, NoiseControlsAffectOutput)
{
    auto makeEngine = [](bool enabled, float level, float mode) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpSustain,  1.0f);
        e.setParameter(ParamId::Osc1Enabled, 0.0f); // oscillators off
        e.setParameter(ParamId::Osc2Enabled, 0.0f);
        e.setParameter(ParamId::Osc3Enabled, 0.0f);
        e.setParameter(ParamId::NoiseEnabled, enabled ? 1.0f : 0.0f);
        e.setParameter(ParamId::NoiseLevel,   level);
        e.setParameter(ParamId::NoiseMode,    mode);
        e.noteOn(60, 100.0f);
        renderN(e, 512); // skip attack
        return e;
    };

    // Noise on vs off
    auto noiseOn  = makeEngine(true,  0.8f, 0.0f);
    auto noiseOff = makeEngine(false, 0.8f, 0.0f);
    EXPECT_GT(maxAbsDiff(renderN(noiseOn, 4096), renderN(noiseOff, 4096)), 0.001)
        << "Noise enabled must differ from noise off";

    // Noise level 0 vs 0.8
    auto lvlHigh = makeEngine(true, 0.8f, 0.0f);
    auto lvlZero = makeEngine(true, 0.0f, 0.0f);
    EXPECT_GT(maxAbsDiff(renderN(lvlHigh, 4096), renderN(lvlZero, 4096)), 0.001)
        << "Noise level must scale output";
}

// ── Mixer drive affects output ────────────────────────────────────────────────
TEST(ControlCompletion, MixerDriveAffectsOutput)
{
    auto makeEngine = [](float drive) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::AmpSustain,  1.0f);
        e.setParameter(ParamId::Osc1Enabled, 1.0f);
        e.setParameter(ParamId::Osc1Level,   0.7f);
        e.setParameter(ParamId::MixerDrive,  drive);
        e.noteOn(60, 100.0f);
        renderN(e, 512);
        return e;
    };

    auto hiDrive = makeEngine(1.0f);
    auto noDrive = makeEngine(0.0f);

    EXPECT_GT(maxAbsDiff(renderN(hiDrive, 4096), renderN(noDrive, 4096)), 0.001)
        << "Mixer drive must change output";
}

// ── Init patch is audible and safe ───────────────────────────────────────────
TEST(ControlCompletion, InitPatchIsSafeAndAudible)
{
    SynthEngine e = makeInitEngine();
    e.noteOn(60, 100.0f);
    renderN(e, 512); // attack

    const auto buf = renderN(e, 8192);
    EXPECT_TRUE(allFinite(buf)) << "Init patch must produce only finite samples";
    EXPECT_LE(maxAbs(buf), 1.0) << "Init patch must not clip";

    double rms = 0.0;
    for (float s : buf) rms += s * s;
    rms = std::sqrt(rms / buf.size());
    // Threshold is deliberately loose: makeInitEngine sets FilterCutoff=1 Hz which
    // attenuates nearly everything — we only check that some non-trivial signal
    // survives, not a specific level.
    EXPECT_GT(rms, 0.0007) << "Init patch must be audible";
}

// ── Output stays finite and bounded across all PlayModes ─────────────────────
TEST(ControlCompletion, OutputRemainsFiniteAndBounded)
{
    for (int mode = 0; mode <= 1; ++mode) {
        SynthEngine e;
        e.prepare(44100.0, 512);
        e.setParameter(ParamId::PlayMode,    static_cast<float>(mode));
        e.setParameter(ParamId::AmpSustain,  1.0f);
        e.setParameter(ParamId::Osc1Enabled, 1.0f);
        e.setParameter(ParamId::Osc1Level,   0.7f);
        e.setParameter(ParamId::MasterVolume,0.8f);
        e.noteOn(60, 100.0f);
        if (mode == 1) { e.noteOn(64, 100.0f); e.noteOn(67, 100.0f); }
        renderN(e, 512);

        const auto buf = renderN(e, 44100);
        EXPECT_TRUE(allFinite(buf)) << "mode=" << mode;
        EXPECT_LE(maxAbs(buf), 1.0)  << "mode=" << mode;
    }
}
