#pragma once

// ── Compile-time gate ─────────────────────────────────────────────────────────
// Enable by passing -DLADDERVOICE_ENABLE_POLY_TRACE=1 to the compiler,
// or via CMake option LADDERVOICE_ENABLE_POLY_TRACE.
// Automatically enabled for Debug builds via CMakeLists.
// At runtime, additionally gate with env var: LADDERVOICE_POLY_TRACE=1
// When the compile flag is 0, every call compiles to a no-op.

#ifndef LADDERVOICE_ENABLE_POLY_TRACE
#define LADDERVOICE_ENABLE_POLY_TRACE 0
#endif

#if LADDERVOICE_ENABLE_POLY_TRACE

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>

namespace SynthCore {

// ─── Allocation reason ────────────────────────────────────────────────────────
enum class AllocReason : uint8_t {
    SameNote    = 0,  // slot already playing this MIDI note
    FreeSlot    = 1,  // fully inactive (not held, not active)
    ReleaseTail = 2,  // not held but still releasing (tail reuse — key click source)
    Oldest      = 3,  // forced steal of the oldest held voice
};

// ─── Event type ───────────────────────────────────────────────────────────────
enum class TraceEventType : uint8_t {
    Block         = 0,
    NoteOn        = 1,
    NoteOff       = 2,
    VoiceAlloc    = 3,
    VoiceSnapshot = 4,
    Crackle       = 5,
};

// ─── Compact per-voice state (used inside AllocPayload) ──────────────────────
struct VoiceInfo {
    uint8_t active;    // 1 = loudnessContour.isActive()
    uint8_t held;      // 1 = _polyHeld[i]
    uint8_t midiNote;
    uint8_t envStage;  // ContourGenerator::Stage ordinal (0=Idle..4=Release)
    float   envValue;  // loudnessContour.getCurrentValue()
};
static_assert(sizeof(VoiceInfo) == 8, "VoiceInfo size changed");

// ─── Payload structs (each ≤ 120 bytes to fit in TraceEvent) ─────────────────

struct BlockPayload {
    uint64_t sessionSample;     // global sample counter at block start
    uint64_t blockIndex;
    uint32_t blockSize;
    float    sampleRate;
    uint8_t  playMode;          // 0=mono, 1=poly4
    uint8_t  activeVoices;
    uint8_t  heldVoices;
    uint8_t  releasingVoices;   // active && !held
    float    finalPeak;
    float    finalRms;
    float    maxDelta;          // max |out[n]-out[n-1]| in block
    uint16_t maxDeltaOffset;    // sample index inside block where maxDelta occurred
    uint16_t _pad;
    float    polySumPeak;       // peak of voiceSample before OutputStage (0 if unavailable)
    uint32_t blockTimeUs;       // wall-clock processing time (from PluginProcessor)
    float    cpuPercent;        // blockTimeUs / blockBudgetUs * 100
    uint32_t droppedEvents;     // ring-buffer drops since last block event
};

struct NotePayload {
    uint64_t sessionSample;
    uint64_t blockIndex;
    uint32_t sampleOffset;      // MIDI event position inside block
    uint8_t  midiNote;
    uint8_t  velocity;          // 0 for noteOff
    uint8_t  voiceIndex;        // 0xFF = mono/unavailable
    uint8_t  wasHeld;
    uint8_t  wasActive;
    uint8_t  isReleaseTailReuse;  // noteOn AND !wasHeld AND wasActive
    uint8_t  activeBefore;
    uint8_t  heldBefore;
    uint8_t  releasingBefore;
    uint8_t  activeAfter;
    uint8_t  heldAfter;
    uint8_t  releasingAfter;
    uint8_t  _pad[5];
};

struct AllocPayload {
    uint64_t   sessionSample;
    uint64_t   blockIndex;
    uint8_t    requestedNote;
    uint8_t    chosenVoice;
    uint8_t    reason;           // AllocReason ordinal
    uint8_t    wasReset;         // 1 if resetAudioChainState() was called
    VoiceInfo  voicesBefore[4];  // state of every poly slot BEFORE allocation
};

struct SnapshotPayload {
    uint64_t sessionSample;
    uint64_t blockIndex;
    uint32_t crackleIndex;       // which crackle triggered this snapshot
    uint8_t  voiceIndex;
    uint8_t  isActive;
    uint8_t  isHeld;
    uint8_t  isReleasing;
    uint8_t  midiNote;
    uint8_t  loudnessStage;      // ContourGenerator::Stage ordinal
    uint8_t  filterStage;
    uint8_t  _pad;
    float    lastOutput;         // last sample returned by voice processSample()
    float    loudnessValue;
    float    filterValue;
    float    ladderStage0;
    float    ladderStage1;
    float    ladderStage2;
    float    ladderStage3;
};

struct CracklePayload {
    uint64_t sessionSample;
    uint64_t blockIndex;
    uint32_t sampleOffset;       // sample inside block where max delta occurred
    uint32_t crackleIndex;       // sequential crackle counter
    float    maxDelta;
    float    prevSample;
    float    curSample;
    float    recentAvgDelta;     // 1-pole LP of per-block maxDelta (slow smoothed)
    uint8_t  activeVoices;
    uint8_t  heldVoices;
    uint8_t  releasingVoices;
    uint8_t  _pad;
    uint64_t lastNoteEventSample;
    uint32_t droppedEvents;
    uint32_t _pad2;
};

// ─── Union event (fixed 128 bytes, cache-line × 2 aligned) ───────────────────
// Header padded to 8 bytes (was 3, giving a 4-byte header) so the union
// below — which holds uint64_t members and therefore needs 8-byte alignment
// — starts exactly on that boundary. At a 4-byte header the compiler had to
// insert 4 more bytes before the union to align it, pushing the whole
// struct (rounded up for alignas(16)) to 144 bytes instead of 128.
struct alignas(16) TraceEvent {
    TraceEventType type;
    uint8_t        _pad[7];
    union {
        BlockPayload    block;
        NotePayload     note;
        AllocPayload    alloc;
        SnapshotPayload snapshot;
        CracklePayload  crackle;
        uint8_t         _raw[120];
    };
};
static_assert(sizeof(TraceEvent) == 128, "TraceEvent must be 128 bytes");

// ─── SPSC ring buffer + writer thread ─────────────────────────────────────────
class PolyTraceLogger {
public:
    static PolyTraceLogger& instance();

