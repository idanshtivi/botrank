# Drive Code For Review

Current drive-related implementation lives mainly in:

- `synth_core/Include/DSPUtils.h`
- `synth_core/Source/Mixer.cpp`
- `synth_core/Source/LadderFilter.cpp`
- `synth_core/Source/OutputStage.cpp`
- `synth_core/Source/SynthEngine.cpp`

## Current Synth Architecture

The synth is organized as a `SynthEngine` that owns one mono voice, four poly
voices, an LFO, a voice controller, and one final output stage.

```text
Plugin/UI/APVTS parameters
        |
        v
SynthEngine::setParameter()
        |
        +--> _applyParameterToAllVoices()
        |        |
        |        +--> SynthVoice.oscillators
        |        +--> SynthVoice.mixer
        |        +--> SynthVoice.ladderFilter
        |        +--> SynthVoice.envelopes
        |
        +--> OutputStage for MasterVolume and internal Main Drive color link
```

Per voice, the audio path is:

```text
OscillatorBank
   osc1
   osc2
   osc3
        \
         \
NoiseGenerator ----> Mixer ----> LadderFilter ----> VCA ----> voice sample
                       ^             ^                ^
                       |             |                |
                  Mixer Drive   Filter Drive     Loudness envelope
                                     ^
                                     |
                              Filter envelope
```

Then the engine output path is:

```text
Mono mode:
    SynthVoice.processSample()
        |
        v
    OutputStage.processSample()

Poly mode:
    up to 4 x SynthVoice.processSample()
        |
        v
    summed voiceSample
        |
        v
    OutputStage.processSample()
```

The important drive locations are:

```text
Mixer Drive:
    inside each voice, before the ladder filter
    Oscs/Noise -> Mixer Drive -> LadderFilter

Filter Drive:
    inside each voice, at the input of the ladder filter
    Mixer output -> Filter Drive -> Ladder stages

Output Drive UI:
    currently stored for compatibility, but not used as a direct DSP drive

Internal Output Color:
    subtle final output saturation linked to MixerDrive
```

## Core Classes

`SynthEngine` owns the global architecture:

```cpp
VoiceController _voiceCtrl;
SynthVoice      _synthVoice;
std::array<SynthVoice, kPolyVoiceCount> _polyVoices;
Lfo             _lfo;
OutputStage     _output;
float           _params[kParamCount] = {};
```

`SynthVoice` owns the per-note architecture:

```cpp
OscillatorBank   oscillators;
NoiseGenerator   noise;
Mixer            mixer;
LadderFilter     ladderFilter;
ContourGenerator loudnessContour;
ContourGenerator filterContour;
Vca              vca;
```

The exact per-voice processing code is:

```cpp
float SynthVoice::processSample(double pitchHz, double currentMidiNote)
{
    oscillators.setBaseMidiNote(_midiFromHz(pitchHz));
    const auto osc           = oscillators.process();
    const double noiseOut    = noise.processSample();
    const double mixed       = mixer.processSample(osc.osc1, osc.osc2, osc.osc3, noiseOut, 0.0);
    const double loudness    = loudnessContour.processSample();
    const double filterEnv   = filterContour.processSample();
    const double filtered    = ladderFilter.processSample(mixed, filterEnv, currentMidiNote);
    const double amplified   = vca.processSample(filtered, loudness);
    return static_cast<float>(amplified);
}
```

The exact engine summing/output code is:

```cpp
float SynthEngine::processSample()
{
    double voiceSample = 0.0;

    if (_playMode == 0) {
        _voiceCtrl.process();
        const double hz = frequencyForMidiNote(_voiceCtrl.getCurrentMidiNote(), _pitchBendSemitones);
        voiceSample = _synthVoice.processSample(hz, _voiceCtrl.getCurrentMidiNote());
    } else {
        for (size_t i = 0; i < _polyVoices.size(); ++i) {
            if (_polyHeld[i] || _polyVoices[i].isActive()) {
                const double midiNote = static_cast<double>(_polyMidiNotes[i]) + _pitchBendSemitones;
                const double hz = frequencyForMidiNote(midiNote, 0.0);
                voiceSample += _polyVoices[i].processSample(hz, midiNote);
            }
        }
    }

    double out = _output.processSample(voiceSample);
    return static_cast<float>(out);
}
```

Note: the pasted `SynthEngine::processSample()` above is simplified to show the
architecture. The real function also applies LFO/mod-wheel modulation before the
voice processing.

## Default Drive Values

Initial patch defaults from `synth_core/Include/SynthEngine.h`:

```cpp
case ParamId::MixerDrive:  return 1.0f;
case ParamId::FilterDrive: return 0.50f;
```

UI drive range is treated as:

```text
0.0 .. 3.0 UI value
       |
       v
DriveUtils::normDrive(value)
       |
       v
0.0 .. 1.0 DSP amount
```

