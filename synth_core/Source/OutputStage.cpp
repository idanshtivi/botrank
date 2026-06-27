#include "../Include/OutputStage.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

static constexpr double kPi = 3.14159265358979323846;

void OutputStage::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
    _updateHpCoeff();
}

void OutputStage::setMasterVolume(double volume)
{
    if (!std::isfinite(volume)) volume = 0.0;
    _masterVolume = std::clamp(volume, 0.0, 1.0);
}

void OutputStage::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 0.0;
    _drive = std::clamp(drive, 0.0, 4.0);
}

void OutputStage::setReferenceToneEnabled(bool enabled)
{
    _referenceToneEnabled = enabled;
}

void OutputStage::setCapLarge(bool large)
{
    _capLarge = large;
    _updateHpCoeff();
}

double OutputStage::processSample(double input)
{
    double x = input;
    if (_referenceToneEnabled) {
        x = 0.2 * std::sin(2.0 * kPi * _referencePhase);
        _referencePhase += 440.0 / _sampleRate;
        if (_referencePhase >= 1.0) _referencePhase -= 1.0;
    }

    if (!std::isfinite(x)) x = 0.0;
    x *= _masterVolume;

    const double dcBlocked = x - _dcPrevInput + _hpCoeff * _dcPrevOutput;
    _dcPrevInput = x;
    _dcPrevOutput = dcBlocked;

    double out = dcBlocked;
    if (_drive > 0.0001) {
        const double denom = std::tanh(_drive);
        out = denom > 0.0001 ? std::tanh(_drive * out) / denom : out;
    }

    out = std::tanh(out);
    if (!std::isfinite(out)) out = 0.0;
    return std::clamp(out, -1.0, 1.0);
}

float OutputStage::process(float in)
{
    return static_cast<float>(processSample(in));
}

void OutputStage::reset()
{
    _dcPrevInput = 0.0;
    _dcPrevOutput = 0.0;
    _referencePhase = 0.0;
}

void OutputStage::_updateHpCoeff()
{
    const double cutoff = _capLarge ? 5.0 : 12.0;
    _hpCoeff = std::exp(-2.0 * kPi * cutoff / _sampleRate);
    _hpCoeff = std::clamp(_hpCoeff, 0.0, 0.9999);
}

} // namespace SynthCore
