#include "../Include/SynthEngine.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace SynthCore {

static Waveform waveformFromParameter(float value)
{
    const int v = static_cast<int>(std::lround(value));
    switch (v) {
    case 0: return Waveform::Triangle;
    case 1: return Waveform::TriangleSaw;
    case 2: return Waveform::Saw;
    case 3: return Waveform::ReverseSaw;
    case 4: return Waveform::Square;
    case 5: return Waveform::WidePulse;
    case 6: return Waveform::NarrowPulse;
    default: return Waveform::Saw;
    }
}

static OscillatorRange rangeFromParameter(float value)
{
    const int v = static_cast<int>(std::lround(value));
    switch (v) {
    case 0: return OscillatorRange::Low;
    case 1: return OscillatorRange::ThirtyTwoFoot;
    case 2: return OscillatorRange::SixteenFoot;
    case 3: return OscillatorRange::EightFoot;
    case 4: return OscillatorRange::FourFoot;
    case 5: return OscillatorRange::TwoFoot;
    default: return OscillatorRange::EightFoot;
    }
}

static double frequencyForMidiNote(double midiNote, double pitchBendSemitones)
{
    return 440.0 * std::pow(2.0, ((midiNote + pitchBendSemitones) - 69.0) / 12.0);
}

SynthEngine::SynthEngine()
{
    std::fill(std::begin(_params), std::end(_params), 0.0f);
    _applyStaticPatchDefaults();
    setSampleRate(44100.0);
}

void SynthEngine::_applyStaticPatchDefaults()
{
    for (int i = 0; i < kParamCount; ++i)
        _params[i] = initPatchValue(static_cast<ParamId>(i));

    _synthVoice.vca.setMasterVolume(0.8);
    _synthVoice.vca.setDrive(0.0);
    for (auto& voice : _polyVoices) {
        voice.vca.setMasterVolume(0.8);
        voice.vca.setDrive(0.0);
    }
    _output.setMasterVolume(0.6);
    _output.setDrive(0.0);

    for (int i = 0; i < kParamCount; ++i) {
        _applyParameter(static_cast<ParamId>(i), _params[i]);
    }
}

void SynthEngine::prepare(double sampleRate, int blockSize)
{
    (void)blockSize;
    setSampleRate(sampleRate);
    _polyHeadroomSmoothed = 1.0f;
}

void SynthEngine::setSampleRate(double sampleRate)
{
    assert(sampleRate >= kMinSampleRate && sampleRate <= kMaxSampleRate);
    _sampleRate = std::clamp(sampleRate, kMinSampleRate, kMaxSampleRate);
    _propagateSampleRate();
}

void SynthEngine::_propagateSampleRate()
{
    _voiceCtrl.setSampleRate(_sampleRate);
    _synthVoice.prepare(_sampleRate, 0);
    for (auto& voice : _polyVoices)
        voice.prepare(_sampleRate, 0);
    _lfo.setSampleRate(_sampleRate);
    _output.setSampleRate(_sampleRate);

    for (auto& s : _smoothers) s.setSampleRate(_sampleRate);
    _polyHeadroomCoeff = static_cast<float>(std::exp(-1.0 / (0.005 * _sampleRate)));
}

void SynthEngine::setParameter(ParamId id, float value)
{
    const int idx = static_cast<int>(id);
    assert(idx >= 0 && idx < kParamCount);
    _params[idx] = value;
    _applyParameter(id, value);
}

float SynthEngine::getParameter(ParamId id) const
{
    const int idx = static_cast<int>(id);
    assert(idx >= 0 && idx < kParamCount);
    return _params[idx];
}

