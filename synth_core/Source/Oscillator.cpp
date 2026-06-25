#include "../Include/Oscillator.h"

namespace SynthCore {

void Oscillator::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
}

void Oscillator::setFrequency(double hz)
{
    _frequency = hz;
}

void Oscillator::setWaveform(OscWaveform w)
{
    _waveform = w;
}

void Oscillator::setOctave(int semitoneOffset)
{
    _octaveOffset = semitoneOffset;
}

float Oscillator::process()
{
    // Milestone 1: linear ramp integration and waveform generation
    return 0.0f;
}

void Oscillator::reset()
{
    _phase = 0.0;
}

} // namespace SynthCore