    // ── Lifecycle (call from non-audio thread) ────────────────────────────────
    void start();   // creates output dir, opens CSV files, starts writer thread
    void stop();    // drains queue, closes files, joins writer thread

    bool isActive() const noexcept {
        return _active.load(std::memory_order_acquire);
    }

    // ── Current block context (set from audio thread before each block) ───────
    void setBlockContext(uint64_t blockIdx, uint64_t sessionSample,
                         uint32_t blockSize, float sampleRate) noexcept {
        _ctx.blockIndex     = blockIdx;
        _ctx.sessionSample  = sessionSample;
        _ctx.blockSize      = blockSize;
        _ctx.sampleRate     = sampleRate;
    }

    uint64_t currentBlockIndex()    const noexcept { return _ctx.blockIndex; }
    uint64_t currentSessionSample() const noexcept { return _ctx.sessionSample; }
    uint32_t currentBlockSize()     const noexcept { return _ctx.blockSize; }

    // ── Crackle detection parameters ──────────────────────────────────────────
    static constexpr float kAbsoluteThreshold = 0.05f;  // |delta| trigger
    static constexpr float kRatioThreshold    = 2.5f;   // × recent average delta

    float recentAvgDelta() const noexcept {
        return _recentAvgDelta.load(std::memory_order_relaxed);
    }
    void updateRecentAvgDelta(float delta) noexcept {
        const float old = _recentAvgDelta.load(std::memory_order_relaxed);
        _recentAvgDelta.store(old * 0.97f + delta * 0.03f, std::memory_order_relaxed);
    }

    // ── Ring-buffer push (RT-safe, inline) ───────────────────────────────────
    bool push(const TraceEvent& e) noexcept {
        const uint32_t w    = _wPos.load(std::memory_order_relaxed);
        const uint32_t next = (w + 1u) & kRingMask;
        if (next == _rPos.load(std::memory_order_acquire)) {
            _dropped.fetch_add(1u, std::memory_order_relaxed);
            return false;
        }
        _ring[w] = e;
        _wPos.store(next, std::memory_order_release);
        return true;
    }

    // ── Convenience push builders ─────────────────────────────────────────────
    void pushBlock(const BlockPayload& p) noexcept {
        if (!isActive()) return;
        TraceEvent e;
        e.type  = TraceEventType::Block;
        e.block = p;
        push(e);
    }
    void pushNoteOn(const NotePayload& p) noexcept {
        if (!isActive()) return;
        TraceEvent e;
        e.type = TraceEventType::NoteOn;
        e.note = p;
        push(e);
    }
    void pushNoteOff(const NotePayload& p) noexcept {
        if (!isActive()) return;
        TraceEvent e;
        e.type = TraceEventType::NoteOff;
        e.note = p;
        push(e);
    }
    void pushAlloc(const AllocPayload& p) noexcept {
        if (!isActive()) return;
        TraceEvent e;
        e.type  = TraceEventType::VoiceAlloc;
        e.alloc = p;
        push(e);
    }
    void pushSnapshot(const SnapshotPayload& p) noexcept {
        if (!isActive()) return;
        TraceEvent e;
        e.type     = TraceEventType::VoiceSnapshot;
        e.snapshot = p;
        push(e);
    }
    void pushCrackle(const CracklePayload& p) noexcept {
        if (!isActive()) return;
        TraceEvent e;
        e.type    = TraceEventType::Crackle;
        e.crackle = p;
        push(e);
    }