void SynthEngine::_applyParameterToVoice(SynthVoice& voice, ParamId id, float value)
{
    switch (id) {
    case ParamId::Osc1Enabled:
        voice.mixer.setSourceEnabled(MixerSource::Osc1, value >= 0.5f);
        break;
    case ParamId::Osc2Enabled:
        voice.mixer.setSourceEnabled(MixerSource::Osc2, value >= 0.5f);
        break;
    case ParamId::Osc3Enabled:
        voice.mixer.setSourceEnabled(MixerSource::Osc3, value >= 0.5f);
        break;
    case ParamId::Osc1Level:
        voice.mixer.setSourceLevel(MixerSource::Osc1, value);
        break;
    case ParamId::Osc2Level:
        voice.mixer.setSourceLevel(MixerSource::Osc2, value);
        break;
    case ParamId::Osc3Level:
        voice.mixer.setSourceLevel(MixerSource::Osc3, value);
        break;
    case ParamId::Osc1Waveform:
        voice.oscillators.setOscillatorWaveform(1, waveformFromParameter(value));
        break;
    case ParamId::Osc2Waveform:
        voice.oscillators.setOscillatorWaveform(2, waveformFromParameter(value));
        break;
    case ParamId::Osc3Waveform:
        voice.oscillators.setOscillatorWaveform(3, waveformFromParameter(value));
        break;
    case ParamId::Osc1Range:
        voice.oscillators.setOscillatorRange(1, rangeFromParameter(value));
        break;
    case ParamId::Osc2Range:
        voice.oscillators.setOscillatorRange(2, rangeFromParameter(value));
        break;
    case ParamId::Osc3Range:
        voice.oscillators.setOscillatorRange(3, rangeFromParameter(value));
        break;
    case ParamId::Osc2Detune:
        voice.oscillators.setOscillatorDetuneSemitones(2, value);
        break;
    case ParamId::Osc3Detune:
        voice.oscillators.setOscillatorDetuneSemitones(3, value);
        break;
    case ParamId::Osc3KeyboardTracking:
        voice.oscillators.setOscillator3KeyboardTrackingEnabled(value >= 0.5f);
        break;
    case ParamId::MixerDrive:
        voice.mixer.setDrive(value);
        voice.ladderFilter.setMainDrivePush(value);
        break;
    case ParamId::NoiseEnabled:
        voice.mixer.setSourceEnabled(MixerSource::Noise, value >= 0.5f);
        break;
    case ParamId::NoiseLevel:
        voice.mixer.setSourceLevel(MixerSource::Noise, value);
        break;
    case ParamId::NoiseMode:
        voice.noise.setMode(value >= 0.5f ? NoiseMode::Pink : NoiseMode::White);
        break;
    case ParamId::FilterCutoff:
        voice.ladderFilter.setCutoffHz(value);
        break;
    case ParamId::FilterResonance:
        voice.ladderFilter.setResonance(value);
        break;
    case ParamId::FilterEnvAmount:
        voice.ladderFilter.setContourAmount(value);
        break;
    case ParamId::FilterKeyboardTracking:
        voice.ladderFilter.setKeyboardTrackingAmount(value);
        break;
    case ParamId::FilterDrive:
        voice.ladderFilter.setDrive(value);
        break;
    case ParamId::AmpAttack:
        voice.loudnessContour.setAttackSeconds(value);
        break;
    case ParamId::AmpDecay:
        voice.loudnessContour.setDecaySeconds(value);
        break;
    case ParamId::AmpSustain:
        voice.loudnessContour.setSustainLevel(value);
        break;
    case ParamId::AmpRelease:
        voice.loudnessContour.setReleaseSeconds(value);
        break;
    case ParamId::FilterAttack:
        voice.filterContour.setAttackSeconds(value);
        break;
    case ParamId::FilterDecay:
        voice.filterContour.setDecaySeconds(value);
        break;
    case ParamId::FilterSustain:
        voice.filterContour.setSustainLevel(value);
        break;
    case ParamId::FilterRelease:
        voice.filterContour.setReleaseSeconds(value);
        break;
    case ParamId::FineTune:
        voice.oscillators.setOscillatorFineTuneCents(1, static_cast<double>(value));
        voice.oscillators.setOscillatorFineTuneCents(2, static_cast<double>(value));
        voice.oscillators.setOscillatorFineTuneCents(3, static_cast<double>(value));
        break;
    case ParamId::Osc1PulseWidth:
        voice.oscillators.setOscillatorPulseWidth(1, static_cast<double>(value));
        break;
    case ParamId::AnalogDrift:
        voice.oscillators.setAnalogDrift(static_cast<double>(value) * 10.0); // 0..1 → 0..10 cents
        break;
    default:
        break;
    }
}

void SynthEngine::_applyParameterToAllVoices(ParamId id, float value)
{
    _applyParameterToVoice(_synthVoice, id, value);
    for (auto& voice : _polyVoices)
        _applyParameterToVoice(voice, id, value);
}

