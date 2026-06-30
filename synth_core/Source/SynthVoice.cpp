#include "../Include/SynthVoice.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

void SynthVoice::prepare(double sampleRate, int /*blockSize*/)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
    oscillators.setSampleRate(_sampleRate);
    noise.setSampleRate(_sampleRate);
    mixer.setSampleRate(_sampleRate);
    ladderFilter.setSampleRate(_sampleRate);
    loudnessContour.setSampleRate(_sampleRate);
    filterContour.setSampleRate(_sampleRate);
    vca.setSampleRate(_sampleRate);
    _dcBlockCoeff      = std::exp(-2.0 * 3.14159265358979323846 * 5.0 / _sampleRate);
    _pitchDeclickCoeff = std::exp(-1.0 / (0.0015 * _sampleRate));
    _pitchSmoothedHz   = 440.0;
    _pitchIsFirstSample = false;
    _startDeclickStep  = 1.0 / (0.002 * _sampleRate);
}

int SynthVoice::_fadeSamplesForMs(double ms) const
{
    return std::max(1, static_cast<int>(std::lround(_sampleRate * ms / 1000.0)));
}

void SynthVoice::_beginStolenReleaseRestart()
{
    _stealResidual = std::isfinite(_lastOutput) ? _lastOutput : 0.0;
    _stealResidualTotalSamples = _fadeSamplesForMs(3.0);
    _stealResidualSamplesRemaining = _stealResidualTotalSamples;

    ladderFilter.reset();
    loudnessContour.reset();
    filterContour.reset();
    vca.reset();
    _dcBlockX = 0.0;
    _dcBlockY = 0.0;
    _pitchIsFirstSample = true;
    _startDeclickGain = 0.0;
    _startDeclickStep = 1.0 / static_cast<double>(_fadeSamplesForMs(3.0));
}

void SynthVoice::noteOn(int /*midiNote*/, float /*velocity*/, VoiceStartMode mode)
{
    const bool wasActive = loudnessContour.isActive();
    if (mode == VoiceStartMode::StolenRelease)
        _beginStolenReleaseRestart();

    loudnessContour.gateOn();
    filterContour.gateOn();
    if (mode == VoiceStartMode::Normal && !wasActive) {
        _startDeclickGain = 0.0;
        _startDeclickStep = 1.0 / (0.002 * _sampleRate);
    }
}

void SynthVoice::noteOff()
{
    loudnessContour.gateOff();
    filterContour.gateOff();
}

float SynthVoice::processSample(double pitchHz, double currentMidiNote)
{
    // When the loudness envelope is idle the VCA output is zero regardless.
    // Track the target pitch so the smoother is ready at the correct frequency
    // when the voice activates, then skip the filter to prevent state accumulation.
    if (!loudnessContour.isActive()) {
        _pitchSmoothedHz = pitchHz;
        oscillators.setBaseMidiNote(_midiFromHz(pitchHz));
        oscillators.process();
        loudnessContour.processSample();
        filterContour.processSample();
        _lastOutput = 0.0;
        return 0.0f;
    }

    // Pitch de-click: snap on the first active sample of a fresh note start so there
    // is no audible pitch ramp; smooth over ~1.5 ms on active transitions to eliminate
    // the PolyBLEP correction discontinuity that occurs when frequency changes instantly
    // while the VCA is open (mono retrigger, legato, poly voice stealing).
    if (_pitchIsFirstSample) {
        _pitchSmoothedHz    = pitchHz;
        _pitchIsFirstSample = false;
    } else {
        _pitchSmoothedHz = _pitchDeclickCoeff * _pitchSmoothedHz
                         + (1.0 - _pitchDeclickCoeff) * pitchHz;
    }

    oscillators.setBaseMidiNote(_midiFromHz(_pitchSmoothedHz));

    const auto osc        = oscillators.process();
    const double noiseOut = noise.processSample();
    const double mixed    = mixer.processSample(osc.osc1, osc.osc2, osc.osc3, noiseOut, 0.0);

    // Remove DC added by drive asymmetry (x² terms in saturation).
    _dcBlockY = _dcBlockCoeff * (_dcBlockY + mixed - _dcBlockX);
    _dcBlockX = mixed;
    const double dcFree   = _dcBlockY;

    const double loudness  = loudnessContour.processSample();
    const double filterEnv = filterContour.processSample();
    const double filtered  = ladderFilter.processSample(dcFree, filterEnv, currentMidiNote);
    double amplified = vca.processSample(filtered, loudness);
    if (_startDeclickGain < 1.0) {
        amplified *= _startDeclickGain;
        _startDeclickGain = std::min(1.0, _startDeclickGain + _startDeclickStep);
    }
    if (_stealResidualSamplesRemaining > 0 && _stealResidualTotalSamples > 0) {
        const double t = static_cast<double>(_stealResidualSamplesRemaining)
                       / static_cast<double>(_stealResidualTotalSamples);
        amplified += _stealResidual * t;
        --_stealResidualSamplesRemaining;
    }
    if (!std::isfinite(amplified))
        amplified = 0.0;
    _lastOutput = amplified;
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
    _dcBlockX = 0.0;
    _dcBlockY = 0.0;
    _pitchSmoothedHz    = 440.0;
    _pitchIsFirstSample = true;
    _startDeclickGain   = 1.0;
    _startDeclickStep   = 1.0 / (0.002 * _sampleRate);
    _lastOutput = 0.0;
    _stealResidual = 0.0;
    _stealResidualSamplesRemaining = 0;
    _stealResidualTotalSamples = 0;
}

void SynthVoice::resetAudioChainState()
{
    ladderFilter.reset();
    _dcBlockX = 0.0;
    _dcBlockY = 0.0;
    _pitchIsFirstSample = true;
}

double SynthVoice::_midiFromHz(double hz)
{
    if (!std::isfinite(hz) || hz <= 0.0) return 69.0;
    return 69.0 + 12.0 * std::log2(hz / 440.0);
}

} // namespace SynthCore
