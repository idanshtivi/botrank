#include "../Include/Mixer.h"
#include "../Include/DSPUtils.h"
#include <algorithm>
#include <cmath>

namespace SynthCore {

int Mixer::_index(MixerSource source)
{
    const int idx = static_cast<int>(source);
    return (idx >= 0 && idx < static_cast<int>(MixerSource::Count)) ? idx : 0;
}

void Mixer::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    constexpr double kLevelTau = 0.008; // 8 ms: smooth gain moves without making level knobs feel laggy
    _levelAlpha = std::exp(-1.0 / (kLevelTau * _sampleRate));
    _levelsNeedSnap = true;
    constexpr double kDriveTau = 0.020; // 20 ms: fast enough to not feel laggy
    _driveAlpha = std::exp(-1.0 / (kDriveTau * _sampleRate));
    _driveNeedsSnap = true; // re-snap on next processSample so new drive takes effect cleanly
}

void Mixer::setSourceEnabled(MixerSource source, bool enabled)
{
    _enabled[_index(source)] = enabled;
}

void Mixer::setSourceLevel(MixerSource source, double level)
{
    if (!std::isfinite(level)) level = 0.0;
    _targetLevels[_index(source)] = std::clamp(level, 0.0, 1.0);
}

void Mixer::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 1.0;
    _drive = std::clamp(drive, 0.0, 3.0);
}

void Mixer::reset()
{
    _levelsNeedSnap = true; // next processSample will snap to the post-reset source levels
    _driveNeedsSnap = true; // next processSample will snap to the post-reset _drive value
}

double Mixer::processSample(double osc1, double osc2, double osc3, double noise, double ext)
{
    const double inputs[static_cast<int>(MixerSource::Count)] = {osc1, osc2, osc3, noise, ext};
    double sum = 0.0;
    double activeLevel = 0.0;

    if (_levelsNeedSnap) {
        for (int i = 0; i < static_cast<int>(MixerSource::Count); ++i)
            _smoothedLevels[i] = _targetLevels[i];
        _levelsNeedSnap = false;
    } else {
        for (int i = 0; i < static_cast<int>(MixerSource::Count); ++i)
            _smoothedLevels[i] = _levelAlpha * _smoothedLevels[i] + (1.0 - _levelAlpha) * _targetLevels[i];
    }

    for (int i = 0; i < static_cast<int>(MixerSource::Count); ++i) {
        const double sample = std::isfinite(inputs[i]) ? inputs[i] : 0.0;
        if (_enabled[i]) {
            activeLevel = std::max(activeLevel, _smoothedLevels[i]);
            sum += sample * _smoothedLevels[i];
        }
    }

    // Snap on first call after init/reset so pre-play parameter changes take effect
    // immediately. During playback, use the 1-pole LP to avoid zipper noise.
    if (_driveNeedsSnap) {
        _driveSmoothed = _drive;
        _driveNeedsSnap = false;
    } else {
        _driveSmoothed = _driveAlpha * _driveSmoothed + (1.0 - _driveAlpha) * _drive;
    }

    const double md = DriveUtils::normDrive(_driveSmoothed);
    const double headroom = DriveUtils::lerp(0.55, 0.78, std::pow(md, 0.65));
    const double mixerSum = sum * headroom;
    const double driven = DriveUtils::mixerDriveSaturate(mixerSum, md);

    // Progressive exciter blend, but tied to the source Level control. A low
    // oscillator level must stay a low-volume signal; it should not be lifted to
    // near-full volume by the drive shaper. Higher levels progressively wake the
    // saturation so the level controls still push the mixer/ladder harder.
    const double driveMix = DriveUtils::smoothstep01(0.0, 1.0, std::pow(md, 0.52));
    const double levelDriveWake = DriveUtils::smoothstep01(0.20, 0.88, activeLevel);
    const double levelAwareDriveMix = driveMix * levelDriveWake;
    const double maxDrivenLift = DriveUtils::lerp(1.08, 3.30, levelDriveWake);
    const double drivenLimit = std::abs(mixerSum) * maxDrivenLift + 1.0e-6;
    const double levelBoundedDriven = std::clamp(driven, -drivenLimit, drivenLimit);
    double out = DriveUtils::lerp(mixerSum, levelBoundedDriven, levelAwareDriveMix);

    if (!std::isfinite(out)) out = 0.0;
    return std::clamp(out, -1.0, 1.0);
}

void Mixer::setOsc1Level(float v)  { setSourceLevel(MixerSource::Osc1, v); }
void Mixer::setOsc2Level(float v)  { setSourceLevel(MixerSource::Osc2, v); }
void Mixer::setOsc3Level(float v)  { setSourceLevel(MixerSource::Osc3, v); }
void Mixer::setNoiseLevel(float v) { setSourceLevel(MixerSource::Noise, v); }
void Mixer::setExtLevel(float v)   { setSourceLevel(MixerSource::ExternalInput, v); }

float Mixer::process(float osc1, float osc2, float osc3, float noise, float ext)
{
    return static_cast<float>(processSample(osc1, osc2, osc3, noise, ext));
}

} // namespace SynthCore
