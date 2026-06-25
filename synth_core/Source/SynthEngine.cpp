#include "../Include/SynthEngine.h"
#include <cstring>
#include <cassert>
#include <algorithm>

namespace SynthCore {

SynthEngine::SynthEngine()
{
    // Initialize all parameter defaults
    std::fill(std::begin(_params), std::end(_params), 0.0f);

    _params[static_cast<int>(ParamId::MasterVolume)]       = 1.0f;
    _params[static_cast<int>(ParamId::FilterCutoff)]       = 1000.0f;
    _params[static_cast<int>(ParamId::AmpAttack)]          = 0.01f;
    _params[static_cast<int>(ParamId::AmpDecay)]           = 0.1f;
    _params[static_cast<int>(ParamId::AmpSustain)]         = 0.7f;
    _params[static_cast<int>(ParamId::AmpRelease)]         = 0.3f;
    _params[static_cast<int>(ParamId::FilterAttack)]       = 0.01f;
    _params[static_cast<int>(ParamId::FilterDecay)]        = 0.1f;
    _params[static_cast<int>(ParamId::FilterSustain)]      = 0.5f;
    _params[static_cast<int>(ParamId::FilterRelease)]      = 0.2f;
    _params[static_cast<int>(ParamId::Osc1Level)]          = 1.0f;
    _params[static_cast<int>(ParamId::Osc2Level)]          = 0.0f;
    _params[static_cast<int>(ParamId::Osc3Level)]          = 0.0f;

    setSampleRate(44100.0);
}

void SynthEngine::setSampleRate(double sampleRate)
{
    assert(sampleRate >= kMinSampleRate && sampleRate <= kMaxSampleRate);
    _sampleRate = sampleRate;
    _propagateSampleRate();
}

void SynthEngine::_propagateSampleRate()
{
    _voice.setSampleRate(_sampleRate);
    _osc1.setSampleRate(_sampleRate);
    _osc2.setSampleRate(_sampleRate);
    _osc3.setSampleRate(_sampleRate);
    _mixer.setSampleRate(_sampleRate);
    _filter.setSampleRate(_sampleRate);
    _ampEnv.setSampleRate(_sampleRate);
    _filterEnv.setSampleRate(_sampleRate);
    _vca.setSampleRate(_sampleRate);
    _output.setSampleRate(_sampleRate);

    for (auto& s : _smoothers) s.setSampleRate(_sampleRate);
}

void SynthEngine::setParameter(ParamId id, float value)
{
    int idx = static_cast<int>(id);
    assert(idx >= 0 && idx < kParamCount);
    _params[idx] = value;
}

float SynthEngine::getParameter(ParamId id) const
{
    int idx = static_cast<int>(id);
    assert(idx >= 0 && idx < kParamCount);
    return _params[idx];
}

// Minimal MIDI 1.0 byte parser – handles note-on/off channel messages
void SynthEngine::processMidi(const uint8_t* data, int numBytes)
{
    if (numBytes < 1 || data == nullptr) return;

    uint8_t status = data[0] & 0xF0u;  // strip channel nibble

    if (status == 0x90u && numBytes >= 3) {
        uint8_t note     = data[1] & 0x7Fu;
        uint8_t velocity = data[2] & 0x7Fu;
        if (velocity == 0u) {
            _voice.noteOff(note);
        } else {
            _voice.noteOn(note, velocity);
        }
    } else if (status == 0x80u && numBytes >= 3) {
        uint8_t note = data[1] & 0x7Fu;
        _voice.noteOff(note);
    } else if (status == 0xB0u && numBytes >= 3) {
        // Control change: CC 123 = all-notes-off
        if (data[1] == 123u) _voice.allNotesOff();
    }
}

void SynthEngine::processBlock(float* outputBuffer, int numFrames)
{
    assert(outputBuffer != nullptr);
    assert(numFrames > 0);

    for (int frame = 0; frame < numFrames; ++frame) {
        // Smooth parameters – one pole per parameter, no allocation
        _applySmoothedParams(frame);

        // Advance voice controller / glide
        VoiceState vs = _voice.process();

        // Update oscillator pitches
        _osc1.setFrequency(vs.pitchHz);
        _osc2.setFrequency(vs.pitchHz);
        _osc3.setFrequency(vs.pitchHz);

        // Gate envelopes
        _ampEnv.gate(vs.gateOn);
        _filterEnv.gate(vs.gateOn);

        // DSP chain
        float osc1Out = _osc1.process();
        float osc2Out = _osc2.process();
        float osc3Out = _osc3.process();

        float mixed    = _mixer.process(osc1Out, osc2Out, osc3Out, 0.0f, 0.0f);
        float filterCv = _filterEnv.process();
        float filtered = _filter.process(mixed, filterCv);
        float ampCv    = _ampEnv.process();
        float amplified = _vca.process(filtered, ampCv);
        float out      = _output.process(amplified);

        // Interleaved stereo (identical L/R – mono engine)
        outputBuffer[frame * 2]     = out;
        outputBuffer[frame * 2 + 1] = out;
    }
}

void SynthEngine::_applySmoothedParams(int /*frame*/)
{
    // Smooth and forward key parameters each sample.
    // Additional parameters are wired here as milestones are completed.
    float masterVol = _smoothers[static_cast<int>(ParamId::MasterVolume)]
                          .process(_params[static_cast<int>(ParamId::MasterVolume)]);
    _vca.setLevel(masterVol);

    float glideTime = _smoothers[static_cast<int>(ParamId::GlideTime)]
                          .process(_params[static_cast<int>(ParamId::GlideTime)]);
    _voice.setGlideTime(glideTime);

    float cutoff = _smoothers[static_cast<int>(ParamId::FilterCutoff)]
                       .process(_params[static_cast<int>(ParamId::FilterCutoff)]);
    _filter.setCutoff(static_cast<double>(cutoff));

    float res = _smoothers[static_cast<int>(ParamId::FilterResonance)]
                    .process(_params[static_cast<int>(ParamId::FilterResonance)]);
    _filter.setResonance(res);
}

void SynthEngine::reset()
{
    _voice.allNotesOff();
    _output.reset();
    _railSag = 0.0f;
    for (auto& s : _smoothers) {
        // Re-init smoothers to zero
        s = ParamSmoother{};
        s.setSampleRate(_sampleRate);
    }
}

} // namespace SynthCore