    uint32_t droppedCount() const noexcept {
        return _dropped.load(std::memory_order_relaxed);
    }

    // ── Internal ring pop (writer thread only) ────────────────────────────────
    bool pop(TraceEvent& e) noexcept {
        const uint32_t r = _rPos.load(std::memory_order_relaxed);
        if (r == _wPos.load(std::memory_order_acquire))
            return false;
        e = _ring[r];
        _rPos.store((r + 1u) & kRingMask, std::memory_order_release);
        return true;
    }

private:
    PolyTraceLogger()  = default;
    ~PolyTraceLogger() { stop(); }
    PolyTraceLogger(const PolyTraceLogger&) = delete;
    PolyTraceLogger& operator=(const PolyTraceLogger&) = delete;

    static constexpr uint32_t kRingCapacity = 1u << 13; // 8192 events
    static constexpr uint32_t kRingMask     = kRingCapacity - 1u;

    // Audio-thread-only context (not accessed from writer thread)
    struct BlockCtx {
        uint64_t blockIndex    = 0;
        uint64_t sessionSample = 0;
        uint32_t blockSize     = 0;
        float    sampleRate    = 44100.0f;
    } _ctx;

    // C4324 (struct padded due to alignment specifier) is the intended
    // effect here — alignas(64) puts _wPos and _rPos on separate cache
    // lines so the audio and writer threads don't false-share — but this
    // project builds with warnings-as-errors, so it needs an explicit
    // suppression rather than silently failing the build.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
    alignas(64) std::atomic<uint32_t> _wPos{0};  // written by audio thread
    alignas(64) std::atomic<uint32_t> _rPos{0};  // written by writer thread
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    std::atomic<uint32_t> _dropped{0};
    std::atomic<bool>     _active{false};
    std::atomic<float>    _recentAvgDelta{0.0f};

    std::array<TraceEvent, kRingCapacity> _ring{};

    // Writer thread and file handles live in Impl (defined in .cpp) to keep
    // <thread> / <fstream> out of the audio-thread-side header.
    void _writerMain();
    void _writeEvent(const TraceEvent& e);

    struct Impl;
    Impl* _impl = nullptr;
};

} // namespace SynthCore

// ─── Convenience macro (zero overhead when disabled) ─────────────────────────
#define POLY_TRACE_PUSH_BLOCK(p)    ::SynthCore::PolyTraceLogger::instance().pushBlock(p)
#define POLY_TRACE_PUSH_NOTE_ON(p)  ::SynthCore::PolyTraceLogger::instance().pushNoteOn(p)
#define POLY_TRACE_PUSH_NOTE_OFF(p) ::SynthCore::PolyTraceLogger::instance().pushNoteOff(p)
#define POLY_TRACE_PUSH_ALLOC(p)    ::SynthCore::PolyTraceLogger::instance().pushAlloc(p)
#define POLY_TRACE_PUSH_SNAPSHOT(p) ::SynthCore::PolyTraceLogger::instance().pushSnapshot(p)
#define POLY_TRACE_PUSH_CRACKLE(p)  ::SynthCore::PolyTraceLogger::instance().pushCrackle(p)
#define POLY_TRACE_SET_CTX(bi,ss,bs,sr) ::SynthCore::PolyTraceLogger::instance().setBlockContext(bi,ss,bs,sr)

#else // LADDERVOICE_ENABLE_POLY_TRACE == 0

// Stubs: everything compiles to nothing
#define POLY_TRACE_PUSH_BLOCK(p)        ((void)0)
#define POLY_TRACE_PUSH_NOTE_ON(p)      ((void)0)
#define POLY_TRACE_PUSH_NOTE_OFF(p)     ((void)0)
#define POLY_TRACE_PUSH_ALLOC(p)        ((void)0)
#define POLY_TRACE_PUSH_SNAPSHOT(p)     ((void)0)
#define POLY_TRACE_PUSH_CRACKLE(p)      ((void)0)
#define POLY_TRACE_SET_CTX(bi,ss,bs,sr) ((void)0)

#endif // LADDERVOICE_ENABLE_POLY_TRACE