void SynthEngine::_applyParameter(ParamId id, float value)
{
    switch (id) {
    case ParamId::MasterVolume:
        setMasterVolume(value);
        break;
    case ParamId::GlideTime:
        _voiceCtrl.setGlideTimeSeconds(value);
        break;
    case ParamId::GlideEnabled:
        _voiceCtrl.setGlideEnabled(value >= 0.5f);
        break;
    case ParamId::PitchBendRange:
        _voiceCtrl.setPitchBendRange(value);
        break;
    case ParamId::PlayMode:
        setPlayMode(static_cast<int>(std::lround(value)));
        break;
    case ParamId::LfoRate:
        _lfo.setRate(std::max(0.01, static_cast<double>(value)));
        break;
    case ParamId::LfoEnabled:
    case ParamId::LfoAmount:
    case ParamId::LfoDestination:
    case ParamId::ModWheelAmount:
        // Consumed per-sample in processSample(); value is stored in _params.
        break;
    case ParamId::Legato:
        _voiceCtrl.setLegato(value >= 0.5f);
        break;
    case ParamId::Retrigger:
        _voiceCtrl.setRetrigger(value >= 0.5f);
        break;
    case ParamId::NotePriority:
        _voiceCtrl.setNotePriority(static_cast<VoiceController::NotePriority>(
            std::clamp(static_cast<int>(std::lround(static_cast<double>(value))), 0, 2)));
        break;
    case ParamId::MixerDrive:
        _applyParameterToAllVoices(id, value);
        _output.setMainDriveLink(value); // internal output color tracks Main Drive
        break;
    default:
        _applyParameterToAllVoices(id, value);
        break;
    }
}

void SynthEngine::processMidi(const uint8_t* data, int numBytes)
{
    if (numBytes < 1 || data == nullptr) return;

    const uint8_t status = data[0] & 0xF0u;
    if (status == 0x90u && numBytes >= 3) {
        if ((data[2] & 0x7Fu) == 0u) noteOff(data[1] & 0x7Fu);
        else noteOn(data[1] & 0x7Fu, static_cast<float>(data[2] & 0x7Fu));
    } else if (status == 0x80u && numBytes >= 3) {
        noteOff(data[1] & 0x7Fu);
    } else if (status == 0xE0u && numBytes >= 3) {
        const int value14 = static_cast<int>(data[1] & 0x7Fu) | (static_cast<int>(data[2] & 0x7Fu) << 7);
        const double normalized = (static_cast<double>(value14) - 8192.0) / 8192.0;
        setPitchBend(normalized * _params[static_cast<int>(ParamId::PitchBendRange)]);
    } else if (status == 0xB0u && numBytes >= 3) {
        const uint8_t controller = data[1] & 0x7Fu;
        const uint8_t value      = data[2] & 0x7Fu;
        if (controller == 1u) {
            _modWheelPosition = static_cast<double>(value) / 127.0;
        } else if (controller == 64u) {
            // Sustain pedal: while held, real note-offs are deferred (see
            // noteOff()); on release, flush every note that was held only
            // by the pedal.
            const bool down = value >= 64u;
            if (down) {
                _sustainPedalDown = true;
            } else if (_sustainPedalDown) {
                _sustainPedalDown = false;
                _releaseSustainedNotes();
            }
        } else if (controller == 120u) {
            // All Sound Off: silence immediately, ignoring release time —
            // equivalent to a power-cycle, which is exactly what reset()
            // already does without disturbing the current patch parameters.
            reset();
        } else if (controller == 121u) {
            // Reset All Controllers: pitch bend/mod wheel/sustain back to
            // default, but currently-held real notes keep sounding (unlike
            // CC120, this is not a note-kill).
            setPitchBend(0.0);
            _modWheelPosition = 0.0;
            if (_sustainPedalDown) {
                _sustainPedalDown = false;
                _releaseSustainedNotes();
            }
        } else if (controller == 123u) {
            // All Notes Off
            _voiceCtrl.allNotesOff();
            _synthVoice.noteOff();
            _allPolyNotesOff();
            _sustainPedalDown = false;
            _sustainedNotes.fill(false);
        }
    }
}

void SynthEngine::_releaseSustainedNotes()
{
    for (int n = 0; n < static_cast<int>(_sustainedNotes.size()); ++n) {
        if (_sustainedNotes[static_cast<size_t>(n)]) {
            _sustainedNotes[static_cast<size_t>(n)] = false;
            noteOff(n); // sustain is already off here, so this is a real release
        }
    }
}

void SynthEngine::setModWheel(double position)
{
    _modWheelPosition = std::clamp(position, 0.0, 1.0);
}

