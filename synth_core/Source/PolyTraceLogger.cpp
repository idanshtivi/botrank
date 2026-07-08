#include "../Include/PolyTraceLogger.h"

#if LADDERVOICE_ENABLE_POLY_TRACE

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>

namespace SynthCore {

// ─── Impl: file handles + writer thread ──────────────────────────────────────
struct PolyTraceLogger::Impl {
    std::ofstream fBlock;
    std::ofstream fNote;
    std::ofstream fAlloc;
    std::ofstream fCrackle;
    std::ofstream fSnapshot;
    std::ofstream fSummary;

    std::thread           writerThread;
    std::atomic<bool>     shouldStop{false};
    uint64_t              totalBlockEvents   = 0;
    uint64_t              totalNoteEvents    = 0;
    uint64_t              totalAllocEvents   = 0;
    uint64_t              totalCrackleEvents = 0;
    uint64_t              totalDropped       = 0;
    std::chrono::steady_clock::time_point startTime;
    std::string           sessionDir;
};

// ─── Singleton ────────────────────────────────────────────────────────────────
PolyTraceLogger& PolyTraceLogger::instance()
{
    static PolyTraceLogger inst;
    return inst;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
static std::string timestampString()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
#ifdef _WIN32
    struct tm tm{};
    localtime_s(&tm, &t);
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
#else
    struct tm* tm = std::localtime(&t);
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", tm);
#endif
    return buf;
}

static void writeBlockHeader(std::ofstream& f)
{
    f << "sessionSample,blockIndex,blockSize,sampleRate,playMode,"
         "activeVoices,heldVoices,releasingVoices,"
         "finalPeak,finalRms,maxDelta,maxDeltaOffset,"
         "polySumPeak,blockTimeUs,cpuPercent,droppedEvents\n";
}
static void writeNoteHeader(std::ofstream& f)
{
    f << "sessionSample,blockIndex,sampleOffset,eventType,midiNote,velocity,"
         "voiceIndex,wasHeld,wasActive,isReleaseTailReuse,"
         "activeBefore,heldBefore,releasingBefore,"
         "activeAfter,heldAfter,releasingAfter\n";
}
static void writeAllocHeader(std::ofstream& f)
{
    f << "sessionSample,blockIndex,requestedNote,chosenVoice,reason,wasReset,"
         "v0active,v0held,v0note,v0stage,v0env,"
         "v1active,v1held,v1note,v1stage,v1env,"
         "v2active,v2held,v2note,v2stage,v2env,"
         "v3active,v3held,v3note,v3stage,v3env\n";
}
static void writeCrackleHeader(std::ofstream& f)
{
    f << "sessionSample,blockIndex,sampleOffset,crackleIndex,"
         "maxDelta,prevSample,curSample,recentAvgDelta,"
         "activeVoices,heldVoices,releasingVoices,playModeAtCrackle,"
         "lastNoteEventSample,droppedEvents,"
         "monoVoiceActive,monoLoudnessValue,monoFilterValue\n";
}
static void writeSnapshotHeader(std::ofstream& f)
{
    f << "sessionSample,blockIndex,crackleIndex,voiceIndex,"
         "isActive,isHeld,isReleasing,midiNote,"
         "loudnessStage,filterStage,"
         "loudnessValue,filterValue,"
         "ladderStage0,ladderStage1,ladderStage2,ladderStage3\n";
}

static const char* allocReasonStr(uint8_t r)
{
    switch (r) {
    case 0: return "SameNote";
    case 1: return "FreeSlot";
    case 2: return "ReleaseTail";
    case 3: return "Oldest";
    default: return "Unknown";
    }
}

static void writeBlockRow(std::ofstream& f, const BlockPayload& p)
{
    f << p.sessionSample << ','
      << p.blockIndex    << ','
      << p.blockSize     << ','
      << p.sampleRate    << ','
      << static_cast<int>(p.playMode)          << ','
      << static_cast<int>(p.activeVoices)      << ','
      << static_cast<int>(p.heldVoices)        << ','
      << static_cast<int>(p.releasingVoices)   << ','
      << p.finalPeak     << ','
      << p.finalRms      << ','
      << p.maxDelta      << ','
      << p.maxDeltaOffset << ','
      << p.polySumPeak   << ','
      << p.blockTimeUs   << ','
      << p.cpuPercent    << ','
      << p.droppedEvents << '\n';
}

static void writeNoteRow(std::ofstream& f, const NotePayload& p, const char* type)
{
    f << p.sessionSample                             << ','
      << p.blockIndex                                << ','
      << p.sampleOffset                              << ','
      << type                                        << ','
      << static_cast<int>(p.midiNote)                << ','
      << static_cast<int>(p.velocity)                << ','
      << static_cast<int>(p.voiceIndex)              << ','
      << static_cast<int>(p.wasHeld)                 << ','
      << static_cast<int>(p.wasActive)               << ','
      << static_cast<int>(p.isReleaseTailReuse)      << ','
      << static_cast<int>(p.activeBefore)            << ','
      << static_cast<int>(p.heldBefore)              << ','
      << static_cast<int>(p.releasingBefore)         << ','
      << static_cast<int>(p.activeAfter)             << ','
      << static_cast<int>(p.heldAfter)               << ','
      << static_cast<int>(p.releasingAfter)          << '\n';
}

static void writeAllocRow(std::ofstream& f, const AllocPayload& p)
{
    f << p.sessionSample                       << ','
      << p.blockIndex                          << ','
      << static_cast<int>(p.requestedNote)     << ','
      << static_cast<int>(p.chosenVoice)       << ','
      << allocReasonStr(p.reason)              << ','
      << static_cast<int>(p.wasReset)          << ',';
    for (int i = 0; i < 4; ++i) {
        const auto& v = p.voicesBefore[i];
        f << static_cast<int>(v.active)   << ','
          << static_cast<int>(v.held)     << ','
          << static_cast<int>(v.midiNote) << ','
          << static_cast<int>(v.envStage) << ','
          << v.envValue;
        if (i < 3) f << ',';
    }
    f << '\n';
}

static void writeCrackleRow(std::ofstream& f, const CracklePayload& p)
{
    f << p.sessionSample                       << ','
      << p.blockIndex                          << ','
      << p.sampleOffset                        << ','
      << p.crackleIndex                        << ','
      << p.maxDelta                            << ','
      << p.prevSample                          << ','
      << p.curSample                           << ','
      << p.recentAvgDelta                      << ','
      << static_cast<int>(p.activeVoices)      << ','
      << static_cast<int>(p.heldVoices)        << ','
      << static_cast<int>(p.releasingVoices)   << ','
      << static_cast<int>(p.playModeAtCrackle) << ','
      << p.lastNoteEventSample                 << ','
      << p.droppedEvents                       << ','
      << static_cast<int>(p.monoVoiceActive)   << ','
      << p.monoLoudnessValue                   << ','
      << p.monoFilterValue                     << '\n';
}

static void writeSnapshotRow(std::ofstream& f, const SnapshotPayload& p)
{
    f << p.sessionSample                       << ','
      << p.blockIndex                          << ','
      << p.crackleIndex                        << ','
      << static_cast<int>(p.voiceIndex)        << ','
      << static_cast<int>(p.isActive)          << ','
      << static_cast<int>(p.isHeld)            << ','
      << static_cast<int>(p.isReleasing)       << ','
      << static_cast<int>(p.midiNote)          << ','
      << static_cast<int>(p.loudnessStage)     << ','
      << static_cast<int>(p.filterStage)       << ','
      << p.loudnessValue                       << ','
      << p.filterValue                         << ','
      << p.ladderStage0                        << ','
      << p.ladderStage1                        << ','
      << p.ladderStage2                        << ','
      << p.ladderStage3                        << '\n';
}

// ─── start() ─────────────────────────────────────────────────────────────────
void PolyTraceLogger::start()
{
    // Runtime gate: require env var LADDERVOICE_POLY_TRACE=1
    // getenv is deprecated in favour of _dupenv_s on MSVC, but this project
    // builds with warnings-as-errors so it needs an explicit suppression
    // rather than switching to the non-portable replacement.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    const char* envVal = std::getenv("LADDERVOICE_POLY_TRACE");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (!envVal || std::string(envVal) != "1")
        return;

    if (_active.load(std::memory_order_acquire))
        return; // already running

    _impl = new Impl();
    _impl->startTime = std::chrono::steady_clock::now();

    // Create session directory: .tmp/live_poly_trace/<timestamp>/
    const std::string ts = timestampString();
    const std::string dir = std::string(".tmp/live_poly_trace/") + ts;
    _impl->sessionDir = dir;
    _sessionDirPublic = dir;

    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        delete _impl;
        _impl = nullptr;
        return;
    }

