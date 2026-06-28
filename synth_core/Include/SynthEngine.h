#pragma once
#include "VoiceController.h"
#include "SynthVoice.h"
#include "OutputStage.h"
#include "DSPUtils.h"
#include "Lfo.h"
#include <array>
#include <cstdint>
#include <cstddef>

namespace SynthCore {

// Global parameter IDs – extend in later milestones
enum class ParamId : uint32_t {
    MasterVolume = 0,
    GlideTime,
    GlideEnabled,
    PitchBendRange,
    Osc1Enabled,
    Osc2Enabled,
    Osc3Enabled,
    Osc1Level,
    Osc2Level,
    Osc3Level,
    Osc1Waveform,
    Osc2Waveform,
    Osc3Waveform,
    Osc1Range,
    Osc2Range,
    Osc3Range,
    Osc2Detune,
    Osc3Detune,
    Osc3KeyboardTracking,
    MixerDrive,
    NoiseEnabled,
    NoiseLevel,
    NoiseMode,
    FilterCutoff,
    FilterResonance,
    FilterEnvAmount,
    FilterKeyboardTracking,
    FilterDrive,
    AmpAttack,
    AmpDecay,
    AmpSustain,
    AmpRelease,
    FilterAttack,
    FilterDecay,
    FilterSustain,
    FilterRelease,
    PlayMode,
    LfoEnabled,
    LfoRate,
    LfoAmount,
    LfoDestination,
    ModWheelAmount,
    FineTune,
    Legato,
    Retrigger,
    NotePriority,
    Osc1PulseWidth,
    AnalogDrift,
    Count
};

static constexpr int kParamCount = static_cast<int>(ParamId::Count);
static constexpr double kMinSampleRate = 44100.0;
static constexpr double kMaxSampleRate = 192000.0;
static constexpr int kPolyVoiceCount = 4;

inline float initPatchValue(ParamId id)
{
    switch (id) {
    case ParamId::MasterVolume:           return 0.60f;
    case ParamId::GlideTime:              return 0.05f;
    case ParamId::GlideEnabled:           return 0.0f;
    case ParamId::PitchBendRange:         return 2.0f;
    case ParamId::Osc1Enabled:            return 1.0f;
    case ParamId::Osc2Enabled:            return 1.0f;
    case ParamId::Osc3Enabled:            return 0.0f;
    case ParamId::Osc1Level:              return 0.65f;
    case ParamId::Osc2Level:              return 0.30f;
    case ParamId::Osc3Level:              return 0.0f;
    case ParamId::Osc1Waveform:           return 2.0f; // Saw
    case ParamId::Osc2Waveform:           return 2.0f; // Saw
    case ParamId::Osc3Waveform:           return 0.0f; // Tri
    case ParamId::Osc1Range:              return 3.0f; // 8'
    case ParamId::Osc2Range:              return 3.0f; // 8'
    case ParamId::Osc3Range:              return 3.0f; // 8'
    case ParamId::Osc2Detune:             return 0.05f;
    case ParamId::Osc3Detune:             return 0.0f;
    case ParamId::Osc3KeyboardTracking:   return 1.0f;
    case ParamId::MixerDrive:             return 1.0f;
    case ParamId::NoiseEnabled:           return 0.0f;
    case ParamId::NoiseLevel:             return 0.0f;
    case ParamId::NoiseMode:              return 0.0f; // White
    case ParamId::FilterCutoff:           return 6000.0f;
    case ParamId::FilterResonance:        return 0.10f;
    case ParamId::FilterEnvAmount:        return 0.15f;
    case ParamId::FilterKeyboardTracking: return 0.0f;
    case ParamId::FilterDrive:            return 0.50f;
    case ParamId::AmpAttack:              return 0.005f;
    case ParamId::AmpDecay:               return 0.25f;
    case ParamId::AmpSustain:             return 0.75f;
    case ParamId::AmpRelease:             return 0.20f;
    case ParamId::FilterAttack:           return 0.005f;
    case ParamId::FilterDecay:            return 0.30f;
    case ParamId::FilterSustain:          return 0.0f;
    case ParamId::FilterRelease:          return 0.20f;
    case ParamId::PlayMode:               return 0.0f;
    case ParamId::LfoEnabled:             return 0.0f;
    case ParamId::LfoRate:                return 2.0f;
    case ParamId::LfoAmount:              return 0.0f;
    case ParamId::LfoDestination:         return 0.0f;
    case ParamId::ModWheelAmount:         return 0.0f;
    case ParamId::FineTune:               return 0.0f;
    case ParamId::Legato:                 return 0.0f;
    case ParamId::Retrigger:              return 1.0f;
    case ParamId::NotePriority:           return 0.0f;
    case ParamId::Osc1PulseWidth:         return 0.50f;
    case ParamId::AnalogDrift:            return 0.0f;
    case ParamId::Count:                  break;
    }
    return 0.0f;
}

// Master orchestration: owns all DSP objects, no heap allocs inside processBlock
class SynthEngine {
public:
    SynthEngine();

