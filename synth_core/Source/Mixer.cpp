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
    constexpr double kDriveTau = 0.020; // 20 ms — fast enough to not feel laggy
    _driveAlpha    = std::exp(-1.0 / (kDriveTau * _sampleRate));
    _driveNeedsSnap = true; // re-snap on next processSample so new drive takes effect cleanly
}

void Mixer::setSourceEnabled(MixerSource source, bool enabled)
{
    _enabled[_index(source)] = enabled;
}

void Mixer::setSourceLevel(MixerSource source, double level)
{
    if (!std::isfinite(level)) level = 0.0;
    _levels[_index(source)] = std::clamp(level, 0.0, 1.0);
}

void Mixer::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 1.0;
    _drive = std::clamp(drive, 0.0, 3.0);
}

void Mixer::reset()
{
    _driveNeedsSnap = true; // next processSample will snap to the post-reset _drive value
}

double Mixer::processSample(double osc1, double osc2, double osc3, double noise, double ext)
{
    const double inputs[static_cast<int>(MixerSource::Count)] = {osc1, osc2, osc3, noise, ext};
    double sum = 0.0;

    for (int i = 0; i < static_cast<int>(MixerSource::Count); ++i) {
        const double sample = std::isfinite(inputs[i]) ? inputs[i] : 0.0;
        if (_enabled[i]) {
            sum += sample * _levels[i];
        }
    }

    constexpr double headroom = 0.45;
    const double mixerSum = sum * headroom;

    // Snap on first call after init/reset so pre-play parameter changes take effect
    // immediately.  During playback (subsequent calls) use the 1-pole LP to avoid
    // zipper noise when the drive knob is turned.
    if (_driveNeedsSnap) {
        _driveSmoothed  = _drive;
        _driveNeedsSnap = false;
    } else {
        _driveSmoothed = _driveAlpha * _driveSmoothed + (1.0 - _driveAlpha) * _drive;
    }
    const double md       = DriveUtils::normDrive(_driveSmoothed);
    const double driven   = DriveUtils::mixerDriveSaturate(mixerSum, md);
    // pow(md, 0.78) bows the blend curve toward Drive 1 (≈37% wet at Drive 1 vs V4's ≈31%);
    // no floor so Drive 0 remains bit-clean.
    const double driveMix = DriveUtils::smoothstep01(0.015, 1.0, std::pow(md, 0.78));
    double out = DriveUtils::lerp(mixerSum, driven, driveMix);

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