    auto open = [&](std::ofstream& f, const char* name) {
        f.open(dir + "/" + name, std::ios::out | std::ios::trunc);
        f.precision(8);
    };

    open(_impl->fBlock,    "block_trace.csv");
    open(_impl->fNote,     "note_events.csv");
    open(_impl->fAlloc,    "voice_allocations.csv");
    open(_impl->fCrackle,  "crackle_events.csv");
    open(_impl->fSnapshot, "voice_state_snapshots.csv");
    open(_impl->fSummary,  "session_summary.csv");

    writeBlockHeader(_impl->fBlock);
    writeNoteHeader(_impl->fNote);
    writeAllocHeader(_impl->fAlloc);
    writeCrackleHeader(_impl->fCrackle);
    writeSnapshotHeader(_impl->fSnapshot);

    _impl->fSummary << "key,value\n"
                    << "session_dir," << dir << '\n'
                    << "start_time," << ts << '\n';
    _impl->fSummary.flush();

    // Reset ring
    _wPos.store(0, std::memory_order_relaxed);
    _rPos.store(0, std::memory_order_relaxed);
    _dropped.store(0, std::memory_order_relaxed);
    _recentAvgDelta.store(0.0f, std::memory_order_relaxed);

    _active.store(true, std::memory_order_release);

