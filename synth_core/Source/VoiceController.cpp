#include "../Include/VoiceController.h"
#include <cmath>
#include <algorithm>

namespace SynthCore {

// MIDI note 69 (A4) maps to 0 V (440 Hz reference)
static constexpr float kCvRefNote = 69.0f;
static constexpr float kCvPerOctave = 1.0f;  // 1 V/oct
static constexpr float kNotesPerOctave = 12.0f;
static constexpr double kA4Hz = 440.0;

float VoiceController::_noteToCv(uint8_t note)
{
    return (static_cast<float>(note) - kCvRefNote) / kNotesPerOctave * kCvPerOctave;
}

double VoiceController::_cvToHz(float cv)
{
    // f = 440 * 2^(cv) where cv is octaves above A4
    return kA4Hz * std::pow(2.0, static_cast<double>(cv));
}

void VoiceController::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
    // Recompute glide coefficient from current glide time
    setGlideTime(_glideTime);
}

void VoiceController::setGlideTime(float seconds)
{
    _glideTime = seconds;
    if (seconds <= 0.0f || _sampleRate <= 0.0) {
        _glideCoeff = 1.0f;  // instant tracking
    } else {
        // 1-pole integrator: coeff = e^(-1/(tau*fs))
        _glideCoeff = static_cast<float>(std::exp(-1.0 / (static_cast<double>(seconds) * _sampleRate)));
    }
}

void VoiceController::noteOn(uint8_t midiNote, uint8_t /*velocity*/)
{
    // Avoid duplicates in the stack
    for (int i = 0; i < _stackSize; ++i) {
        if (_stack[static_cast<size_t>(i)] == midiNote) return;
    }
    _insertSorted(midiNote);
    _rebuildState();
    _state.triggered = true;
}

void VoiceController::noteOff(uint8_t midiNote)
{
    _removeSorted(midiNote);
    _rebuildState();
    _state.triggered = false;
}

void VoiceController::allNotesOff()
{
    _stackSize = 0;
    _state.gateOn    = false;
    _state.triggered = false;
}

VoiceState VoiceController::process()
{
    // Advance glide integrator toward target CV
    if (_glideCoeff < 1.0f) {
        _state.glideCv = _glideCoeff * _state.glideCv + (1.0f - _glideCoeff) * _targetCv;
    } else {
        _state.glideCv = _targetCv;
    }

    _state.pitchHz = _cvToHz(_state.glideCv);

    // triggered is a single-sample pulse – clear after process() returns
    bool trig = _state.triggered;
    _state.triggered = false;
    VoiceState out = _state;
    out.triggered = trig;  // expose it for this sample only
    return out;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void VoiceController::_rebuildState()
{
    if (_stackSize == 0) {
        _state.gateOn = false;
    } else {
        // Low-note priority: stack[0] is always the lowest note
        uint8_t lowestNote = _stack[0];
        _targetCv = _noteToCv(lowestNote);
        _state.gateOn = true;
    }
}

void VoiceController::_insertSorted(uint8_t note)
{
    if (_stackSize >= kMaxNoteStackDepth) return;

    // Find insertion point (ascending order)
    int pos = _stackSize;
    for (int i = 0; i < _stackSize; ++i) {
        if (note < _stack[static_cast<size_t>(i)]) {
            pos = i;
            break;
        }
    }
    // Shift right
    for (int i = _stackSize; i > pos; --i) {
        _stack[static_cast<size_t>(i)] = _stack[static_cast<size_t>(i - 1)];
    }
    _stack[static_cast<size_t>(pos)] = note;
    ++_stackSize;
}

void VoiceController::_removeSorted(uint8_t note)
{
    int found = -1;
    for (int i = 0; i < _stackSize; ++i) {
        if (_stack[static_cast<size_t>(i)] == note) {
            found = i;
            break;
        }
    }
    if (found < 0) return;

    // Shift left
    for (int i = found; i < _stackSize - 1; ++i) {
        _stack[static_cast<size_t>(i)] = _stack[static_cast<size_t>(i + 1)];
    }
    --_stackSize;
}

} // namespace SynthCore
