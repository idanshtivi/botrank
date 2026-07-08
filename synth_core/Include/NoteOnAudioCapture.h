#pragma once

// ── Live NoteOn audio-snippet capture (real Release build, no PolyTrace) ────
// Independent of PolyTraceLogger and RtEventLog entirely. Purely observes the
// actual output stream: keeps a rolling ring buffer of the final audio (the
// exact samples sent to the speakers), and on every NoteOn writes out a WAV
// snippet spanning 250ms before through 500ms after that note, so a reported
// click can be inspected in the real captured waveform instead of a
// synthetic offline re-creation.
//
// Purely additive: pushSample() is a ring-buffer write + counter increment
// (no DSP, no allocation). onNoteOn() records a pending capture (fixed-size
// array, no allocation). The only heavier work -- one memcpy per completed
// capture, and all file I/O -- happens in serviceCompletedCaptures(), which
// the caller should invoke once per audio block; the actual WAV write runs
// on a background thread, never on the audio thread.
//
// Runtime gate: env var LADDERVOICE_NOTEON_CAPTURE=1 (checked in start()).

#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SynthCore {

class NoteOnAudioCapture {
public:
    static NoteOnAudioCapture& instance();

    // Allocates ring/pool buffers sized for sampleRate and starts the writer
    // thread. Runtime-gated: no-ops unless LADDERVOICE_NOTEON_CAPTURE=1.
    void start(double sampleRate);
    void stop();
    bool isActive() const noexcept { return _active.load(std::memory_order_acquire); }
    const std::string& sessionDirectory() const noexcept { return _sessionDirPublic; }

    // Call once per sample from the audio thread with the actual output
    // value (post-everything -- the real signal being sent to the speakers).
    void pushSample(float sample) noexcept;

    // Call when a NoteOn MIDI message is processed, at the exact sample
    // where its audio will begin (i.e. before pushSample() for that sample).
    void onNoteOn(int midiNote, uint8_t velocity) noexcept;

    // Call once per audio block (after all of that block's pushSample calls)
    // to hand off any captures whose post-roll window has completed.
    void serviceCompletedCaptures() noexcept;

private:
    NoteOnAudioCapture() = default;
    ~NoteOnAudioCapture() { stop(); }
    NoteOnAudioCapture(const NoteOnAudioCapture&) = delete;
    NoteOnAudioCapture& operator=(const NoteOnAudioCapture&) = delete;

    static constexpr int kMaxPendingCaptures = 16;
    static constexpr int kSnapshotPoolSize = 8;

    struct PendingCapture {
        bool active = false;
        uint64_t noteOnSample = 0;
        uint64_t captureEndSample = 0;
        uint64_t preRollSamples = 0;
        int midiNote = -1;
        uint8_t velocity = 0;
        uint64_t wallClockMs = 0;
    };

    struct Snapshot {
        std::vector<float> samples; // fixed capacity, allocated once in start()
        size_t validLength = 0;     // how many of `samples` are valid for this capture
        double sampleRate = 44100.0;
        int midiNote = -1;
        uint8_t velocity = 0;
        uint64_t wallClockMs = 0;
    };

    double _sampleRate = 44100.0;
    uint64_t _totalWritten = 0;
    std::vector<float> _ring;
    uint32_t _ringCapacity = 0;

    std::array<PendingCapture, kMaxPendingCaptures> _pending{};

    // Fixed pool of snapshot buffers + a simple SPSC index queue handed to
    // the writer thread -- avoids any allocation on the audio thread once
    // start() has pre-sized everything.
    std::array<Snapshot, kSnapshotPoolSize> _snapshotPool;
    std::array<std::atomic<bool>, kSnapshotPoolSize> _snapshotReady{};
    std::atomic<uint32_t> _snapshotWritePos{0};

    std::atomic<bool> _active{false};
    std::string _sessionDirPublic;

    struct Impl;
    Impl* _impl = nullptr;

    void _writerMain();
    void _writeSnapshotToDisk(const Snapshot& snap, int index);

    uint64_t _elapsedMs() const noexcept;
};

} // namespace SynthCore
