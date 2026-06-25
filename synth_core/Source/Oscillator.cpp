#include "../Include/Oscillator.h"
#include <cmath>
#include <cassert>

namespace SynthCore {

// ── helpers ───────────────────────────────────────────────────────────────────

static constexpr double kTwoPi    = 6.283185307179586476925;
static constexpr double kMinFreq  = 8.0;      // below sub-bass: clamp
static constexpr double kMaxFreqRatio = 0.499; // never exceed Nyquist

void Oscillator::_updatePhaseInc()
{
    // Apply semitone transpose: f_eff = f_base * 2^(semitones/12)
    double f = _frequency * std::pow(2.0, _semitones / 12.0);

    // Hard clamp: no DC, no aliasing regardless of caller input
    if (f < kMinFreq) f = kMinFreq;
    double nyquist = _sampleRate * kMaxFreqRatio;
    if (f > nyquist) f = nyquist;

    _phaseInc = f / _sampleRate;
}

// ── PolyBLEP kernel ───────────────────────────────────────────────────────────
//
// Corrects the ideal sawtooth discontinuity at the wrap point.
// t  : fractional position within [0,1) — t=0 is exactly at the reset edge
// dt : phaseIncrement (= f/fs, one sample width in normalised phase)
//
// Returns a correction value to subtract from the naive waveform.
// The kernel covers one sample on each side of the discontinuity.
//
double Oscillator::_polyBlep(double t, double dt)
{
    if (t < dt) {
        // One sample after the discontinuity
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        // One sample before the discontinuity
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

// ── public interface ──────────────────────────────────────────────────────────

void Oscillator::setSampleRate(double sampleRate)
{
    assert(sampleRate > 0.0);
    _sampleRate = sampleRate;
    _updatePhaseInc();
}

void Oscillator::setFrequency(double hz)
{
    assert(hz > 0.0);
    _frequency = hz;
    _updatePhaseInc();
}

void Oscillator::setOctave(int semitoneOffset)
{
    _semitones = semitoneOffset;
    _updatePhaseInc();
}

void Oscillator::setWaveform(OscWaveform w)
{
    _waveform = w;
}

void Oscillator::reset()
{
    _phase = 0.0;
}

float Oscillator::process()
{
    // Naive sawtooth: ramps from -1 to +1 over [0, 1), then wraps.
    // Represented in normalised phase [0, 1).
    double naive = 2.0 * _phase - 1.0;

    // Apply PolyBLEP correction at the wrap discontinuity.
    naive -= _polyBlep(_phase, _phaseInc);

    // Advance phase — no conditional branch, no drift accumulation.
    _phase += _phaseInc;
    if (_phase >= 1.0) _phase -= 1.0;

    return static_cast<float>(naive);
}

} // namespace SynthCore