static void advanceRng(uint32_t& s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
}

void SynthEngine::noteOn(int midiNote, float velocity)
{
    // A fresh key-down means this note is physically held again, not merely
    // sustain-held — otherwise a later pedal-up would incorrectly cut off a
    // note whose key is still down (press, release-under-pedal, re-press,
    // then release the pedal).
    {
        const int note = std::clamp(midiNote, 0, 127);
        _sustainedNotes[static_cast<size_t>(note)] = false;
    }

    if (_playMode == 0) {
        const bool wasActive  = _synthVoice.isActive();
        // Legato gate must reflect physical key-held state, not envelope
        // activity: _voiceCtrl.isGateHigh() is true iff the note stack is
        // non-empty (a key is actually down), whereas _synthVoice.isActive()
        // stays true through a still-decaying release tail even after every
        // key has been released. Read before _voiceCtrl.noteOn() below,
        // which mutates the stack/gate state for the incoming note.
        const bool wasKeyHeld = _voiceCtrl.isGateHigh();
        const bool legatoMode = _params[static_cast<int>(ParamId::Legato)]   >= 0.5f;
        const bool retrigMode = _params[static_cast<int>(ParamId::Retrigger)] >= 0.5f;
        // Legato suppresses envelope retrigger only when Retrigger is also off.
        const bool isLegato   = wasKeyHeld && legatoMode && !retrigMode;

        _voiceCtrl.noteOn(midiNote, velocity);
        // In legato mode (no retrigger), keep envelopes running continuously.
        // Only call noteOn (which fires gateOn) when we actually want a retrigger.
        if (!isLegato)
            _synthVoice.noteOn(midiNote, velocity);

        // Only randomize phases when the voice was truly silent.
        // Jumping phase while the envelope is non-zero creates an audible click;
        // real analog oscillators run continuously and never reset on retrigger.
        if (!isLegato && !wasActive) {
            advanceRng(_noteStartRng);
            const uint32_t seed = _noteStartRng
                ^ (static_cast<uint32_t>(midiNote + 1) * 2246822519u);
            _synthVoice.oscillators.randomizePhases(seed);
            _synthVoice.resetAudioChainState();
        } else if (!isLegato) {
            advanceRng(_noteStartRng); // keep RNG sequence consistent
        }
        return;
    }

    // Capture pre-allocation voice state for the diagnostic logger
#if LADDERVOICE_ENABLE_POLY_TRACE
    AllocPayload allocPay{};
    allocPay.sessionSample = _sessionSample;
    allocPay.blockIndex    = PolyTraceLogger::instance().currentBlockIndex();
    allocPay.requestedNote = static_cast<uint8_t>(std::clamp(midiNote, 0, 127));
    int tracActiveBefore = 0, tracHeldBefore = 0, tracRelBefore = 0;
    for (size_t k = 0; k < _polyVoices.size(); ++k) {
        auto& vi    = allocPay.voicesBefore[k];
        vi.active   = _polyVoices[k].isActive() ? 1u : 0u;
        vi.held     = _polyHeld[k] ? 1u : 0u;
        vi.midiNote = static_cast<uint8_t>(_polyMidiNotes[k] & 0x7F);
        vi.envStage = static_cast<uint8_t>(_polyVoices[k].loudnessContour.currentStage());
        vi.envValue = static_cast<float>(_polyVoices[k].loudnessContour.getCurrentValue());
        if (vi.active) { ++tracActiveBefore; if (!vi.held) ++tracRelBefore; }
        if (vi.held)   ++tracHeldBefore;
    }
#endif

    const int voiceIndex = _allocatePolyVoice(std::clamp(midiNote, 0, 127));
    const size_t voiceSlot = static_cast<size_t>(voiceIndex);
    const bool wasHeld = _polyHeld[voiceSlot];
    const bool wasActive = _polyVoices[voiceSlot].isActive();
    const bool isReleaseTailReuse = !wasHeld && wasActive;

#if LADDERVOICE_ENABLE_POLY_TRACE
    // Determine allocation reason from pre-alloc state
    {
        const auto& vb = allocPay.voicesBefore[voiceSlot];
        AllocReason reason;
        const int clampedNote = std::clamp(midiNote, 0, 127);
        if (vb.held && static_cast<int>(vb.midiNote) == clampedNote)
            reason = AllocReason::SameNote;
        else if (!vb.held && !vb.active)
            reason = AllocReason::FreeSlot;
        else if (!vb.held)
            reason = AllocReason::ReleaseTail;
        else
            reason = AllocReason::Oldest;
        allocPay.chosenVoice = static_cast<uint8_t>(voiceSlot);
        allocPay.reason      = static_cast<uint8_t>(reason);
        allocPay.wasReset    = wasActive ? 0u : 1u;
        POLY_TRACE_PUSH_ALLOC(allocPay);
    }
#endif

    _polyMidiNotes[voiceSlot] = std::clamp(midiNote, 0, 127);
    _polyHeld[voiceSlot] = true;
    _polyAges[voiceSlot] = ++_voiceAgeCounter;

    // Only randomize phases when the poly voice was silent — same click-prevention
    // logic as mono: jumping phase mid-release causes a discontinuity in the output.
    advanceRng(_noteStartRng);
    if (!wasActive) {
        const uint32_t seed = _noteStartRng
            ^ (static_cast<uint32_t>(voiceIndex + 1) * 2654435761u)
            ^ (static_cast<uint32_t>(midiNote + 1) * 2246822519u);
        _polyVoices[voiceSlot].oscillators.randomizePhases(seed);
        _polyVoices[voiceSlot].resetAudioChainState();
    }

    _polyVoices[voiceSlot].noteOn(
        midiNote,
        velocity,
        isReleaseTailReuse ? VoiceStartMode::StolenRelease : VoiceStartMode::Normal);

#if LADDERVOICE_ENABLE_POLY_TRACE
    {
        // Count post-allocation voice state
        int tracActiveAfter = 0, tracHeldAfter = 0, tracRelAfter = 0;
        for (size_t k = 0; k < _polyVoices.size(); ++k) {
            if (_polyVoices[k].isActive()) { ++tracActiveAfter; if (!_polyHeld[k]) ++tracRelAfter; }
            if (_polyHeld[k]) ++tracHeldAfter;
        }
        NotePayload notePay{};
        notePay.sessionSample      = _sessionSample;
        notePay.blockIndex         = PolyTraceLogger::instance().currentBlockIndex();
        notePay.sampleOffset       = 0;
        notePay.midiNote           = static_cast<uint8_t>(std::clamp(midiNote, 0, 127));
        notePay.velocity           = static_cast<uint8_t>(std::clamp(static_cast<int>(velocity), 0, 127));
        notePay.voiceIndex         = static_cast<uint8_t>(voiceSlot);
        notePay.wasHeld            = wasHeld ? 1u : 0u;
        notePay.wasActive          = wasActive ? 1u : 0u;
        notePay.isReleaseTailReuse = isReleaseTailReuse ? 1u : 0u;
        notePay.activeBefore       = static_cast<uint8_t>(tracActiveBefore);
        notePay.heldBefore         = static_cast<uint8_t>(tracHeldBefore);
        notePay.releasingBefore    = static_cast<uint8_t>(tracRelBefore);
        notePay.activeAfter        = static_cast<uint8_t>(tracActiveAfter);
        notePay.heldAfter          = static_cast<uint8_t>(tracHeldAfter);
        notePay.releasingAfter     = static_cast<uint8_t>(tracRelAfter);
        POLY_TRACE_PUSH_NOTE_ON(notePay);
    }
#endif
}

