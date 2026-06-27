#include <gtest/gtest.h>
#include "../Include/NoiseGenerator.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace SynthCore;

static std::vector<double> renderNoise(NoiseMode mode, uint32_t seed, int samples)
{
    NoiseGenerator noise;
    noise.setSampleRate(48000.0);
    noise.setMode(mode);
    noise.setSeed(seed);
    std::vector<double> out(static_cast<size_t>(samples));
    for (auto& s : out) s = noise.processSample();
    return out;
}

static double averageAbsDiff(const std::vector<double>& data)
{
    double sum = 0.0;
    for (size_t i = 1; i < data.size(); ++i) sum += std::abs(data[i] - data[i - 1]);
    return sum / static_cast<double>(data.size() - 1);
}

TEST(NoiseGeneratorTest, WhiteAndPinkAreNotSilentAndDiffer)
{
    const auto white = renderNoise(NoiseMode::White, 1234u, 4096);
    const auto pink = renderNoise(NoiseMode::Pink, 1234u, 4096);

    double whitePeak = 0.0;
    double pinkPeak = 0.0;
    double diff = 0.0;
    for (size_t i = 0; i < white.size(); ++i) {
        whitePeak = std::max(whitePeak, std::abs(white[i]));
        pinkPeak = std::max(pinkPeak, std::abs(pink[i]));
        diff += std::abs(white[i] - pink[i]);
    }

    EXPECT_GT(whitePeak, 0.1);
    EXPECT_GT(pinkPeak, 0.02);
    EXPECT_GT(diff / static_cast<double>(white.size()), 0.05);
}

TEST(NoiseGeneratorTest, SameSeedIsDeterministic)
{
    EXPECT_EQ(renderNoise(NoiseMode::White, 77u, 256), renderNoise(NoiseMode::White, 77u, 256));
    EXPECT_EQ(renderNoise(NoiseMode::Pink, 77u, 256), renderNoise(NoiseMode::Pink, 77u, 256));
}

TEST(NoiseGeneratorTest, DifferentSeedsDiffer)
{
    const auto a = renderNoise(NoiseMode::White, 1u, 256);
    const auto b = renderNoise(NoiseMode::White, 2u, 256);
    EXPECT_NE(a, b);
}

TEST(NoiseGeneratorTest, OutputIsBoundedAndFinite)
{
    for (NoiseMode mode : {NoiseMode::White, NoiseMode::Pink}) {
        const auto data = renderNoise(mode, 99u, 48000);
        for (double sample : data) {
            EXPECT_TRUE(std::isfinite(sample));
            EXPECT_GE(sample, -1.0);
            EXPECT_LE(sample, 1.0);
        }
    }
}

TEST(NoiseGeneratorTest, ResetWithFixedSeedReproducesSequence)
{
    NoiseGenerator noise;
    noise.setSeed(42u);
    std::vector<double> a(128);
    std::vector<double> b(128);
    for (auto& s : a) s = noise.processSample();
    noise.reset();
    for (auto& s : b) s = noise.processSample();
    EXPECT_EQ(a, b);
}

TEST(NoiseGeneratorTest, PinkHasLowerHighFrequencyMovement)
{
    const auto white = renderNoise(NoiseMode::White, 123u, 8192);
    const auto pink = renderNoise(NoiseMode::Pink, 123u, 8192);
    EXPECT_LT(averageAbsDiff(pink), averageAbsDiff(white));
}