## What Is Not Part Of The Current Drive Architecture

- No oversampling in the drive blocks.
- No post-filter user Drive stage.
- No independent Output Drive DSP, despite `OutputStage::setDrive()` existing.
- Mixer/Main Drive now also pushes the ladder input and resonance feedback return.
- No drive-specific gain compensation based on oscillator count.
- No separate dry/wet UI control for either drive.
- No explicit harmonic analysis or tone/EQ stage after saturation.

## Shared Drive DSP

```cpp
namespace DriveUtils {

inline double clamp01(double x)
{
    return std::max(0.0, std::min(1.0, x));
}

inline double normDrive(double uiValue)
{
    return clamp01(uiValue / 3.0);
}

inline double smoothstep01(double edge0, double edge1, double x)
{
    const double t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

inline double lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

inline double softLimit(double x, double limit)
{
    limit = std::max(0.1, limit);
    return limit * std::tanh(x / limit);
}

inline double antiFizz(double input, double shaped, double amount)
{
    const double tame = smoothstep01(0.42, 1.0, amount);
    const double hiDelta = shaped - input;
    return shaped - (0.10 * tame) * hiDelta * hiDelta * hiDelta;
}

inline double removeBias(double value, double bias)
{
    return value - std::tanh(bias);
}
```

## Output/Internal Color Shaper

```cpp
inline double mainDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double driveCurve = std::pow(amount, 1.12);
    const double preGain = 1.0 + 7.2 * driveCurve;
    const double x = input * preGain;

    const double presence = x + (0.055 * amount) * (x - std::tanh(0.72 * x) / 0.72);
    const double warmLimit = 1.04 - 0.12 * amount;
    const double warm = warmLimit * std::tanh(presence / warmLimit);

    const double bias = 0.012 * amount;
    const double asym = 0.026 * amount;
    const double edgeInput = presence + asym * presence * presence
                           + (0.030 + 0.060 * amount) * presence * presence * presence
                           + bias;
    double edge = std::tanh(edgeInput);
    edge -= std::tanh(bias);

    const double edgeMix = smoothstep01(0.28, 0.95, amount);
    double y = lerp(warm, edge, edgeMix);

    const double bodyMix = 0.08 * amount * (1.0 - 0.35 * edgeMix);
    y = lerp(y, input, bodyMix);
    y = antiFizz(input, y, amount);

    const double trim = 1.0 / (1.0 + 0.105 * (preGain - 1.0));
    return y * trim;
}
```

## Filter Drive Shaper

```cpp
inline double filterDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double curve = std::pow(amount, 0.96);
    const double push = 1.0 + 5.6 * curve + 2.4 * amount;
    const double x = std::clamp(input * push, -5.0, 5.0);

    const double bodyLimit = 1.10 - 0.18 * amount;
    const double body = bodyLimit * std::tanh(x / bodyLimit);

    const double fold = std::tanh(0.55 * x);
    const double bitePre = x
                         + (0.085 + 0.230 * curve) * x * x * x
                         - (0.045 * curve) * fold * fold * fold;
    const double bias = 0.010 * curve;
    const double asym = 0.070 * curve + 0.030 * amount;
    const double edgeIn = std::clamp(bitePre + asym * x * x + bias, -7.0, 7.0);
    const double edge = removeBias(std::tanh(edgeIn), bias);

    const double highPush = smoothstep01(0.55, 1.0, amount);
    const double bite = (edge - body) * (1.0 + 0.45 * highPush);
    const double biteMix = clamp01(0.18 + 0.78 * std::pow(amount, 0.82));
    const double dryBody = 0.18 * (1.0 - amount) + 0.060 * amount;
    double y = body + biteMix * bite + dryBody * input;

    y = antiFizz(input, y, amount);

    const double outputLift = 1.0 + 0.86 * curve;
    const double trim = 1.0 / (1.0 + 0.035 * (push - 1.0));
    return softLimit(y * outputLift * trim, 1.12);
}
```

## Mixer/Main Drive Shaper

```cpp
inline double mixerDriveSaturate(double input, double amount)
{
    amount = clamp01(amount);

    const double driveCurve = std::pow(amount, 0.52);

    const double bodyGain = 1.0 + 4.2 * driveCurve + 1.5 * amount;
    const double bodyX = std::clamp(input * bodyGain, -5.0, 5.0);
    const double warm = std::tanh(bodyX) / (1.0 + 0.10 * (bodyGain - 1.0));

    const double edgeGain = 1.0 + 12.5 * driveCurve + 4.8 * amount;
    const double edgeX = std::clamp(input * edgeGain, -4.0, 4.0);
    const double asym = 0.075 * driveCurve + 0.034 * amount;
    const double cubic = 0.090 + 0.300 * driveCurve;
    const double bias = 0.008 * driveCurve;
    const double edgePre = edgeX + asym * edgeX * edgeX + cubic * edgeX * edgeX * edgeX + bias;
    double edge = std::tanh(edgePre) - std::tanh(bias);
    edge /= (1.0 + 0.012 * edgeGain);

    const double highPush = smoothstep01(0.60, 1.0, amount);
    const double harmonicLayer = (edge - warm) * (1.0 + 0.35 * highPush);
    const double harmonicBlend = clamp01(0.24 + 0.92 * std::pow(amount, 0.62));
    const double bodyPreserve = 0.28 * (1.0 - amount) + 0.070 * amount;
    double modern = warm + harmonicBlend * harmonicLayer + bodyPreserve * input;

    modern = antiFizz(input, modern, amount);
    modern *= 1.0 + 1.85 * driveCurve;
    return softLimit(modern, 1.18);
}

} // namespace DriveUtils
```