void SynthEngine::noteOff(int midiNote)
{
    if (_sustainPedalDown) {
        // Defer the real release until the pedal comes up — the key was
        // lifted, but the note must keep sounding.
        const int note = std::clamp(midiNote, 0, 127);
        _sustainedNotes[static_cast<size_t>(note)] = true;
        return;
    }

    if (_playMode == 0) {
        _voiceCtrl.noteOff(midiNote);
        _voiceCtrl.process();
        if (!_voiceCtrl.isGateHigh())
            _synthVoice.noteOff();
        return;
    }

    const int note = std::clamp(midiNote, 0, 127);
    for (size_t i = 0; i < _polyVoices.size(); ++i) {
        if (_polyHeld[i] && _polyMidiNotes[i] == note) {
            _polyHeld[i] = false;
            _polyVoices[i].noteOff();

#if LADDERVOICE_ENABLE_POLY_TRACE
            {
                int activeAfter = 0, heldAfter = 0, relAfter = 0;
                for (size_t k = 0; k < _polyVoices.size(); ++k) {
                    if (_polyVoices[k].isActive()) { ++activeAfter; if (!_polyHeld[k]) ++relAfter; }
                    if (_polyHeld[k]) ++heldAfter;
                }
                NotePayload np{};
                np.sessionSample   = _sessionSample;
                np.blockIndex      = PolyTraceLogger::instance().currentBlockIndex();
                np.midiNote        = static_cast<uint8_t>(note);
                np.voiceIndex      = static_cast<uint8_t>(i);
                np.wasHeld         = 1u;
                np.wasActive       = _polyVoices[i].isActive() ? 1u : 0u;
                np.activeAfter     = static_cast<uint8_t>(activeAfter);
                np.heldAfter       = static_cast<uint8_t>(heldAfter);
                np.releasingAfter  = static_cast<uint8_t>(relAfter);
                POLY_TRACE_PUSH_NOTE_OFF(np);
            }
#endif
        }
    }
}

