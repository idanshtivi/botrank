#include "../Include/NoteOnAudioCapture.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <thread>

namespace SynthCore {

struct NoteOnAudioCapture::Impl {
    std::thread writerThread;
    std::atomic<bool> shouldStop{false};
    std::chrono::steady_clock::time_point startTime;
};

NoteOnAudioCapture& NoteOnAudioCapture::instance()
{
    static NoteOnAudioCapture inst;
    return inst;
}

uint64_t NoteOnAudioCapture::_elapsedMs() const noexcept
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

namespace {
void writeLE16(std::ofstream& f, uint16_t v) {
    uint8_t buf[2] = { static_cast<uint8_t>(v & 0xFFu), static_cast<uint8_t>(v >> 8u) };
    f.write(reinterpret_cast<const char*>(buf), 2);
}
void writeLE32(std::ofstream& f, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>(v & 0xFFu), static_cast<uint8_t>((v >> 8u) & 0xFFu),
        static_cast<uint8_t>((v >> 16u) & 0xFFu), static_cast<uint8_t>((v >> 24u) & 0xFFu)
    };
    f.write(reinterpret_cast<const char*>(buf), 4);
}
} // namespace

void NoteOnAudioCapture::start(double sampleRate)
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    const char* envVal = std::getenv("LADDERVOICE_NOTEON_CAPTURE");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (!envVal || std::string(envVal) != "1")
        return;

    if (_active.load(std::memory_order_acquire))
        return;

    _sampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;

    // Ring buffer: comfortably larger than pre-roll + post-roll so a capture
    // in flight is never overwritten before it's extracted. 2 seconds at the
    // current sample rate is generous headroom for 250ms+500ms windows.
    _ringCapacity = static_cast<uint32_t>(_sampleRate * 2.0);
    _ring.assign(_ringCapacity, 0.0f);
    _totalWritten = 0;
    for (auto& p : _pending) p = PendingCapture{};

    const size_t maxSnapshotLen = static_cast<size_t>(_sampleRate * 0.80); // 250ms + 500ms + margin
    for (auto& snap : _snapshotPool) {
        snap.samples.assign(maxSnapshotLen, 0.0f);
        snap.sampleRate = _sampleRate;
    }
    for (auto& r : _snapshotReady) r.store(false, std::memory_order_relaxed);
    _snapshotWritePos.store(0, std::memory_order_relaxed);

    _impl = new Impl();
    _impl->startTime = std::chrono::steady_clock::now();

    const std::string dir = std::string(".tmp/live_noteon_capture/") + timestampString();
    _sessionDirPublic = dir;
    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        delete _impl;
        _impl = nullptr;
        return;
    }

    _active.store(true, std::memory_order_release);
    _impl->writerThread = std::thread([this] { _writerMain(); });

    std::fputs(("NoteOnAudioCapture: ACTIVE, writing snippets to " + dir + "\n").c_str(), stdout);
    std::fflush(stdout);
}

void NoteOnAudioCapture::stop()
{
    if (!_active.load(std::memory_order_acquire))
        return;

    _active.store(false, std::memory_order_release);

    if (_impl) {
        _impl->shouldStop.store(true, std::memory_order_release);
        if (_impl->writerThread.joinable())
            _impl->writerThread.join();
        delete _impl;
        _impl = nullptr;
    }
}

void NoteOnAudioCapture::pushSample(float sample) noexcept
{
    if (!isActive() || _ringCapacity == 0) return;
    _ring[static_cast<uint32_t>(_totalWritten % _ringCapacity)] = sample;
    ++_totalWritten;
}

void NoteOnAudioCapture::onNoteOn(int midiNote, uint8_t velocity) noexcept
{
    if (!isActive()) return;

    const uint64_t preRoll = static_cast<uint64_t>(_sampleRate * 0.250);
    const uint64_t postRoll = static_cast<uint64_t>(_sampleRate * 0.500);

    for (auto& p : _pending) {
        if (!p.active) {
            p.active = true;
            p.noteOnSample = _totalWritten;
            p.captureEndSample = _totalWritten + postRoll;
            p.preRollSamples = preRoll;
            p.midiNote = midiNote;
            p.velocity = velocity;
            p.wallClockMs = _elapsedMs();
            return;
        }
    }
    // All pending slots full (many rapid notes with no free slot yet) --
    // drop this one silently; this is diagnostic tooling, not the DSP path.
}

