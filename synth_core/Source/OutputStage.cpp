#include "../Include/OutputStage.h"
#include <cmath>

namespace SynthCore {

void OutputStage::setSampleRate(double sampleRate)
{
    _sampleRate = sampleRate;
    _updateHpCoeff();
}

void OutputStage::setCapLarge(bool large)
{
    _capLarge = large;
    _updateHpCoeff();
}

float OutputStage::process(float in)
{
    // Milestone 8: Class-A saturation + 1-pole HP coupling filter
    return in;
}

void OutputStage::reset()
{
    _hpState = 0.0f;
}

void OutputStage::_updateHpCoeff()
{
    // R = 10 kΩ (typical output load), C = 10µF or 47µF
    // fc = 1/(2π·R·C)
    constexpr double R = 10000.0;
    double C = _capLarge ? 47e-6 : 10e-6;
    double fc = 1.0 / (2.0 * 3.14159265358979 * R * C);
    if (_sampleRate > 0.0)
        _hpCoeff = static_cast<float>(std::exp(-2.0 * 3.14159265358979 * fc / _sampleRate));
    else
        _hpCoeff = 0.0f;
}

} // namespace SynthCore