void SynthEngine::setPitchBend(double semitones)
{
    _pitchBendSemitones = semitones;
    _voiceCtrl.setPitchBend(semitones);
}

void SynthEngine::setLoudnessAttack(double seconds)
{
    _applyParameterToAllVoices(ParamId::AmpAttack, static_cast<float>(seconds));
}

void SynthEngine::setLoudnessDecay(double seconds)
{
    _applyParameterToAllVoices(ParamId::AmpDecay, static_cast<float>(seconds));
}

void SynthEngine::setLoudnessSustain(double level)
{
    _applyParameterToAllVoices(ParamId::AmpSustain, static_cast<float>(level));
}

void SynthEngine::setLoudnessRelease(double seconds)
{
    _applyParameterToAllVoices(ParamId::AmpRelease, static_cast<float>(seconds));
}

void SynthEngine::setFilterAttack(double seconds)
{
    _applyParameterToAllVoices(ParamId::FilterAttack, static_cast<float>(seconds));
}

void SynthEngine::setFilterDecay(double seconds)
{
    _applyParameterToAllVoices(ParamId::FilterDecay, static_cast<float>(seconds));
}

void SynthEngine::setFilterSustain(double level)
{
    _applyParameterToAllVoices(ParamId::FilterSustain, static_cast<float>(level));
}

void SynthEngine::setFilterRelease(double seconds)
{
    _applyParameterToAllVoices(ParamId::FilterRelease, static_cast<float>(seconds));
}

void SynthEngine::setMasterVolume(double volume)
{
    _output.setMasterVolume(volume);
}

void SynthEngine::setOutputDrive(double drive)
{
    _output.setDrive(drive);
}

void SynthEngine::setVCADrive(double drive)
{
    _synthVoice.vca.setDrive(drive);
    for (auto& voice : _polyVoices)
        voice.vca.setDrive(drive);
}

void SynthEngine::setNoiseEnabled(bool enabled)
{
    _applyParameterToAllVoices(ParamId::NoiseEnabled, enabled ? 1.0f : 0.0f);
}

void SynthEngine::setNoiseLevel(double level)
{
    _applyParameterToAllVoices(ParamId::NoiseLevel, static_cast<float>(level));
}

void SynthEngine::setNoiseMode(NoiseMode mode)
{
    const float value = mode == NoiseMode::Pink ? 1.0f : 0.0f;
    _applyParameterToAllVoices(ParamId::NoiseMode, value);
}

void SynthEngine::setNoiseSeed(uint32_t seed)
{
    _synthVoice.noise.setSeed(seed);
    for (auto& voice : _polyVoices)
        voice.noise.setSeed(seed);
}

void SynthEngine::setFilterCutoffHz(double hz)
{
    _applyParameterToAllVoices(ParamId::FilterCutoff, static_cast<float>(hz));
}

void SynthEngine::setFilterResonance(double resonance)
{
    _applyParameterToAllVoices(ParamId::FilterResonance, static_cast<float>(resonance));
}

void SynthEngine::setFilterContourAmount(double amount)
{
    _applyParameterToAllVoices(ParamId::FilterEnvAmount, static_cast<float>(amount));
}

void SynthEngine::setFilterKeyboardTrackingAmount(double amount)
{
    _applyParameterToAllVoices(ParamId::FilterKeyboardTracking, static_cast<float>(amount));
}

void SynthEngine::setFilterDrive(double drive)
{
    _applyParameterToAllVoices(ParamId::FilterDrive, static_cast<float>(drive));
}

