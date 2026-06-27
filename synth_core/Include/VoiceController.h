#pragma once
#include <array>
#include <cstdint>

namespace SynthCore {

// Maximum simultaneous physical keys tracked in the note stack
static constexpr int kMaxNoteStackDepth = 16;

struct VoiceState {
    double  pitchHz    = 0.0;   // active pitch in Hz
    float   glideCv    = 0.0f;  // current glide CV (V, 1V/oct)
    bool    gateOn     = false;
    bool    triggered  = false; // single-sample true on new note-on
};

// Monophonic voice controller – configurable priority, legato, retrigger
class VoiceController {
public:
    enum class NotePriority { Low, Last, High };

    VoiceController() = default;

    void setSampleRate(double sampleRate);

    // Raw MIDI input
    void noteOn (uint8_t midiNote, uint8_t velocity);
    void noteOn(int midiNote, float velocity);
    void noteOff(uint8_t midiNote);
    void noteOff(int midiNote);
    void allNotesOff();
    void reset();

    // Glide (portamento) time in seconds; 0 = instant
    void setGlideTime(float seconds);
    void setGlideEnabled(bool enabled);
    void setGlideTimeSeconds(double seconds);
    void setPitchBend(double semitones);
    void setPitchBendRange(double semitones);

    // Mono performance controls
    void setLegato(bool legato);        // skip envelope retrigger when gate already held
    void setRetrigger(bool retrigger);  // force retrigger even in legato mode
    void setNotePriority(NotePriority priority);

    // Advance the glide integrator by one sample and return current state
    VoiceState process();

    const VoiceState& state() const { return _state; }
    double getCurrentMidiNote() const;
    double getCurrentFrequencyHz() const;
    bool isGateHigh() const;
    bool consumeTrigger();

private:
    // Pitch-sorted note stack (ascending, index 0 = lowest)
    std::array<uint8_t, kMaxNoteStackDepth> _stack{};
    int     _stackSize    = 0;

    // Insertion-order stack for Last priority
    std::array<uint8_t, kMaxNoteStackDepth> _orderStack{};
    int     _orderSize    = 0;

    double  _sampleRate   = 44100.0;
    float   _glideTime    = 0.0f;
    float   _glideCoeff   = 1.0f;
    bool    _glideEnabled = true;
    double  _pitchBendSemitones = 0.0;
    double  _pitchBendRange = 2.0;

    bool          _legato       = false;
    bool          _retrigger    = true;
    NotePriority  _notePriority = NotePriority::Low;

    float   _targetCv     = 0.0f;
    double  _currentMidiNote = 69.0;

    VoiceState _state;

    void    _rebuildState();
    void    _insertSorted(uint8_t note);
    void    _removeSorted(uint8_t note);

    static float  _noteToCv(uint8_t note);
    static double _cvToHz(float cv);
};

} // namespace SynthCore
