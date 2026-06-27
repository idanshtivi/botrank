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
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
    // Recompute glide coefficient from current glide time
    setGlideTime(_glideTime);
}

void VoiceController::setGlideTime(float seconds)
{
    if (!std::isfinite(seconds)) seconds = 0.0f;
    _glideTime = std::clamp(seconds, 0.0f, 10.0f);
    if (!_glideEnabled || _glideTime <= 0.0f || _sampleRate <= 0.0) {
        _glideCoeff = 1.0f;  // instant tracking
    } else {
        // 1-pole integrator: coeff = e^(-1/(tau*fs))
        _glideCoeff = static_cast<float>(std::exp(-1.0 / (static_cast<double>(_glideTime) * _sampleRate)));
    }
}

void VoiceController::setGlideEnabled(bool enabled)
{
    _glideEnabled = enabled;
    setGlideTime(_glideTime);
}

void VoiceController::setGlideTimeSeconds(double seconds)
{
    if (!std::isfinite(seconds)) seconds = 0.0;
    setGlideTime(static_cast<float>(std::clamp(seconds, 0.0, 10.0)));
}

void VoiceController::setPitchBend(double semitones)
{
    if (!std::isfinite(semitones)) semitones = 0.0;
    _pitchBendSemitones = std::clamp(semitones, -_pitchBendRange, _pitchBendRange);
}

void VoiceController::setPitchBendRange(double semitones)
{
    if (!std::isfinite(semitones)) semitones = 2.0;
    _pitchBendRange = std::clamp(std::abs(semitones), 0.0, 24.0);
    setPitchBend(_pitchBendSemitones);
}

void VoiceController::setLegato(bool legato)       { _legato    = legato;    }
void VoiceController::setRetrigger(bool retrigger) { _retrigger = retrigger; }
void VoiceController::setNotePriority(NotePriority priority) { _notePriority = priority; }

void VoiceController::noteOn(uint8_t midiNote, uint8_t /*velocity*/)
{
    // Avoid duplicates in the pitch stack
    for (int i = 0; i < _stackSize; ++i) {
        if (_stack[static_cast<size_t>(i)] == midiNote) return;
    }

    const bool wasGateHigh = _state.gateOn;

    // Track insertion order for Last priority
    if (_orderSize < kMaxNoteStackDepth)
        _orderStack[static_cast<size_t>(_orderSize++)] = midiNote;

    _insertSorted(midiNote);
    _rebuildState();

    // Legato: skip envelope retrigger if gate was already held, unless retrigger forces it
    if (!_legato || !wasGateHigh || _retrigger)
        _state.triggered = true;
}

void VoiceController::noteOn(int midiNote, float velocity)
{
    const int note = std::clamp(midiNote, 0, 127);
    const int vel = std::clamp(static_cast<int>(velocity), 0, 127);
    if (vel == 0) {
        noteOff(note);
        return;
    }
    noteOn(static_cast<uint8_t>(note), static_cast<uint8_t>(vel));
}

void VoiceController::noteOff(uint8_t midiNote)
{
    _removeSorted(midiNote);

    // Remove from insertion-order stack
    for (int i = 0; i < _orderSize; ++i) {
        if (_orderStack[static_cast<size_t>(i)] == midiNote) {
            for (int j = i; j < _orderSize - 1; ++j)
                _orderStack[static_cast<size_t>(j)] = _orderStack[static_cast<size_t>(j + 1)];
            --_orderSize;
            break;
        }
    }

    _rebuildState();
    _state.triggered = false;
}

void VoiceController::noteOff(int midiNote)
{
    noteOff(static_cast<uint8_t>(std::clamp(midiNote, 0, 127)));
}

void VoiceController::allNotesOff()
{
    _stackSize  = 0;
    _orderSize  = 0;
    _state.gateOn    = false;
    _state.triggered = false;
    _targetCv = _noteToCv(static_cast<uint8_t>(std::clamp(static_cast<int>(_currentMidiNote), 0, 127)));
}

void VoiceController::reset()
{
    allNotesOff();
    _orderSize = 0;
    _currentMidiNote = 69.0;
    _targetCv = 0.0f;
    _state = VoiceState{};
    _state.pitchHz = kA4Hz;
}

VoiceState VoiceController::process()
{
    // Advance glide integrator toward target CV
    if (_glideCoeff < 1.0f) {
        _state.glideCv = _glideCoeff * _state.glideCv + (1.0f - _glideCoeff) * _targetCv;
    } else {
        _state.glideCv = _targetCv;
    }

    _state.pitchHz = _cvToHz(_state.glideCv + static_cast<float>(_pitchBendSemitones / 12.0));

    // triggered is a single-sample pulse – clear after process() returns
    bool trig = _state.triggered;
    _state.triggered = false;
    VoiceState out = _state;
    out.triggered = trig;  // expose it for this sample only
    return out;
}

double VoiceController::getCurrentMidiNote() const
{
    return _currentMidiNote;
}

double VoiceController::getCurrentFrequencyHz() const
{
    return _cvToHz(_noteToCv(static_cast<uint8_t>(std::clamp(static_cast<int>(_currentMidiNote), 0, 127)))
        + static_cast<float>(_pitchBendSemitones / 12.0));
}

bool VoiceController::isGateHigh() const
{
    return _state.gateOn;
}

bool VoiceController::consumeTrigger()
{
    const bool triggered = _state.triggered;
    _state.triggered = false;
    return triggered;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void VoiceController::_rebuildState()
{
    if (_stackSize == 0) {
        _state.gateOn = false;
    } else {
        uint8_t note;
        switch (_notePriority) {
        case NotePriority::High:
            note = _stack[static_cast<size_t>(_stackSize - 1)]; // highest (stack ascending)
            break;
        case NotePriority::Last:
            note = (_orderSize > 0)
                ? _orderStack[static_cast<size_t>(_orderSize - 1)]
                : _stack[0];
            break;
        case NotePriority::Low:
        default:
            note = _stack[0]; // lowest
            break;
        }
        _currentMidiNote = static_cast<double>(note);
        _targetCv = _noteToCv(note);
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