    _impl->writerThread = std::thread([this]{ _writerMain(); });
}

// ─── stop() ──────────────────────────────────────────────────────────────────
void PolyTraceLogger::stop()
{
    if (!_active.load(std::memory_order_acquire))
        return;

    _active.store(false, std::memory_order_release);

    if (_impl) {
        _impl->shouldStop.store(true, std::memory_order_release);
        if (_impl->writerThread.joinable())
            _impl->writerThread.join();

        // Drain remaining events
        TraceEvent e;
        while (pop(e)) {
            _writeEvent(e);
        }

        // Write summary footer
        const auto endTime = std::chrono::steady_clock::now();
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - _impl->startTime).count();
        _impl->fSummary
            << "duration_ms,"        << durationMs                  << '\n'
            << "total_blocks,"       << _impl->totalBlockEvents      << '\n'
            << "total_notes,"        << _impl->totalNoteEvents       << '\n'
            << "total_allocs,"       << _impl->totalAllocEvents      << '\n'
            << "total_crackles,"     << _impl->totalCrackleEvents    << '\n'
            << "total_dropped,"      << _dropped.load()              << '\n';

        _impl->fSummary.flush();
        _impl->fBlock.flush();
        _impl->fNote.flush();
        _impl->fAlloc.flush();
        _impl->fCrackle.flush();
        _impl->fSnapshot.flush();

        delete _impl;
        _impl = nullptr;
    }
}

// ─── Writer thread ────────────────────────────────────────────────────────────
void PolyTraceLogger::_writerMain()
{
    TraceEvent e;
    while (!_impl->shouldStop.load(std::memory_order_acquire)) {
        bool drainedAny = false;
        while (pop(e)) {
            _writeEvent(e);
            drainedAny = true;
        }
        if (drainedAny) {
            // Flush periodically when there was activity
            _impl->fBlock.flush();
            _impl->fNote.flush();
            _impl->fAlloc.flush();
            _impl->fCrackle.flush();
            _impl->fSnapshot.flush();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// ─── Dispatch ────────────────────────────────────────────────────────────────
void PolyTraceLogger::_writeEvent(const TraceEvent& e)
{
    if (!_impl) return;
    switch (e.type) {
    case TraceEventType::Block:
        writeBlockRow(_impl->fBlock, e.block);
        ++_impl->totalBlockEvents;
        break;
    case TraceEventType::NoteOn:
        writeNoteRow(_impl->fNote, e.note, "NoteOn");
        ++_impl->totalNoteEvents;
        break;
    case TraceEventType::NoteOff:
        writeNoteRow(_impl->fNote, e.note, "NoteOff");
        ++_impl->totalNoteEvents;
        break;
    case TraceEventType::VoiceAlloc:
        writeAllocRow(_impl->fAlloc, e.alloc);
        ++_impl->totalAllocEvents;
        break;
    case TraceEventType::VoiceSnapshot:
        writeSnapshotRow(_impl->fSnapshot, e.snapshot);
        break;
    case TraceEventType::Crackle:
        writeCrackleRow(_impl->fCrackle, e.crackle);
        ++_impl->totalCrackleEvents;
        break;
    }
}

} // namespace SynthCore

#endif // LADDERVOICE_ENABLE_POLY_TRACE
