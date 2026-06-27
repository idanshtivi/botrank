#include "../Include/Oscillator.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

static double polyBlep(double t, double dt)
{
    if (dt <= 0.0) return 0.0;
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

Oscillator::Oscillator()
{
    _updatePhaseInc();
}

void Oscillator::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
    _updatePhaseInc();
}

void Oscillator::setFrequency(double hz)
{
    _frequency = (std::isfinite(hz) && hz > 0.0) ? hz : 0.0;
    _updatePhaseInc();
}

void Oscillator::setMidiNote(double note)
{
    if (!std::isfinite(note)) note = 69.0;
    _baseMidiNote = std::clamp(note, 0.0, 127.0);
    if (_keyboardTrackingEnabled) {
        _frequency = 440.0 * std::pow(2.0, (_baseMidiNote - 69.0) / 12.0);
    }
    _updatePhaseInc();
}

void Oscillator::setWaveform(Waveform w)
{
    _waveform = w;
}

void Oscillator::setRange(OscillatorRange range)
{
    _range = range;
    _updatePhaseInc();
}

void Oscillator::setDetuneSemitones(double semitones)
{
    if (!std::isfinite(semitones)) semitones = 0.0;
    _detuneSemitones = std::clamp(semitones, -24.0, 24.0);
    _updatePhaseInc();
}

void Oscillator::setFineTuneCents(double cents)
{
    if (!std::isfinite(cents)) cents = 0.0;
    _fineTuneCents = std::clamp(cents, -1200.0, 1200.0);
    _updatePhaseInc();
}

void Oscillator::setPulseWidth(double width)
{
    if (!std::isfinite(width)) width = 0.5;
    _pulseWidth = std::clamp(width, 0.10, 0.90);
}

void Oscillator::setDriftCents(double cents)
{
    if (!std::isfinite(cents)) cents = 0.0;
    _driftCents = std::clamp(cents, -50.0, 50.0);
    _updatePhaseInc();
}

void Oscillator::setKeyboardTrackingEnabled(bool enabled)
{
    _keyboardTrackingEnabled = enabled;
    if (enabled) {
        setMidiNote(_baseMidiNote);
    }
}

void Oscillator::setOctave(int semitoneOffset)
{
    _octaveOffset = std::clamp(semitoneOffset, -48, 48);
    _updatePhaseInc();
}

int Oscillator::_rangeSemitones(OscillatorRange range)
{
    switch (range) {
    case OscillatorRange::Low: return -48;
    case OscillatorRange::ThirtyTwoFoot: return -24;
    case OscillatorRange::SixteenFoot: return -12;
    case OscillatorRange::EightFoot: return 0;
    case OscillatorRange::FourFoot: return 12;
    case OscillatorRange::TwoFoot: return 24;
    }
    return 0;
}

double Oscillator::_effectiveFrequency() const
{
    const double semitones = static_cast<double>(_octaveOffset + _rangeSemitones(_range))
        + _detuneSemitones + (_fineTuneCents / 100.0) + (_driftCents / 100.0);
    const double hz = _frequency * std::pow(2.0, semitones / 12.0);
    return (std::isfinite(hz) && hz > 0.0) ? hz : 0.0;
}

void Oscillator::_updatePhaseInc()
{
    if (_sampleRate <= 0.0) {
        _phaseInc = 0.0;
        return;
    }

    const double effectiveHz = std::min(_effectiveFrequency(), _sampleRate * 0.45);
    _phaseInc = std::clamp(effectiveHz / _sampleRate, 0.0, 0.5);
}

float Oscillator::processSample()
{
    _phase += _phaseInc;
    if (_phase >= 1.0) _phase -= 1.0;

    double saw = _phase * 2.0 - 1.0;
    saw -= polyBlep(_phase, _phaseInc);

    auto pulse = [this](double width) {
        width = std::clamp(width, 0.10, 0.90);
        double y = _phase < width ? 1.0 : -1.0;
        y += polyBlep(_phase, _phaseInc);
        double t2 = _phase - width;
        if (t2 < 0.0) t2 += 1.0;
        y -= polyBlep(t2, _phaseInc);
        return y;
    };

    double out = 0.0;
    switch (_waveform) {
    case Waveform::Saw:
        out = saw;
        break;
    case Waveform::ReverseSaw:
        out = -saw;
        break;
    case Waveform::Square:
        out = pulse(_pulseWidth);
        break;
    case Waveform::WidePulse:
        out = pulse(std::clamp(_pulseWidth + 0.15, 0.10, 0.90));
        break;
    case Waveform::NarrowPulse:
        out = pulse(std::clamp(_pulseWidth - 0.25, 0.10, 0.90));
        break;
    case Waveform::Triangle: {
        const double square = pulse(0.5);
        _triangleState += square * _phaseInc * 4.0;
        _triangleState *= 0.9995;
        out = _triangleState;
        break;
    }
    case Waveform::TriangleSaw: {
        const double triangle = _phase < 0.5 ? (4.0 * _phase - 1.0) : (3.0 - 4.0 * _phase);
        out = 0.5 * triangle + 0.5 * saw;
        break;
    }
    }

    if (!std::isfinite(out)) out = 0.0;
    return static_cast<float>(std::clamp(out, -1.0, 1.0));
}

float Oscillator::process()
{
    return processSample();
}

void Oscillator::reset()
{
    _phase = 0.0;
    _triangleState = 0.0;
}

} // namespace SynthCore
