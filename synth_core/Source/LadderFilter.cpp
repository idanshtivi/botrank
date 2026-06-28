#include "../Include/LadderFilter.h"
#include "../Include/DSPUtils.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

static constexpr double kPi = 3.14159265358979323846;

void LadderFilter::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 1000.0) ? sampleRate : 44100.0;
    setCutoffHz(_cutoffHz);
}

void LadderFilter::setCutoffHz(double cutoffHz)
{
    if (!std::isfinite(cutoffHz)) cutoffHz = 1000.0;
    const double maxCutoff = std::max(20.0, _sampleRate * 0.45);
    _cutoffHz = std::clamp(cutoffHz, 20.0, std::min(20000.0, maxCutoff));
}

void LadderFilter::setResonance(double resonance)
{
    if (!std::isfinite(resonance)) resonance = 0.0;
    _resonance = std::clamp(resonance, 0.0, 1.0);
}

void LadderFilter::setContourAmount(double amount)
{
    if (!std::isfinite(amount)) amount = 0.0;
    _contourAmount = std::clamp(amount, 0.0, 1.0);
}

void LadderFilter::setKeyboardTrackingAmount(double amount)
{
    if (!std::isfinite(amount)) amount = 0.0;
    _keyboardTrackingAmount = std::clamp(amount, 0.0, 1.0);
}

void LadderFilter::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 0.0;
    _drive = std::clamp(drive, 0.0, 3.0);
}

void LadderFilter::reset()
{
    for (double& s : _stage) s = 0.0;
}

double LadderFilter::_effectiveCutoff(double filterEnvelopeValue, double keyboardMidiNote) const
{
    const double env = std::clamp(std::isfinite(filterEnvelopeValue) ? filterEnvelopeValue : 0.0, 0.0, 1.0);
    const double note = std::clamp(std::isfinite(keyboardMidiNote) ? keyboardMidiNote : 60.0, 0.0, 127.0);

    const double envOctaves = env * _contourAmount * 4.0;
    const double keyboardOctaves = ((note - 60.0) / 12.0) * _keyboardTrackingAmount;
    double cutoff = _cutoffHz * std::pow(2.0, envOctaves + keyboardOctaves);
    cutoff = std::clamp(cutoff, 20.0, std::min(20000.0, _sampleRate * 0.45));
    return cutoff;
}

double LadderFilter::processSample(double input, double filterEnvelopeValue, double keyboardMidiNote)
{
    double x = std::isfinite(input) ? input : 0.0;

    // Stable filter input push — no feedbackReturn, no sweep, no self-oscillation change.
    const double fd = DriveUtils::normDrive(_drive);
    if (fd > 0.001) {
        const double inputGain  = 1.0 + 2.8 * std::pow(fd, 1.20);
        const double filterInput = x * inputGain;
        x = DriveUtils::mainDriveSaturate(filterInput, 0.55 * fd);
    }

    const double cutoff = _effectiveCutoff(filterEnvelopeValue, keyboardMidiNote);
    const double g = 1.0 - std::exp(-2.0 * kPi * cutoff / _sampleRate);
    const double feedback = _resonance * 3.2;

    double u = x - feedback * _stage[3];
    u = std::clamp(u, -4.0, 4.0);
    for (double& s : _stage) {
        s += g * (std::tanh(u) - std::tanh(s));
        s = std::clamp(s, -2.0, 2.0);
        u = s;
    }

    double out = _stage[3];
    if (!std::isfinite(out)) {
        reset();
        out = 0.0;
    }
    return std::clamp(out, -1.0, 1.0);
}

} // namespace SynthCore