## Mixer Drive Use Site

File: `synth_core/Source/Mixer.cpp`

```cpp
void Mixer::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 1.0;
    _drive = std::clamp(drive, 0.0, 3.0);
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

    const double driveMix = DriveUtils::smoothstep01(0.0, 1.0, std::pow(md, 0.52));
    double out = DriveUtils::lerp(mixerSum, driven, driveMix);

    if (!std::isfinite(out)) out = 0.0;
    return std::clamp(out, -1.0, 1.0);
}
```

Mixer smoothing setup:

```cpp
void Mixer::setSampleRate(double sampleRate)
{
    _sampleRate = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    constexpr double kDriveTau = 0.020;
    _driveAlpha = std::exp(-1.0 / (kDriveTau * _sampleRate));
    _driveNeedsSnap = true;
}
```

## Filter Drive Use Site

File: `synth_core/Source/LadderFilter.cpp`

```cpp
void LadderFilter::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 0.0;
    _drive = std::clamp(drive, 0.0, 3.0);
}

double LadderFilter::processSample(double input, double filterEnvelopeValue, double keyboardMidiNote)
{
    double x = std::isfinite(input) ? input : 0.0;

    const double panelDrive = DriveUtils::normDrive(_drive);
    const double mainPush = DriveUtils::normDrive(_mainDrivePush);
    const double mainPushCurve = std::pow(mainPush, 0.48);
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
```

## Output Stage Drive-Related Code

File: `synth_core/Source/OutputStage.cpp`

```cpp
void OutputStage::setDrive(double drive)
{
    if (!std::isfinite(drive)) drive = 0.0;
    _drive = std::clamp(drive, 0.0, 3.0);
    // _drive is stored for APVTS/preset compatibility but no longer used in DSP.
}

void OutputStage::setMainDriveLink(double mainDriveUiValue)
{
    if (!std::isfinite(mainDriveUiValue)) mainDriveUiValue = 0.0;
    _mainDriveLink = std::clamp(mainDriveUiValue, 0.0, 3.0);
}

double OutputStage::processSample(double input)
{
    double x = input;
    if (!std::isfinite(x)) x = 0.0;
    x *= _masterVolume;

    const double dcBlocked = x - _dcPrevInput + _hpCoeff * _dcPrevOutput;
    _dcPrevInput = x;
    _dcPrevOutput = dcBlocked;

    double out = dcBlocked;

    const double mainDriveNorm = DriveUtils::normDrive(_mainDriveLink);
    const double colorAmt      = 0.025 + 0.055 * DriveUtils::smoothstep01(0.20, 1.0, mainDriveNorm);
    const double colored       = DriveUtils::mainDriveSaturate(out, colorAmt);
    const double colorMix      = 0.08  + 0.10  * DriveUtils::smoothstep01(0.35, 1.0, mainDriveNorm);
    out = DriveUtils::lerp(out, colored, colorMix);

    out = DriveUtils::softLimit(out, 0.98);

    if (!std::isfinite(out)) out = 0.0;
    return std::clamp(out, -1.0, 1.0);
}
```

## SynthEngine Parameter Routing

File: `synth_core/Source/SynthEngine.cpp`

```cpp
case ParamId::MixerDrive:
    voice.mixer.setDrive(value);
    voice.ladderFilter.setMainDrivePush(value);
    break;

case ParamId::FilterDrive:
    voice.ladderFilter.setDrive(value);
    break;
```

```cpp
case ParamId::MixerDrive:
    _applyParameterToAllVoices(id, value);
    _output.setMainDriveLink(value); // internal output color tracks Main Drive
    break;
```

## Notes For Reviewer

- UI values appear to be `0.0..3.0`.
- `DriveUtils::normDrive()` converts those to `0.0..1.0`.
- Mixer Drive is applied per voice in `Mixer::processSample()`.
- Filter Drive is applied before the ladder stages in `LadderFilter::processSample()`.
- Output Drive UI is stored but intentionally not used directly; output color follows Mixer Drive through `setMainDriveLink()`.
- The tests currently pass, but the user reports the audible result still does not feel right.