void NoteOnAudioCapture::serviceCompletedCaptures() noexcept
{
    if (!isActive()) return;

    for (auto& p : _pending) {
        if (!p.active || _totalWritten < p.captureEndSample) continue;

        const uint64_t rangeStart = (p.noteOnSample >= p.preRollSamples)
            ? p.noteOnSample - p.preRollSamples : 0;
        const uint64_t len = p.captureEndSample - rangeStart;

        const uint32_t slot = _snapshotWritePos.fetch_add(1, std::memory_order_relaxed) % kSnapshotPoolSize;
        if (!_snapshotReady[slot].load(std::memory_order_acquire)) {
            auto& snap = _snapshotPool[slot];
            const size_t copyLen = std::min(static_cast<size_t>(len), snap.samples.size());
            for (size_t i = 0; i < copyLen; ++i) {
                const uint64_t srcIdx = (rangeStart + i) % _ringCapacity;
                snap.samples[i] = _ring[static_cast<uint32_t>(srcIdx)];
            }
            snap.validLength = copyLen; // fixed-capacity buffer; only this many samples are valid
            snap.sampleRate = _sampleRate;
            snap.midiNote = p.midiNote;
            snap.velocity = p.velocity;
            snap.wallClockMs = p.wallClockMs;
            _snapshotReady[slot].store(true, std::memory_order_release);
        }
        p.active = false;
    }
}

void NoteOnAudioCapture::_writerMain()
{
    while (!_impl->shouldStop.load(std::memory_order_acquire)) {
        bool didWork = false;
        for (int i = 0; i < kSnapshotPoolSize; ++i) {
            if (_snapshotReady[static_cast<size_t>(i)].load(std::memory_order_acquire)) {
                _writeSnapshotToDisk(_snapshotPool[static_cast<size_t>(i)], i);
                _snapshotReady[static_cast<size_t>(i)].store(false, std::memory_order_release);
                didWork = true;
            }
        }
        if (!didWork)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // Drain once more on shutdown.
    for (int i = 0; i < kSnapshotPoolSize; ++i) {
        if (_snapshotReady[static_cast<size_t>(i)].load(std::memory_order_acquire)) {
            _writeSnapshotToDisk(_snapshotPool[static_cast<size_t>(i)], i);
            _snapshotReady[static_cast<size_t>(i)].store(false, std::memory_order_release);
        }
    }
}

void NoteOnAudioCapture::_writeSnapshotToDisk(const Snapshot& snap, int index)
{
    const std::string path = _sessionDirPublic + "/noteon_" + std::to_string(snap.wallClockMs)
        + "ms_note" + std::to_string(snap.midiNote) + "_vel" + std::to_string(static_cast<int>(snap.velocity))
        + "_slot" + std::to_string(index) + ".wav";

    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    std::vector<int16_t> pcm(snap.validLength);
    for (size_t i = 0; i < snap.validLength; ++i) {
        const float s = std::clamp(snap.samples[i], -1.0f, 1.0f);
        pcm[i] = static_cast<int16_t>(s * 32767.0f);
    }

    const uint32_t dataSize = static_cast<uint32_t>(pcm.size()) * 2u;
    const uint32_t fmtSize = 16u;
    const uint32_t riffSize = 4u + (8u + fmtSize) + (8u + dataSize);
    f.write("RIFF", 4); writeLE32(f, riffSize); f.write("WAVE", 4);
    f.write("fmt ", 4); writeLE32(f, fmtSize);
    writeLE16(f, 1u); writeLE16(f, 1u); // mono -- the exact signal, not duplicated to stereo
    writeLE32(f, static_cast<uint32_t>(snap.sampleRate));
    writeLE32(f, static_cast<uint32_t>(snap.sampleRate) * 2u);
    writeLE16(f, 2u); writeLE16(f, 16u);
    f.write("data", 4); writeLE32(f, dataSize);
    f.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(dataSize));
}

} // namespace SynthCore
