#include "../Include/ContourGenerator.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

double ContourGenerator::_clampTime(double seconds)
{
    if (!std::isfinite(seconds)) seconds = 0.001;
    return std::clamp(seconds, 0.001, 10.0);
}

double ContourGenerator::_stepFor(double seconds) const
{
    if (_sampleRate <= 0.0) return 1.0;
    return 1.0 / (_clampTime(seconds) * _sampleRate);
}

void ContourGenerator::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
    constexpr double kSustainRampTau = 0.008; // 8ms — matches Mixer's level smoother
    _sustainRampCoeff = std::exp(-1.0 / (kSustainRampTau * _sampleRate));
}

void ContourGenerator::setAttackSeconds(double seconds)
{
    _attackSeconds = _clampTime(seconds);
}

void ContourGenerator::setDecaySeconds(double seconds)
{
    _decaySeconds = _clampTime(seconds);
}

void ContourGenerator::setSustainLevel(double level)
{
    if (!std::isfinite(level)) level = 0.0;
    _sustainLevel = std::clamp(level, 0.0, 1.0);
}

void ContourGenerator::setReleaseSeconds(double seconds)
{
    _releaseSeconds = _clampTime(seconds);
}

void ContourGenerator::setDecayActsAsRelease(bool enabled)
{
    _decayActsAsRelease = enabled;
}

void ContourGenerator::gateOn()
{
    _gateHigh = true;
    _stage = Stage::Attack;
}

void ContourGenerator::gateOff()
{
    _gateHigh = false;
    if (_stage != Stage::Idle) {
        _stage = Stage::Release;
    }
}

void ContourGenerator::trigger()
{
    gateOn();
}

void ContourGenerator::reset()
{
    _gateHigh = false;
    _value = 0.0;
    _stage = Stage::Idle;
}

double ContourGenerator::processSample()
{
    switch (_stage) {
    case Stage::Idle:
        _value = 0.0;
        break;
    case Stage::Attack:
        _value += _stepFor(_attackSeconds);
        if (_value >= 1.0) {
            _value = 1.0;
            _stage = Stage::Decay;
        }
        break;
    case Stage::Decay:
        _value -= _stepFor(_decaySeconds);
        if (_value <= _sustainLevel) {
            _value = _sustainLevel;
            _stage = _gateHigh ? Stage::Sustain : Stage::Release;
        }
        break;
    case Stage::Sustain:
        _value = _sustainRampCoeff * _value + (1.0 - _sustainRampCoeff) * _sustainLevel;
        if (!_gateHigh) {
            _stage = Stage::Release;
        }
        break;
    case Stage::Release: {
        const double releaseTime = _decayActsAsRelease ? _decaySeconds : _releaseSeconds;
        _value -= _stepFor(releaseTime);
        if (_value <= 0.0) {
            _value = 0.0;
            _stage = Stage::Idle;
        }
        break;
    }
    }

    if (!std::isfinite(_value)) {
        _value = 0.0;
        _stage = Stage::Idle;
    }
    _value = std::clamp(_value, 0.0, 1.0);
    return _value;
}

bool ContourGenerator::isActive() const
{
    return _stage != Stage::Idle || _value > 0.0;
}

} // namespace SynthCore
