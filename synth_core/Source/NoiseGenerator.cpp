#include "../Include/NoiseGenerator.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace SynthCore {

void NoiseGenerator::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
}

void NoiseGenerator::setMode(NoiseMode mode)
{
    _mode = mode;
}

void NoiseGenerator::setSeed(uint32_t seed)
{
    _seed = seed == 0u ? 0x12345678u : seed;
    reset();
}

void NoiseGenerator::setRandomSeed()
{
    const auto ticks = static_cast<uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    setSeed(ticks ^ 0x9E3779B9u);
}

void NoiseGenerator::reset()
{
    _state = _seed == 0u ? 0x12345678u : _seed;
    _pink0 = 0.0;
    _pink1 = 0.0;
    _pink2 = 0.0;
}

uint32_t NoiseGenerator::_nextUint()
{
    uint32_t x = _state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    _state = x == 0u ? 0x12345678u : x;
    return _state;
}

double NoiseGenerator::_white()
{
    constexpr double scale = 1.0 / 2147483648.0;
    const int32_t signedValue = static_cast<int32_t>(_nextUint());
    return std::clamp(static_cast<double>(signedValue) * scale, -1.0, 1.0);
}

double NoiseGenerator::processSample()
{
    const double white = _white();
    if (_mode == NoiseMode::White) {
        return white;
    }

    // Simple stable pink-ish approximation: filtered white noise with DC bleed.
    _pink0 = 0.99765 * _pink0 + white * 0.0990460;
    _pink1 = 0.96300 * _pink1 + white * 0.2965164;
    _pink2 = 0.57000 * _pink2 + white * 1.0526913;
    double pink = (_pink0 + _pink1 + _pink2 + white * 0.1848) * 0.18;
    pink = std::clamp(pink, -1.0, 1.0);
    if (!std::isfinite(pink)) {
        reset();
        pink = 0.0;
    }
    return pink;
}

} // namespace SynthCore