void SynthEngine::setPlayMode(int mode)
{
    const int nextMode = mode >= 1 ? 1 : 0;
    if (_playMode == nextMode) return;

    _voiceCtrl.allNotesOff();
    _synthVoice.noteOff();
    _allPolyNotesOff();
    _playMode = nextMode;
}

float SynthEngine::processSample()
{
    _applySmoothedParams(0);

    // ── LFO ──────────────────────────────────────────────────────────────────
    const double lfoRaw     = _lfo.processSample();
    const bool   lfoOn      = _params[static_cast<int>(ParamId::LfoEnabled)] >= 0.5f;
    const double lfoAmt     = static_cast<double>(_params[static_cast<int>(ParamId::LfoAmount)]);
    const double mwAmt      = static_cast<double>(_params[static_cast<int>(ParamId::ModWheelAmount)]);
    // The visible Mod Wheel knob is a standalone extra depth in the standalone app.
    // MIDI CC1 can still push the same depth harder while never blocking base LFO amount.
    const double totalDepth = std::clamp(lfoAmt + mwAmt + (mwAmt * _modWheelPosition), 0.0, 1.0);

    // Hard bypass: when both LFO amount and wheel depth are zero, skip ALL modulation.
    // This guarantees that rate, destination, and LFO phase cannot create any audible
    // effect even if LfoEnabled is left on by the host or the standalone initialiser.
    const bool   lfoActive  = lfoOn && (totalDepth > 1.0e-5);
    const double lfoValue   = lfoActive ? totalDepth * lfoRaw : 0.0;
    const int    lfoDest    = static_cast<int>(std::lround(
                                  static_cast<double>(_params[static_cast<int>(ParamId::LfoDestination)])));

    if (lfoActive) {
        // Dest 1 — Filter: modulate cutoff logarithmically (±3 octaves at full depth)
        if (lfoDest == 1) {
            const double base    = static_cast<double>(_params[static_cast<int>(ParamId::FilterCutoff)]);
            const double modFreq = std::clamp(base * std::pow(2.0, lfoValue * 3.0), 20.0, 20000.0);
            _synthVoice.ladderFilter.setCutoffHz(modFreq);
            for (auto& voice : _polyVoices)
                voice.ladderFilter.setCutoffHz(modFreq);
        }
        // Dest 2 — Pulse Width (square/pulse waveforms only)
        else if (lfoDest == 2) {
            const double pw = std::clamp(0.5 + lfoValue * 0.4, 0.10, 0.90);
            for (int osc = 1; osc <= 3; ++osc) {
                _synthVoice.oscillators.setOscillatorPulseWidth(osc, pw);
                for (auto& voice : _polyVoices)
                    voice.oscillators.setOscillatorPulseWidth(osc, pw);
            }
        }
    }

    // Dest 0 — Pitch: ±12 semitones at full depth, applied per voice below
    const double lfoPitchSemitones = (lfoActive && lfoDest == 0) ? lfoValue * 12.0 : 0.0;

    // ── Voice rendering ───────────────────────────────────────────────────────
    float voiceSample = 0.0f;
    if (_playMode == 0) {
        const VoiceState voiceState = _voiceCtrl.process();
        double hz = voiceState.pitchHz;
        if (lfoDest == 0 && hz > 0.0)
            hz *= std::pow(2.0, lfoPitchSemitones / 12.0);
        voiceSample = _synthVoice.processSample(hz, _voiceCtrl.getCurrentMidiNote());
    } else {
        int activeCount = 0;
        for (size_t i = 0; i < _polyVoices.size(); ++i) {
            if (_polyHeld[i] || _polyVoices[i].isActive()) {
                const double midiNote = static_cast<double>(_polyMidiNotes[i]);
                double hz = frequencyForMidiNote(midiNote, _pitchBendSemitones);
                if (lfoDest == 0)
                    hz *= std::pow(2.0, lfoPitchSemitones / 12.0);
                voiceSample += _polyVoices[i].processSample(hz, midiNote);
                ++activeCount;
            }
        }
        if (activeCount == 0) {
            _polyHeadroomSmoothed = 1.0f; // reset for next chord — voiceSample is 0 here
        } else {
            const float polyHeadroomTarget =
                1.0f / (1.0f + 0.25f * static_cast<float>(activeCount - 1));
            _polyHeadroomSmoothed = _polyHeadroomCoeff * _polyHeadroomSmoothed
                                  + (1.0f - _polyHeadroomCoeff) * polyHeadroomTarget;
            voiceSample *= _polyHeadroomSmoothed;
        }
    }

    double out = _output.processSample(voiceSample);
    if (!std::isfinite(out)) out = 0.0;
    ++_sessionSample;
    return static_cast<float>(std::clamp(out, -1.0, 1.0));
}

