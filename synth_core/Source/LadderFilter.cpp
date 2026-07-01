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

void LadderFilter::setMainDrivePush(double drive)
{
    if (!std::isfinite(drive)) drive = 0.0;
    _mainDrivePush = std::clamp(drive, 0.0, 3.0);
}

void LadderFilter::reset()
{
    for (double& s : _stage) s = 0.0;
    _driveBodyLp = 0.0;
    _driveGrowlLp = 0.0;
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
    const double panelDrive = DriveUtils::normDrive(_drive);
    const double mainPush = DriveUtils::normDrive(_mainDrivePush);
    const double mainPushCurve = std::pow(mainPush, 2.2);
    const double fd = DriveUtils::clamp01(panelDrive + 0.78 * mainPushCurve * (1.0 - 0.25 * panelDrive));

    if (fd > 0.001) {
        const double inputPush = 1.0 + 2.15 * std::pow(fd, 0.62);
        x = DriveUtils::filterDriveSaturate(x * inputPush, fd);

        const double bodyCutoff = 360.0;
        const double bodyG = 1.0 - std::exp(-2.0 * kPi * bodyCutoff / _sampleRate);
        _driveBodyLp += bodyG * (x - _driveBodyLp);

        const double body = _driveBodyLp;
        const double upper = x - body;
        const double growlDrive = 1.0 + 3.25 * std::pow(fd, 0.70);
        const double growledBody = std::tanh(body * growlDrive) / growlDrive;
        const double bodyDelta = growledBody - body;
        _driveGrowlLp += bodyG * (bodyDelta - _driveGrowlLp);

        const double highTame = 1.0 - 0.18 * fd;
        const double growlAmount = 0.55 * std::pow(fd, 0.68);
        x = body + upper * highTame + _driveGrowlLp * growlAmount;
    }

    const double cutoff = _effectiveCutoff(filterEnvelopeValue, keyboardMidiNote);
    const double g = 1.0 - std::exp(-2.0 * kPi * cutoff / _sampleRate);
    const double feedback = _resonance * 3.2;

    const double fbDrive = 1.0 + 2.25 * fd;
    const double fbRaw = _stage[3];
    const double fbReturn = std::tanh(fbRaw * fbDrive) / fbDrive;
    const double fbBody = 0.08 * fd * _stage[2];

    double u = x - feedback * (fbReturn + fbBody);
    const double ladderInputDrive = 1.0 + 0.85 * fd;
    u = std::tanh(u * ladderInputDrive) / ladderInputDrive;
    u = std::clamp(u, -4.5, 4.5);

    const double stageSat = 1.0 + 0.45 * fd;
    for (double& s : _stage) {
        const double drivenU = std::tanh(stageSat * u) / stageSat;
        const double drivenS = std::tanh(stageSat * s) / stageSat;

        s += g * (drivenU - drivenS);
        s = std::clamp(s, -2.2, 2.2);
        u = s;
    }

    double out = _stage[3];
    const double bodyKeep = 0.16 * std::pow(fd, 0.70);
    out = DriveUtils::lerp(out, DriveUtils::softLimit(out * (1.0 + 0.65 * fd), 1.05), bodyKeep);

    if (!std::isfinite(out)) {
        reset();
        out = 0.0;
    }
    return std::clamp(out, -1.0, 1.0);
}

} // namespace SynthCore
