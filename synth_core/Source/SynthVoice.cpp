#include "../Include/SynthVoice.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

void SynthVoice::prepare(double sampleRate, int /*blockSize*/)
{
    _sampleRate = sampleRate;
    oscillators.setSampleRate(sampleRate);
    noise.setSampleRate(sampleRate);
    mixer.setSampleRate(sampleRate);
    ladderFilter.setSampleRate(sampleRate);
    loudnessContour.setSampleRate(sampleRate);
    filterContour.setSampleRate(sampleRate);
    vca.setSampleRate(sampleRate);
}

void SynthVoice::noteOn(int /*midiNote*/, float /*velocity*/)
{
    loudnessContour.gateOn();
    filterContour.gateOn();
}

void SynthVoice::noteOff()
{
    loudnessContour.gateOff();
    filterContour.gateOff();
}

float SynthVoice::processSample(double pitchHz, double currentMidiNote)
{
    oscillators.setBaseMidiNote(_midiFromHz(pitchHz));
    const auto osc           = oscillators.process();
    const double noiseOut    = noise.processSample();
    const double mixed       = mixer.processSample(osc.osc1, osc.osc2, osc.osc3, noiseOut, 0.0);
    const double loudness    = loudnessContour.processSample();
    const double filterEnv   = filterContour.processSample();
    const double filtered    = ladderFilter.processSample(mixed, filterEnv, currentMidiNote);
    const double amplified   = vca.processSample(filtered, loudness);
    return static_cast<float>(amplified);
}

bool SynthVoice::isActive() const
{
    return loudnessContour.isActive();
}

void SynthVoice::reset()
{
    oscillators.reset();
    noise.reset();
    mixer.reset();
    ladderFilter.reset();
    loudnessContour.reset();
    filterContour.reset();
    vca.reset();
}

double SynthVoice::_midiFromHz(double hz)
{
    if (!std::isfinite(hz) || hz <= 0.0) return 69.0;
    return 69.0 + 12.0 * std::log2(hz / 440.0);
}

} // namespace SynthCore
