#include "../Include/OscillatorBank.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

void OscillatorBank::setSampleRate(double sampleRate)
{
    _osc1.setSampleRate(sampleRate);
    _osc2.setSampleRate(sampleRate);
    _osc3.setSampleRate(sampleRate);
}

void OscillatorBank::setBaseMidiNote(double note)
{
    if (!std::isfinite(note)) note = 69.0;
    _baseMidiNote = std::clamp(note, 0.0, 127.0);
    _updatePitch();
}

void OscillatorBank::setPitchBendSemitones(double semitones)
{
    if (!std::isfinite(semitones)) semitones = 0.0;
    _pitchBendSemitones = std::clamp(semitones, -24.0, 24.0);
    _updatePitch();
}

void OscillatorBank::setOscillatorWaveform(int oscIndex, Waveform waveform)
{
    if (auto* osc = _oscillator(oscIndex)) osc->setWaveform(waveform);
}

void OscillatorBank::setOscillatorRange(int oscIndex, OscillatorRange range)
{
    if (auto* osc = _oscillator(oscIndex)) osc->setRange(range);
}

void OscillatorBank::setOscillatorDetuneSemitones(int oscIndex, double semitones)
{
    if (!std::isfinite(semitones)) semitones = 0.0;
    if (auto* osc = _oscillator(oscIndex)) osc->setDetuneSemitones(std::clamp(semitones, -7.0, 7.0));
}

void OscillatorBank::setOscillatorFineTuneCents(int oscIndex, double cents)
{
    if (!std::isfinite(cents)) cents = 0.0;
    if (auto* osc = _oscillator(oscIndex)) osc->setFineTuneCents(std::clamp(cents, -100.0, 100.0));
}

void OscillatorBank::setOscillatorPulseWidth(int oscIndex, double width)
{
    if (auto* osc = _oscillator(oscIndex)) osc->setPulseWidth(width);
}

void OscillatorBank::setOscillator3KeyboardTrackingEnabled(bool enabled)
{
    _osc3.setKeyboardTrackingEnabled(enabled);
    _updatePitch();
}

void OscillatorBank::setAnalogDrift(double maxCents)
{
    _driftMaxCents = std::max(0.0, maxCents);
    if (_driftMaxCents <= 0.0) {
        _driftCents[0] = _driftCents[1] = _driftCents[2] = 0.0;
        _osc1.setDriftCents(0.0);
        _osc2.setDriftCents(0.0);
        _osc3.setDriftCents(0.0);
    }
}

void OscillatorBank::reset()
{
    _osc1.reset();
    _osc2.reset();
    _osc3.reset();
}

OscillatorBankOutput OscillatorBank::process()
{
    // Update analog drift every 512 samples via a simple xorshift PRNG
    if (++_driftCounter >= 512) {
        _driftCounter = 0;
        if (_driftMaxCents > 0.0) {
            _driftRng ^= _driftRng << 13;
            _driftRng ^= _driftRng >> 17;
            _driftRng ^= _driftRng << 5;
            for (int i = 0; i < 3; ++i) {
                const uint32_t r = (_driftRng >> (i * 7)) ^ (_driftRng << (i * 3));
                const double step = (static_cast<double>(static_cast<int32_t>(r)) / 2147483648.0)
                                    * _driftMaxCents * 0.12;
                _driftCents[i] = std::clamp(_driftCents[i] + step,
                                            -_driftMaxCents, _driftMaxCents);
            }
            _osc1.setDriftCents(_driftCents[0]);
            _osc2.setDriftCents(_driftCents[1]);
            _osc3.setDriftCents(_driftCents[2]);
        }
    }
    return { _osc1.processSample(), _osc2.processSample(), _osc3.processSample() };
}

Oscillator* OscillatorBank::_oscillator(int oscIndex)
{
    switch (oscIndex) {
    case 1: return &_osc1;
    case 2: return &_osc2;
    case 3: return &_osc3;
    default: return nullptr;
    }
}

const Oscillator* OscillatorBank::_oscillator(int oscIndex) const
{
    switch (oscIndex) {
    case 1: return &_osc1;
    case 2: return &_osc2;
    case 3: return &_osc3;
    default: return nullptr;
    }
}

void OscillatorBank::_updatePitch()
{
    const double note = _baseMidiNote + _pitchBendSemitones;
    _osc1.setMidiNote(note);
    _osc2.setMidiNote(note);
    _osc3.setMidiNote(note);
}

} // namespace SynthCore
