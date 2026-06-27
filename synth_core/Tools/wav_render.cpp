// Renders 5 seconds of a single MIDI note to output.wav using SynthCoreLib.
// No JUCE, no UI, no VST — pure C++17 stdlib + SynthCoreLib.
#include "../Include/SynthEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

using namespace SynthCore;

static void writeLE16(std::ofstream& f, uint16_t v)
{
    uint8_t buf[2] = { static_cast<uint8_t>(v & 0xFFu), static_cast<uint8_t>(v >> 8u) };
    f.write(reinterpret_cast<const char*>(buf), 2);
}

static void writeLE32(std::ofstream& f, uint32_t v)
{
    uint8_t buf[4] = {
        static_cast<uint8_t>(v & 0xFFu),
        static_cast<uint8_t>((v >> 8u)  & 0xFFu),
        static_cast<uint8_t>((v >> 16u) & 0xFFu),
        static_cast<uint8_t>((v >> 24u) & 0xFFu)
    };
    f.write(reinterpret_cast<const char*>(buf), 4);
}

int main()
{
    constexpr double   kSampleRate    = 48000.0;
    constexpr int      kDurationSec   = 5;
    constexpr int      kNumFrames     = static_cast<int>(kSampleRate) * kDurationSec;
    constexpr int      kChannels      = 2;
    constexpr int      kBitsPerSample = 16;
    constexpr uint8_t  kNoteA2        = 45;  // A2 = 110 Hz

    SynthEngine engine;
    engine.prepare(kSampleRate, 512);

    std::vector<float> floatBuf(static_cast<size_t>(kNumFrames * kChannels), 0.0f);

    const int noteOnFrame = static_cast<int>(kSampleRate * 0.1);
    const int noteOffFrame = static_cast<int>(kSampleRate * 3.5);
    for (int frame = 0; frame < kNumFrames; ++frame) {
        if (frame == noteOnFrame) {
            engine.noteOn(kNoteA2, 100.0f);
        }
        if (frame == noteOffFrame) {
            engine.noteOff(kNoteA2);
        }

        const float sample = engine.processSample();
        floatBuf[static_cast<size_t>(frame * 2)] = sample;
        floatBuf[static_cast<size_t>(frame * 2 + 1)] = sample;
    }

    // Check for silence before writing
    float maxAbs = 0.0f;
    double sumSquares = 0.0;
    int clippingSamples = 0;
    bool hasNanOrInf = false;
    for (float s : floatBuf) {
        if (!std::isfinite(s)) {
            hasNanOrInf = true;
            continue;
        }
        maxAbs = std::max(maxAbs, std::abs(s));
        sumSquares += static_cast<double>(s) * s;
        if (std::abs(s) >= 0.98f) ++clippingSamples;
    }
    const double rms = std::sqrt(sumSquares / static_cast<double>(floatBuf.size()));

    std::cout << "Output path: C:\\Dev\\Projects\\MinimoogSynth\\output.wav\n";
    std::cout << "Sample rate: " << kSampleRate << " Hz\n";
    std::cout << "Duration: " << kDurationSec << " s\n";
    std::cout << "Peak amplitude: " << maxAbs << "\n";
    std::cout << "RMS amplitude: " << rms << "\n";
    std::cout << "Clipping samples: " << clippingSamples << "\n";
    std::cout << "NaN/Inf detected: " << (hasNanOrInf ? "yes" : "no") << "\n";
    if (hasNanOrInf) {
        std::cerr << "ERROR: rendered audio contains NaN/Inf.\n";
        return 1;
    }
    if (maxAbs < 0.001f) {
        std::cerr << "ERROR: rendered audio is silent — check oscillator/envelope chain.\n";
        return 1;
    }

    // Convert to 16-bit PCM
    std::vector<int16_t> pcm(floatBuf.size());
    for (size_t i = 0; i < floatBuf.size(); ++i) {
        float s = std::clamp(floatBuf[i], -1.0f, 1.0f);
        pcm[i] = static_cast<int16_t>(s * 32767.0f);
    }

    // Write WAV
    const char* outPath = "output.wav";
    std::ofstream f(outPath, std::ios::binary);
    if (!f) {
        std::cerr << "ERROR: failed to open " << outPath << " for writing.\n";
        return 1;
    }

    uint32_t dataSize  = static_cast<uint32_t>(pcm.size()) * static_cast<uint32_t>(kBitsPerSample / 8);
    uint32_t fmtSize   = 16u;
    uint32_t riffSize  = 4u + (8u + fmtSize) + (8u + dataSize);

    f.write("RIFF", 4);
    writeLE32(f, riffSize);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    writeLE32(f, fmtSize);
    writeLE16(f, 1u);                                                           // PCM
    writeLE16(f, static_cast<uint16_t>(kChannels));
    writeLE32(f, static_cast<uint32_t>(kSampleRate));
    writeLE32(f, static_cast<uint32_t>(kSampleRate) * static_cast<uint32_t>(kChannels) * static_cast<uint32_t>(kBitsPerSample / 8));
    writeLE16(f, static_cast<uint16_t>(kChannels * (kBitsPerSample / 8)));
    writeLE16(f, static_cast<uint16_t>(kBitsPerSample));

    f.write("data", 4);
    writeLE32(f, dataSize);
    f.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(dataSize));

    if (!f) {
        std::cerr << "ERROR: write failed.\n";
        return 1;
    }

    std::cout << "Wrote " << kNumFrames << " frames (" << kDurationSec
              << "s, " << kSampleRate << " Hz stereo 16-bit) to " << outPath << "\n";
    return 0;
}
