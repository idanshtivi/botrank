#pragma once
#include "VoiceController.h"
#include "Oscillator.h"
#include "Mixer.h"
#include "Filter.h"
#include "Envelope.h"
#include "Vca.h"
#include "OutputStage.h"
#include "DSPUtils.h"
#include <cstdint>
#include <cstddef>

namespace SynthCore {

// Global parameter IDs – extend in later milestones
enum class ParamId : uint32_t {
    MasterVolume = 0,
    GlideTime,
    Osc1Level,
    Osc2Level,
    Osc3Level,
    NoiseLevel,
    FilterCutoff,
    FilterResonance,
    FilterEnvAmount,
    AmpAttack,
    AmpDecay,
    AmpSustain,
    AmpRelease,
    FilterAttack,
    FilterDecay,
    FilterSustain,
    FilterRelease,
    Count
};

static constexpr int kParamCount = static_cast<int>(ParamId::Count);
static constexpr double kMinSampleRate = 44100.0;
static constexpr double kMaxSampleRate = 192000.0;

// Master orchestration: owns all DSP objects, no heap allocs inside processBlock
class SynthEngine {
public:
    SynthEngine();

    // Must be called before processBlock
    void setSampleRate(double sampleRate);
    double sampleRate() const { return _sampleRate; }

    // Thread-safe parameter update (called from UI/automation thread)
    void setParameter(ParamId id, float value);
    float getParameter(ParamId id) const;

    // Raw MIDI byte-level input
    void processMidi(const uint8_t* data, int numBytes);

    // Fill stereo (interleaved L/R) output buffer – no heap allocation inside
    void processBlock(float* outputBuffer, int numFrames);

    // Reset all internal state (equivalent to power-cycle)
    void reset();

private:
    double _sampleRate = 44100.0;

    // All DSP objects pre-allocated as value members
    VoiceController _voice;
    Oscillator      _osc1;
    Oscillator      _osc2;
    Oscillator      _osc3;
    Mixer           _mixer;
    Filter          _filter;
    Envelope        _ampEnv;
    Envelope        _filterEnv;
    Vca             _vca;
    OutputStage     _output;

    // Parameter smoothers – one per parameter
    ParamSmoother _smoothers[kParamCount];

    // Raw parameter targets (written from non-audio thread)
    float _params[kParamCount] = {};

    void _propagateSampleRate();
    void _applySmoothedParams(int frame);

    // Power-supply sag state (populated in Milestone 7)
    float _railSag = 0.0f;
};

} // namespace SynthCore
