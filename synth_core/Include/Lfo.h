#pragma once
#include <cmath>
#include <algorithm>

namespace SynthCore {

// Simple sine LFO — runs at audio rate, output in [-1, +1].
class Lfo {
public:
    void setSampleRate(double sampleRate) { _sampleRate = sampleRate; }
    void setRate(double hz) { _rate = std::max(0.001, hz); }
    void reset() { _phase = 0.0; }

    double processSample()
    {
        const double out = std::sin(_phase * (2.0 * 3.14159265358979323846));
        _phase += _rate / _sampleRate;
        if (_phase >= 1.0) _phase -= 1.0;
        return out;
    }

private:
    double _sampleRate = 44100.0;
    double _rate       = 1.0;
    double _phase      = 0.0;
};

} // namespace SynthCore
