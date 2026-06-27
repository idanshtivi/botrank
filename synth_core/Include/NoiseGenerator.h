#pragma once

#include <cstdint>

namespace SynthCore {

enum class NoiseMode {
    White,
    Pink
};

class NoiseGenerator {
public:
    void setSampleRate(double sampleRate);
    void setMode(NoiseMode mode);
    void setSeed(uint32_t seed);
    void setRandomSeed();
    void reset();
    double processSample();

private:
    double _sampleRate = 44100.0;
    NoiseMode _mode = NoiseMode::White;
    uint32_t _seed = 0x12345678u;
    uint32_t _state = 0x12345678u;
    double _pink0 = 0.0;
    double _pink1 = 0.0;
    double _pink2 = 0.0;

    uint32_t _nextUint();
    double _white();
};

} // namespace SynthCore
