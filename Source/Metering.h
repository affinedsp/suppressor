#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace suppressor
{
struct MeterSnapshot
{
    enum Flag : uint32_t { active = 1, bypassed = 2, removed = 4, multiband = 8,
                           learning = 16, inputClip = 32, outputClip = 64 };
    float input = 0.0f, output = 0.0f, reduction = 0.0f;
    uint32_t flags = 0, sequence = 0;
};

// Single audio writer, message-thread readers. Atomic fields make even a
// rejected read data-race-free. The writer never waits for the UI.
class Metering
{
public:
    static_assert (std::atomic<float>::is_always_lock_free);
    static_assert (std::atomic<uint32_t>::is_always_lock_free);

    void reset() noexcept
    {
        in = out = reduction = inClipTime = outClipTime = 0.0f;
        previousRoute = 0;
        publish (0);
    }

    void push (float inputPeak, float outputPeak, float reductionDb,
               int samples, double sampleRate, uint32_t flags) noexcept
    {
        if (samples <= 0 || sampleRate <= 0.0) return;
        const auto route = flags & (MeterSnapshot::bypassed | MeterSnapshot::removed
                                   | MeterSnapshot::multiband | MeterSnapshot::learning);
        if (route != previousRoute)
            in = out = reduction = inClipTime = outClipTime = 0.0f;
        previousRoute = route;
        const auto seconds = static_cast<float> (samples / sampleRate);
        const auto decay = std::exp (-seconds / 0.3f);
        in = std::max (clean (inputPeak), in * decay);
        out = std::max (clean (outputPeak), out * decay);
        // Cap the display envelope, not just the drawing. A zero-gain startup
        // sample must not leave a >100 dB peak hanging above the real depth.
        reduction = (flags & (MeterSnapshot::bypassed | MeterSnapshot::learning)) != 0
                  ? 0.0f : std::max (std::clamp (clean (reductionDb), 0.0f, 60.0f),
                                    reduction * std::exp (-seconds / 0.12f));
        inClipTime = inputPeak >= 1.0f ? 1.0f : std::max (0.0f, inClipTime - seconds);
        outClipTime = outputPeak >= 1.0f ? 1.0f : std::max (0.0f, outClipTime - seconds);
        if (inClipTime > 0.0f) flags |= MeterSnapshot::inputClip;
        if (outClipTime > 0.0f) flags |= MeterSnapshot::outputClip;
        publish (flags | MeterSnapshot::active);
    }

    bool read (MeterSnapshot& result) const noexcept
    {
        // seq_cst across the atomic fields gives a simple portable coherent
        // snapshot, not a seqlock around non-atomic (racy) storage.
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto before = sequence.load();
            if ((before & 1u) != 0) continue;
            MeterSnapshot s { input.load(), output.load(), gr.load(), status.load(), before };
            if (before == sequence.load()) { result = s; return true; }
        }
        return false;
    }

private:
    static float clean (float v) noexcept { return std::isfinite (v) ? std::max (0.0f, v) : 0.0f; }
    void publish (uint32_t flags) noexcept
    {
        sequence.fetch_add (1);
        input.store (in); output.store (out); gr.store (reduction); status.store (flags);
        sequence.fetch_add (1);
    }

    float in = 0.0f, out = 0.0f, reduction = 0.0f, inClipTime = 0.0f, outClipTime = 0.0f;
    uint32_t previousRoute = 0;
    std::atomic<float> input { 0 }, output { 0 }, gr { 0 };
    std::atomic<uint32_t> status { 0 }, sequence { 0 };
};
} // namespace suppressor
