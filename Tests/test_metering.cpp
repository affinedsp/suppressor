#include <doctest/doctest.h>
#include "Metering.h"
#include <limits>
#include <thread>

using suppressor::Metering;
using M = suppressor::MeterSnapshot;

TEST_CASE ("meter snapshots start empty and reset on release")
{
    Metering meter;
    M s;
    REQUIRE (meter.read (s));
    CHECK (s.flags == 0);
    meter.push (0.5f, 0.25f, 20.0f, 480, 48000, 0);
    REQUIRE (meter.read (s));
    CHECK (s.input == 0.5f);
    CHECK (s.output == 0.25f);
    CHECK (s.reduction == 20.0f);
    CHECK ((s.flags & M::active) != 0);
    const auto sequence = s.sequence;
    meter.push (1, 1, 1, 0, 48000, 0);
    meter.push (1, 1, 1, 480, 0, 0);
    REQUIRE (meter.read (s));
    CHECK (s.sequence == sequence);
    meter.reset();
    REQUIRE (meter.read (s));
    CHECK (s.flags == 0);
    CHECK (s.input == 0);
    CHECK (s.reduction == 0);
}

TEST_CASE ("peak decay is time-based and clips are held for one second")
{
    for (int block : { 48, 480, 4800 })
    {
        Metering meter;
        M s;
        meter.push (1.1f, 0.4f, 40.0f, block, 48000, 0);
        for (int n = 0; n < 14400 / block; ++n) meter.push (0, 0, 0, block, 48000, 0);
        REQUIRE (meter.read (s));
        CHECK (s.input == doctest::Approx (1.1f * std::exp (-1.0f)).epsilon (0.0001));
        CHECK (s.reduction == doctest::Approx (40.0f * std::exp (-2.5f)).epsilon (0.0001));
        CHECK ((s.flags & M::inputClip) != 0);
        CHECK ((s.flags & M::outputClip) == 0);
        for (int n = 0; n < 48000 / block; ++n) meter.push (0, 0, 0, block, 48000, 0);
        REQUIRE (meter.read (s));
        CHECK ((s.flags & M::inputClip) == 0);
    }
}

TEST_CASE ("bypass and learning never claim suppression, nonfinite input cannot poison meters")
{
    Metering meter;
    M s;
    meter.push (0.5f, 0.1f, 40, 480, 48000, 0);
    for (auto flags : { M::bypassed, M::learning })
    {
        meter.push (0.5f, 0.5f, 40, 480, 48000, flags);
        REQUIRE (meter.read (s));
        CHECK (s.reduction == 0);
    }
    meter.push (std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), -2, 480, 48000, 0);
    REQUIRE (meter.read (s));
    CHECK (std::isfinite (s.input));
    CHECK (std::isfinite (s.output));
    CHECK (s.reduction >= 0);
}

TEST_CASE ("concurrent meter reads are coherent and never block the writer")
{
    Metering meter;
    std::atomic<bool> done { false };
    std::thread writer ([&]
    {
        for (int n = 1; n <= 100000; ++n)
            meter.push (static_cast<float> (n), static_cast<float> (n * 2), 0, 48, 48000, 0);
        done.store (true);
    });
    bool coherent = true;
    do
    {
        M s;
        if (meter.read (s)) coherent = coherent && s.output == s.input * 2 && (s.sequence & 1u) == 0;
    } while (! done.load());
    writer.join();
    CHECK (coherent);
}
