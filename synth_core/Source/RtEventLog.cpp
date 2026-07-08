#include "../Include/RtEventLog.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace SynthCore {

struct RtEventLog::Impl {
    std::ofstream out;
    std::thread   writerThread;
    std::atomic<bool> shouldStop{false};
    std::chrono::steady_clock::time_point startTime;
};

RtEventLog& RtEventLog::instance()
{
    static RtEventLog inst;
    return inst;
}

uint64_t RtEventLog::_elapsedMs() const noexcept
{
    if (!_impl) return 0;
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - _impl->startTime).count());
}

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

static const char* eventTypeStr(RtEventType t)
{
    switch (t) {
    case RtEventType::Block:       return "Block";
    case RtEventType::NoteOnMsg:   return "NoteOn";
    case RtEventType::NoteOffMsg:  return "NoteOff";
    case RtEventType::MidiOther:   return "MidiOther";
    case RtEventType::VoiceAlloc:  return "VoiceAlloc";
    case RtEventType::EnvGateOn:   return "EnvGateOn";
    case RtEventType::EnvGateOff:  return "EnvGateOff";
    case RtEventType::VoiceReset:  return "VoiceReset";
    case RtEventType::ParamChange: return "ParamChange";
    case RtEventType::CpuWarning:  return "CpuWarning";
    }
    return "Unknown";
}

static const char* allocReasonStr(RtAllocReason r)
{
    switch (r) {
    case RtAllocReason::SameNote:      return "SameNote";
    case RtAllocReason::FreeSlot:      return "FreeSlot";
    case RtAllocReason::ReleaseTail:   return "ReleaseTail";
    case RtAllocReason::Oldest:        return "Oldest";
    case RtAllocReason::NotApplicable: return "";
    }
    return "";
}

void RtEventLog::start()
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    const char* envVal = std::getenv("LADDERVOICE_RT_LOG");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (!envVal || std::string(envVal) != "1")
        return;

    if (_active.load(std::memory_order_acquire))
        return;

    _impl = new Impl();
    _impl->startTime = std::chrono::steady_clock::now();

    const std::string dir = std::string(".tmp/live_rt_log/") + timestampString();
    _sessionDirPublic = dir;

    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        delete _impl;
        _impl = nullptr;
        return;
    }

    _impl->out.open(dir + "/rt_events.csv", std::ios::out | std::ios::trunc);
    _impl->out.precision(9);
    _impl->out << "wallClockMs,sessionSample,blockIndex,sampleOffset,type,voiceIndex,"
                  "note,velocity,reason,paramId,paramValue,blockSize,sampleRate\n";

    _wPos.store(0, std::memory_order_relaxed);
    _rPos.store(0, std::memory_order_relaxed);
    _dropped.store(0, std::memory_order_relaxed);

    _active.store(true, std::memory_order_release);

    _impl->writerThread = std::thread([this] { _writerMain(); });

    std::fputs(("RtEventLog: ACTIVE, writing to " + dir + "/rt_events.csv\n").c_str(), stdout);
    std::fflush(stdout);
}

void RtEventLog::stop()
{
    if (!_active.load(std::memory_order_acquire))
        return;

    _active.store(false, std::memory_order_release);

    if (_impl) {
        _impl->shouldStop.store(true, std::memory_order_release);
        if (_impl->writerThread.joinable())
            _impl->writerThread.join();

        RtEvent e;
        while (pop(e)) _writeEvent(e);

        _impl->out << "# total_dropped," << _dropped.load() << '\n';
        _impl->out.flush();

        delete _impl;
        _impl = nullptr;
    }
}

void RtEventLog::_writerMain()
{
    RtEvent e;
    while (!_impl->shouldStop.load(std::memory_order_acquire)) {
        bool drainedAny = false;
        while (pop(e)) { _writeEvent(e); drainedAny = true; }
        if (drainedAny) _impl->out.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void RtEventLog::_writeEvent(const RtEvent& e)
{
    if (!_impl) return;
    _impl->out << e.wallClockMs << ',' << e.sessionSample << ',' << e.blockIndex << ','
                << e.sampleOffset << ',' << eventTypeStr(e.type) << ','
                << static_cast<int>(e.voiceIndex) << ',' << e.note << ','
                << static_cast<int>(e.velocity) << ',' << allocReasonStr(e.reason) << ','
                << e.paramId << ',' << e.paramValue << ',' << e.blockSize << ','
                << e.sampleRate << '\n';
}

} // namespace SynthCore