    // Must be called before processBlock
    void prepare(double sampleRate, int blockSize);
    void setSampleRate(double sampleRate);
    double sampleRate() const { return _sampleRate; }

    // Thread-safe parameter update (called from UI/automation thread)
    void setParameter(ParamId id, float value);
    float getParameter(ParamId id) const;

    // Raw MIDI byte-level input
    void processMidi(const uint8_t* data, int numBytes);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void setPitchBend(double semitones);
    void setLoudnessAttack(double seconds);
    void setLoudnessDecay(double seconds);
    void setLoudnessSustain(double level);
    void setLoudnessRelease(double seconds);
    void setFilterAttack(double seconds);
    void setFilterDecay(double seconds);
    void setFilterSustain(double level);
    void setFilterRelease(double seconds);
    void setMasterVolume(double volume);
    void setOutputDrive(double drive);
    void setVCADrive(double drive);
    void setNoiseEnabled(bool enabled);
    void setNoiseLevel(double level);
    void setNoiseMode(NoiseMode mode);
    void setNoiseSeed(uint32_t seed);
    void setFilterCutoffHz(double hz);
    void setFilterResonance(double resonance);
    void setFilterContourAmount(double amount);
    void setFilterKeyboardTrackingAmount(double amount);
    void setFilterDrive(double drive);
    void setPlayMode(int mode);
    void setModWheel(double position);

    float processSample();
    // Fill stereo (interleaved L/R) output buffer – no heap allocation inside
    void processBlock(float* outputBuffer, int numFrames);

    // Reset all internal state (equivalent to power-cycle)
    void reset();

private:
    double _sampleRate = 44100.0;

    // All DSP objects pre-allocated as value members
    VoiceController _voiceCtrl;
    SynthVoice      _synthVoice;
    std::array<SynthVoice, kPolyVoiceCount> _polyVoices;
    std::array<int, kPolyVoiceCount> _polyMidiNotes {};
    std::array<bool, kPolyVoiceCount> _polyHeld {};
    std::array<uint64_t, kPolyVoiceCount> _polyAges {};
    uint64_t        _voiceAgeCounter = 0;
    uint32_t        _noteStartRng    = 0x52B7E151u; // persists across reset() for variety
    int             _playMode = 0;
    double          _pitchBendSemitones = 0.0;
    Lfo             _lfo;
    double          _modWheelPosition   = 0.0;
    OutputStage     _output;

    // Parameter smoothers – one per parameter
    ParamSmoother _smoothers[kParamCount];

    // Raw parameter targets (written from non-audio thread)
    float _params[kParamCount] = {};

    void _propagateSampleRate();
    void _applySmoothedParams(int frame);
    void _applyStaticPatchDefaults();
    void _applyParameter(ParamId id, float value);
    void _applyParameterToVoice(SynthVoice& voice, ParamId id, float value);
    void _applyParameterToAllVoices(ParamId id, float value);
    int _allocatePolyVoice(int midiNote);
    void _allPolyNotesOff();

    // Power-supply sag state (populated in Milestone 7)
    float _railSag = 0.0f;
};

} // namespace SynthCore
