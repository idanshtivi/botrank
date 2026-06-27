#include <gtest/gtest.h>
#include "../Include/ContourGenerator.h"
#include <cmath>

using namespace SynthCore;

TEST(ContourGeneratorTest, InitialStateIsIdle)
{
    ContourGenerator env;
    EXPECT_FALSE(env.isActive());
    EXPECT_DOUBLE_EQ(env.getCurrentValue(), 0.0);
    EXPECT_DOUBLE_EQ(env.processSample(), 0.0);
}

TEST(ContourGeneratorTest, AttackDecaySustainRelease)
{
    ContourGenerator env;
    env.setSampleRate(10000.0);
    env.setAttackSeconds(0.01);
    env.setDecaySeconds(0.02);
    env.setSustainLevel(0.5);
    env.setReleaseSeconds(0.02);
    env.gateOn();

    for (int i = 0; i < 100; ++i) env.processSample();
    EXPECT_NEAR(env.getCurrentValue(), 1.0, 0.001);

    for (int i = 0; i < 200; ++i) env.processSample();
    EXPECT_NEAR(env.getCurrentValue(), 0.5, 0.001);

    for (int i = 0; i < 1000; ++i) env.processSample();
    EXPECT_NEAR(env.getCurrentValue(), 0.5, 0.001);

    env.gateOff();
    for (int i = 0; i < 200; ++i) env.processSample();
    EXPECT_NEAR(env.getCurrentValue(), 0.0, 0.001);
    EXPECT_FALSE(env.isActive());
}

TEST(ContourGeneratorTest, ValuesClampAndRemainFinite)
{
    ContourGenerator env;
    env.setSampleRate(48000.0);
    env.setAttackSeconds(-1.0);
    env.setDecaySeconds(100.0);
    env.setReleaseSeconds(0.0);
    env.setSustainLevel(2.0);
    env.gateOn();

    for (int i = 0; i < 48000; ++i) {
        const double v = env.processSample();
        EXPECT_TRUE(std::isfinite(v));
        EXPECT_GE(v, 0.0);
        EXPECT_LE(v, 1.0);
    }
}

TEST(ContourGeneratorTest, DecayActsAsRelease)
{
    ContourGenerator env;
    env.setSampleRate(1000.0);
    env.setAttackSeconds(0.001);
    env.setDecaySeconds(0.01);
    env.setReleaseSeconds(1.0);
    env.setDecayActsAsRelease(true);
    env.gateOn();
    env.processSample();
    env.gateOff();
    for (int i = 0; i < 10; ++i) env.processSample();
    EXPECT_NEAR(env.getCurrentValue(), 0.0, 0.001);
}

TEST(ContourGeneratorTest, RetriggerIsDeterministic)
{
    ContourGenerator a;
    ContourGenerator b;
    a.setSampleRate(1000.0);
    b.setSampleRate(1000.0);
    a.gateOn();
    b.gateOn();
    for (int i = 0; i < 5; ++i) {
        a.processSample();
        b.processSample();
    }
    a.trigger();
    b.trigger();
    for (int i = 0; i < 20; ++i) {
        EXPECT_DOUBLE_EQ(a.processSample(), b.processSample());
    }
}

TEST(ContourGeneratorTest, ResetClearsState)
{
    ContourGenerator env;
    env.gateOn();
    env.processSample();
    env.reset();
    EXPECT_FALSE(env.isActive());
    EXPECT_DOUBLE_EQ(env.getCurrentValue(), 0.0);
}

TEST(ContourGeneratorTest, IndependentInstancesDoNotInteract)
{
    ContourGenerator a;
    ContourGenerator b;
    a.setSampleRate(1000.0);
    b.setSampleRate(1000.0);
    a.setAttackSeconds(0.001);
    b.setAttackSeconds(1.0);
    a.gateOn();
    b.gateOn();
    a.processSample();
    b.processSample();
    EXPECT_GT(a.getCurrentValue(), b.getCurrentValue());
}

TEST(ContourGeneratorTest, DefaultAttackAndReleaseAreRampedAndBounded)
{
    ContourGenerator env;
    env.setSampleRate(48000.0);
    env.setAttackSeconds(0.005);
    env.setDecaySeconds(0.25);
    env.setSustainLevel(0.75);
    env.setReleaseSeconds(0.20);

    env.gateOn();
    const double first = env.processSample();
    EXPECT_GT(first, 0.0);
    EXPECT_LT(first, 0.01);

    for (int i = 0; i < 1000; ++i) {
        const double v = env.processSample();
        EXPECT_TRUE(std::isfinite(v));
        EXPECT_GE(v, 0.0);
        EXPECT_LE(v, 1.0);
    }

    const double beforeRelease = env.getCurrentValue();
    env.gateOff();
    const double releaseStart = env.processSample();
    EXPECT_LT(releaseStart, beforeRelease);
    EXPECT_GT(releaseStart, 0.0);

    for (int i = 0; i < 12000; ++i) {
        const double v = env.processSample();
        EXPECT_TRUE(std::isfinite(v));
        EXPECT_GE(v, 0.0);
        EXPECT_LE(v, 1.0);
    }
    EXPECT_NEAR(env.getCurrentValue(), 0.0, 0.001);
}

TEST(ContourGeneratorTest, RapidRetriggerRemainsFiniteAndBounded)
{
    ContourGenerator env;
    env.setSampleRate(48000.0);
    env.setAttackSeconds(0.005);
    env.setReleaseSeconds(0.20);

    for (int n = 0; n < 64; ++n) {
        env.gateOn();
        for (int i = 0; i < 17; ++i) {
            const double v = env.processSample();
            EXPECT_TRUE(std::isfinite(v));
            EXPECT_GE(v, 0.0);
            EXPECT_LE(v, 1.0);
        }
        env.gateOff();
        for (int i = 0; i < 11; ++i) {
            const double v = env.processSample();
            EXPECT_TRUE(std::isfinite(v));
            EXPECT_GE(v, 0.0);
            EXPECT_LE(v, 1.0);
        }
    }
}