void SynthEngine::processBlock(float* outputBuffer, int numFrames)
{
    assert(outputBuffer != nullptr);
    assert(numFrames > 0);

    for (int frame = 0; frame < numFrames; ++frame) {
        const float out = processSample();
        outputBuffer[frame * 2] = out;
        outputBuffer[frame * 2 + 1] = out;
    }
}

void SynthEngine::_applySmoothedParams(int /*frame*/)
{
}

void SynthEngine::reset()
{
    _voiceCtrl.reset();
    _synthVoice.reset();
    for (auto& voice : _polyVoices)
        voice.reset();
    _polyMidiNotes.fill(-1);
    _polyHeld.fill(false);
    _polyAges.fill(0);
    _voiceAgeCounter = 0;
    _pitchBendSemitones = 0.0;
    _lfo.reset();
    _modWheelPosition = 0.0;
    _sustainPedalDown = false;
    _sustainedNotes.fill(false);
    _output.reset();
    _railSag              = 0.0f;
    _polyHeadroomSmoothed = 1.0f;
    _sessionSample        = 0;
    for (auto& s : _smoothers) {
        s = ParamSmoother{};
        s.setSampleRate(_sampleRate);
    }
    for (int i = 0; i < kParamCount; ++i)
        _applyParameter(static_cast<ParamId>(i), _params[i]);
}

int SynthEngine::_allocatePolyVoice(int midiNote)
{
    for (size_t i = 0; i < _polyVoices.size(); ++i) {
        if (_polyHeld[i] && _polyMidiNotes[i] == midiNote)
            return static_cast<int>(i);
    }

    for (size_t i = 0; i < _polyVoices.size(); ++i) {
        if (!_polyHeld[i] && !_polyVoices[i].isActive())
            return static_cast<int>(i);
    }

    for (size_t i = 0; i < _polyVoices.size(); ++i) {
        if (!_polyHeld[i])
            return static_cast<int>(i);
    }

    size_t oldest = 0;
    for (size_t i = 1; i < _polyAges.size(); ++i) {
        if (_polyAges[i] < _polyAges[oldest])
            oldest = i;
    }
    return static_cast<int>(oldest);
}

void SynthEngine::_allPolyNotesOff()
{
    for (size_t i = 0; i < _polyVoices.size(); ++i) {
        _polyHeld[i] = false;
        _polyVoices[i].noteOff();
    }
}

#if LADDERVOICE_ENABLE_POLY_TRACE
void SynthEngine::fillVoiceSnapshots(SnapshotPayload* out, uint32_t crackleIndex) const
{
    for (size_t i = 0; i < _polyVoices.size(); ++i) {
        const auto& v = _polyVoices[i];
        auto& s = out[i];
        std::memset(&s, 0, sizeof(s));
        s.sessionSample  = _sessionSample;
        s.blockIndex     = PolyTraceLogger::instance().currentBlockIndex();
        s.crackleIndex   = crackleIndex;
        s.voiceIndex     = static_cast<uint8_t>(i);
        s.isActive       = v.isActive() ? 1u : 0u;
        s.isHeld         = _polyHeld[i] ? 1u : 0u;
        s.isReleasing    = (v.isActive() && !_polyHeld[i]) ? 1u : 0u;
        s.midiNote       = static_cast<uint8_t>(_polyMidiNotes[i] & 0x7F);
        s.loudnessStage  = static_cast<uint8_t>(v.loudnessContour.currentStage());
        s.filterStage    = static_cast<uint8_t>(v.filterContour.currentStage());
        s.loudnessValue  = static_cast<float>(v.loudnessContour.getCurrentValue());
        s.filterValue    = static_cast<float>(v.filterContour.getCurrentValue());
        s.ladderStage0   = v.ladderFilter.stageValue(0);
        s.ladderStage1   = v.ladderFilter.stageValue(1);
        s.ladderStage2   = v.ladderFilter.stageValue(2);
        s.ladderStage3   = v.ladderFilter.stageValue(3);
    }
}
#endif

} // namespace SynthCore
