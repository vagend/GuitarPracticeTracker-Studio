#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

// TinySoundFont (MIT, Source/vendor/TinySoundFont): plays the free SoundFont the tab uses.
// Its code is compiled here, once; its own warnings are not Studio's.
#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
#elif defined (_MSC_VER)
 #pragma warning (push, 0)
#endif
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "vendor/TinySoundFont/tsf.h"
#if defined (__clang__)
 #pragma clang diagnostic pop
#elif defined (__GNUC__)
 #pragma GCC diagnostic pop
#elif defined (_MSC_VER)
 #pragma warning (pop)
#endif

// Plays the tab's notes in the audio engine, on the metronome's clock, out the same output.
//
// Like the metronome, the page decides what plays when: at the start of each bar it sends that bar's notes
// with their times on the engine's clock (so the tab follows the tempo you set, the tab's tempo changes, the
// tempo ladder and count-ins). The engine starts and stops each note on its exact sample.
class TabSynth
{
public:
    enum Type { noteOn, noteOff, program };

    TabSynth() { interleaved.resize ((size_t) maxChunk * 2); }

    ~TabSynth()
    {
        if (auto* f = sf.exchange (nullptr))
            tsf_close (f);
    }

    //==============================================================================
    // Message thread.

    // The SoundFont (web/vendor/alphatab/soundfont/sonivox.sf2). Everything that could allocate memory
    // happens here, so playing never does.
    bool load (const juce::File& file)
    {
        juce::MemoryBlock data;
        if (sf.load() != nullptr || ! file.loadFileAsData (data))
            return false;
        tsf* f = tsf_load_memory (data.getData(), (int) data.getSize());
        if (f == nullptr)
            return false;
        tsf_set_max_voices (f, 128);   // above this, the oldest note stops instead of allocating more
        for (int ch = 0; ch < 16; ++ch)
            tsf_channel_set_presetnumber (f, ch, 0, ch == 9);
        tsf_set_output (f, TSF_STEREO_INTERLEAVED, 48000, 0.0f);
        sf.store (f);
        return true;
    }

    bool isLoaded() const noexcept { return sf.load() != nullptr; }

    // A note on (vel 0..1), note off, or the instrument for a channel (key = General MIDI program),
    // at `time` on the engine's clock.
    void schedule (double time, int type, int channel, int key, float vel)
    {
        if (! juce::isPositiveAndBelow (type, 3) || ! juce::isPositiveAndBelow (channel, 16) || ! juce::isPositiveAndBelow (key, 128))
            return;
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
            events[(size_t) scope.startIndex1] = { time, (juce::uint8) type, (juce::uint8) channel, (juce::uint8) key,
                                                   juce::jlimit (0.0f, 1.0f, vel), generation.load() };
    }

    // Stopped or restarted: forget notes that haven't played, and let the ringing ones fade out.
    void clearScheduled() noexcept { ++generation; }

    void setVolume (float v) noexcept { volume.store (juce::jlimit (0.0f, 1.5f, v)); }

    //==============================================================================
    // Audio thread: adds this block's tab notes to the outputs. t0 is the clock at the block's start.
    void render (float* const* outputs, int numOutputs, int numSamples, double sampleRate, double t0) noexcept
    {
        const int gen = generation.load();
        {
            const auto scope = fifo.read (fifo.getNumReady());
            scope.forEach ([&] (int i)
            {
                if (events[(size_t) i].generation == gen && numPending < (int) pending.size())
                    pending[(size_t) numPending++] = events[(size_t) i];
            });
        }

        tsf* f = sf.load();
        if (f == nullptr)
        {
            numPending = 0;
            return;
        }
        if (gen != playedGeneration)
        {
            playedGeneration = gen;
            tsf_note_off_all (f);
            for (int i = 0; i < numPending;)
                if (pending[(size_t) i].generation != gen) pending[(size_t) i] = pending[(size_t) --numPending]; else ++i;
        }
        if (std::abs (sampleRate - outputRate) > 0.5)
        {
            outputRate = sampleRate;
            tsf_set_output (f, TSF_STEREO_INTERLEAVED, (int) sampleRate, 0.0f);
        }

        // this block's events, in order
        const double t1 = t0 + numSamples / sampleRate;
        int due[256];
        int numDue = 0;
        for (int i = 0; i < numPending && numDue < 256; ++i)
            if (pending[(size_t) i].time < t1)
                due[numDue++] = i;
        std::sort (due, due + numDue, [this] (int a, int b) { return pending[(size_t) a].time < pending[(size_t) b].time; });

        const float gain = volume.load();
        int pos = 0;
        for (int d = 0; d <= numDue; ++d)
        {
            const Event* e = d < numDue ? &pending[(size_t) due[d]] : nullptr;
            const int at = e == nullptr ? numSamples
                                        : juce::jlimit (pos, numSamples, (int) ((e->time - t0) * sampleRate));
            renderInto (f, outputs, numOutputs, pos, at - pos, gain);
            pos = at;
            if (e == nullptr)
                break;
            if (e->type == noteOn)
            {
                if (e->time > t0 - 0.25)   // much later than that (audio was stopped): skip it
                    tsf_channel_note_on (f, e->channel, e->key, e->vel);
            }
            else if (e->type == noteOff)
                tsf_channel_note_off (f, e->channel, e->key);
            else
                tsf_channel_set_presetnumber (f, e->channel, e->key, e->channel == 9);
        }

        // drop the events just played (indexes in `due` are into the pending list; remove from the end)
        std::sort (due, due + numDue);
        for (int d = numDue; --d >= 0;)
            pending[(size_t) due[d]] = pending[(size_t) --numPending];
    }

private:
    struct Event
    {
        double time = 0.0;
        juce::uint8 type = 0, channel = 0, key = 0;
        float vel = 0.0f;
        int generation = 0;
    };

    void renderInto (tsf* f, float* const* outputs, int numOutputs, int start, int count, float gain) noexcept
    {
        while (count > 0)
        {
            const int n = juce::jmin (count, maxChunk);
            tsf_render_float (f, interleaved.data(), n, 0);
            for (int ch = 0; ch < numOutputs; ++ch)
            {
                if (outputs[ch] == nullptr)
                    continue;
                const float* src = interleaved.data() + (ch & 1);
                float* dst = outputs[ch] + start;
                for (int i = 0; i < n; ++i)
                    dst[i] += gain * src[2 * i];
            }
            start += n;
            count -= n;
        }
    }

    static constexpr int maxChunk = 1024;
    std::vector<float> interleaved;

    std::atomic<tsf*> sf { nullptr };
    std::atomic<float> volume { 0.8f };
    double outputRate = 0.0;   // audio thread

    static constexpr int fifoSize = 4096;
    juce::AbstractFifo fifo { fifoSize };
    std::array<Event, fifoSize> events {};
    std::array<Event, 2048> pending {};   // audio thread only
    int numPending = 0;
    std::atomic<int> generation { 0 };
    int playedGeneration = 0;

    JUCE_DECLARE_NON_COPYABLE (TabSynth)
};
