#pragma once

// ── Lightweight real-time instrumentation for the live-click investigation ──
// Independent of PolyTraceLogger / LADDERVOICE_ENABLE_POLY_TRACE entirely —
// this is a separate, always-compiled, runtime-gated observer so it can be
// used in a normal Release build without enabling poly tracing.
//
// Purely additive: every hook is a single "if inactive, return" branch plus
// a lock-free ring-buffer push. It does not read back any logged value, does
// not allocate on the audio thread, and does not change control flow or
// timing of the DSP/voice-allocation code it observes.
//
// Runtime gate: env var LADDERVOICE_RT_LOG=1 (checked once in start()).
// When inactive (the default), every call is a single relaxed atomic load
// and an early return.

#include <atomic>
#include <array>
#include <cstdint>
#include <string>

namespace SynthCore {

enum class RtEventType : uint8_t {
    Block         = 0, // block start: sessionSample/blockIndex/blockSize/sampleRate
    NoteOnMsg     = 1, // incoming MIDI note-on, sampleOffset = position inside block
    NoteOffMsg    = 2, // incoming MIDI note-off
    MidiOther     = 3, // CC / pitch bend / other channel messages
    VoiceAlloc    = 4, // poly voice allocation decision (voiceIndex + reason)
    EnvGateOn     = 5, // loudness+filter contour gateOn() for a voice
    EnvGateOff    = 6, // loudness+filter contour gateOff() for a voice
    VoiceReset    = 7, // oscillator phase randomize / audio-chain reset / stolen-release restart
    ParamChange   = 8, // a forwarded parameter's value changed since the previous block
    CpuWarning    = 9, // block wall-clock time exceeded a fraction of the real-time budget
};

// Mirrors the allocation reasons in SynthEngine::_allocatePolyVoice, kept as
// a small independent enum so this header has no dependency on PolyTraceLogger.
enum class RtAllocReason : uint8_t {
    SameNote = 0, FreeSlot = 1, ReleaseTail = 2, Oldest = 3, NotApplicable = 255
};

// Sub-reason for VoiceReset events.
enum class RtResetReason : uint8_t {
    PhaseRandomizeAndChainReset = 0, // fresh voice start (SynthEngine::noteOn, !wasActive)
    // 1 was StolenReleaseRestart, retired when ReleaseTail reuse stopped
    // hard-resetting voice state (see SynthVoice::noteOn); reserved to keep
    // any previously-recorded trace CSVs' values meaningful.
};

struct RtEvent {
    uint64_t     sessionSample  = 0;
    uint64_t     blockIndex     = 0;
    uint32_t     sampleOffset   = 0;   // offset within the block, where applicable
    RtEventType  type           = RtEventType::Block;
    int8_t       voiceIndex     = -1;  // -1 = n/a
    int16_t      note           = -1;  // -1 = n/a
    uint8_t      velocity       = 0;
    RtAllocReason reason        = RtAllocReason::NotApplicable;
    int16_t      paramId        = -1;  // -1 = n/a; otherwise SynthCore::ParamId ordinal
    float        paramValue     = 0.0f;
    uint32_t     blockSize      = 0;   // Block events only
    float        sampleRate     = 0.0f;// Block events only
    uint64_t     wallClockMs    = 0;   // ms since logger start — for correlating with "I heard it at ~X"
};

class RtEventLog {
public:
    static RtEventLog& instance();

    void start();  // creates session dir + CSV, starts writer thread (env-var gated)
    void stop();

    bool isActive() const noexcept { return _active.load(std::memory_order_acquire); }
    const std::string& sessionDirectory() const noexcept { return _sessionDirPublic; }

    void setBlockContext(uint64_t blockIdx, uint64_t sessionSample) noexcept {
        _blockIndex = blockIdx;
        _sessionSample = sessionSample;
    }

    bool push(const RtEvent& e) noexcept {
        if (!isActive()) return false;
        const uint32_t w = _wPos.load(std::memory_order_relaxed);
        const uint32_t next = (w + 1u) & kRingMask;
        if (next == _rPos.load(std::memory_order_acquire)) {
            _dropped.fetch_add(1u, std::memory_order_relaxed);
            return false;
        }
        _ring[w] = e;
        _wPos.store(next, std::memory_order_release);
        return true;
    }

    // Convenience: fills in sessionSample/blockIndex/wallClockMs from current
    // context automatically so call sites only set the event-specific fields.
    void log(RtEventType type, uint32_t sampleOffset = 0, int8_t voiceIndex = -1,
              int16_t note = -1, uint8_t velocity = 0,
              RtAllocReason reason = RtAllocReason::NotApplicable,
              int16_t paramId = -1, float paramValue = 0.0f,
              uint32_t blockSize = 0, float sampleRate = 0.0f) noexcept
    {
        if (!isActive()) return;
        RtEvent e;
        e.sessionSample = _sessionSample;
        e.blockIndex    = _blockIndex;
        e.sampleOffset  = sampleOffset;
        e.type          = type;
        e.voiceIndex    = voiceIndex;
        e.note          = note;
        e.velocity      = velocity;
        e.reason        = reason;
        e.paramId       = paramId;
        e.paramValue    = paramValue;
        e.blockSize     = blockSize;
        e.sampleRate    = sampleRate;
        e.wallClockMs   = _elapsedMs();
        push(e);
    }

private:
    RtEventLog() = default;
    ~RtEventLog() { stop(); }
    RtEventLog(const RtEventLog&) = delete;
    RtEventLog& operator=(const RtEventLog&) = delete;

    uint64_t _elapsedMs() const noexcept;

    static constexpr uint32_t kRingCapacity = 1u << 14; // 16384 events
    static constexpr uint32_t kRingMask     = kRingCapacity - 1u;

    uint64_t _sessionSample = 0;
    uint64_t _blockIndex    = 0;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
    alignas(64) std::atomic<uint32_t> _wPos{0};
    alignas(64) std::atomic<uint32_t> _rPos{0};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    std::atomic<uint32_t> _dropped{0};
    std::atomic<bool>     _active{false};

    std::array<RtEvent, kRingCapacity> _ring{};

    struct Impl;
    Impl* _impl = nullptr;
    std::string _sessionDirPublic;

    void _writerMain();
    void _writeEvent(const RtEvent& e);
    bool pop(RtEvent& e) noexcept {
        const uint32_t r = _rPos.load(std::memory_order_relaxed);
        if (r == _wPos.load(std::memory_order_acquire)) return false;
        e = _ring[r];
        _rPos.store((r + 1u) & kRingMask, std::memory_order_release);
        return true;
    }
};

} // namespace SynthCore
