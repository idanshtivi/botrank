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

// Monophonic voice controller – low-note priority, no dynamic allocation
class VoiceController {
public:
    VoiceController() = default;

    void setSampleRate(double sampleRate);

    // Raw MIDI input
    void noteOn (uint8_t midiNote, uint8_t velocity);
    void noteOff(uint8_t midiNote);
    void allNotesOff();

    // Glide (portamento) time in seconds; 0 = instant
    void setGlideTime(float seconds);

    // Advance the glide integrator by one sample and return current state
    VoiceState process();

    const VoiceState& state() const { return _state; }

private:
    // Sorted note stack (ascending pitch = index 0 is lowest)
    std::array<uint8_t, kMaxNoteStackDepth> _stack{};
    int     _stackSize    = 0;

    double  _sampleRate   = 44100.0;
    float   _glideTime    = 0.0f;    // seconds
    float   _glideCoeff   = 1.0f;    // 1-pole coeff for glide integrator

    // 1V/oct CV of the current target note
    float   _targetCv     = 0.0f;

    VoiceState _state;

    void    _rebuildState();
    void    _insertSorted(uint8_t note);
    void    _removeSorted(uint8_t note);

    // Convert MIDI note to 1V/oct CV  (MIDI 69 = 0 V reference)
    static float _noteToCv(uint8_t note);
    // Convert CV to Hz
    static double _cvToHz(float cv);
};

} // namespace SynthCore
