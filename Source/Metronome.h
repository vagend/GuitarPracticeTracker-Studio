#pragma once

#include <array>
#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>

// The metronome's sounds, played by the audio engine: out the same audio interface as the guitar,
// on the engine's own clock.
//
// The page (web/index.html) still decides what plays when: the pattern, the tempo ladder, the tab's bars.
// It works a quarter of a second ahead and sends each hit with its time on this clock (seconds of audio
// the engine has played). The engine starts each sound on exactly that sample, so the spacing between
// clicks never depends on how quickly the page or its messages run.
class Metronome
{
public:
    // Numbers the page sends for each sound.
    enum Sound { drumsP, drumsS, drumsM,      // kick, snare, hi-hat
                 clickP, clickS, clickM,      // high, middle, low click
                 readyAccent, readyBeat,      // the tempo ladder's count-in
                 userP, userS, userM,         // your own samples
                 numSounds };

    Metronome()
    {
        formats.registerBasicFormats();
        renderBuiltInSounds();
    }

    //==============================================================================
    // Message thread.

    // Seconds of audio the engine has played (its clock). Hits are scheduled on it.
    double now() const noexcept { return clock.load(); }

    // A sound at `time` on the clock. `fallback` plays instead if `sound` is one of your samples
    // and that sample isn't loaded (yet).
    void schedule (double time, int sound, int fallback, float gain)
    {
        if (! juce::isPositiveAndBelow (sound, (int) numSounds) || ! juce::isPositiveAndBelow (fallback, (int) userP))
            return;
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
            hits[(size_t) scope.startIndex1] = { time, (juce::uint8) sound, (juce::uint8) fallback,
                                                 juce::jlimit (0.0f, 2.0f, gain), generation.load() };
    }

    // Stopped or restarted: forget hits that haven't played yet (sounds already playing ring out).
    void clearScheduled() noexcept { ++generation; }

    // Your sample for one voice (0 primary, 1 secondary, 2 minor), as the file's bytes; empty removes it.
    // Returns an error ("unreadable" or "long") or an empty string.
    juce::String setUserSample (int voice, const juce::MemoryBlock& file)
    {
        if (! juce::isPositiveAndBelow (voice, 3))
            return "unreadable";

        std::unique_ptr<Buffer> fresh;
        if (file.getSize() > 0)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (
                formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (file, true)));
            if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
                return "unreadable";
            if ((double) reader->lengthInSamples > reader->sampleRate * 5.5)
                return "long";   // the page allows samples up to 5 s

            fresh = std::make_unique<Buffer>();
            fresh->rate = reader->sampleRate;
            const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
            const int length = (int) reader->lengthInSamples;
            fresh->audio.setSize (channels, length + 1);   // +1: a silent sample for interpolation
            fresh->audio.clear();
            reader->read (&fresh->audio, 0, length, 0, true, channels > 1);
        }

        {
            const juce::ScopedLock sl (soundLock);
            for (auto& v : voices)
                if (v.buffer != nullptr && v.buffer == user[(size_t) voice].get())
                    v.buffer = nullptr;   // stop the old sample before it's deleted
            std::swap (user[(size_t) voice], fresh);
        }
        return {};   // the old sample is deleted here, outside the lock
    }

    //==============================================================================
    // Audio thread: adds this block's sounds to the outputs and moves the clock on.
    // With numOutputs 0 (a stand-in device) the clock still runs but nothing is heard.
    void render (float* const* outputs, int numOutputs, int numSamples, double sampleRate) noexcept
    {
        const double t0 = clock.load();
        const double t1 = t0 + numSamples / sampleRate;
        clock.store (t1);

        // New hits from the page; drop any from before the last stop.
        const int gen = generation.load();
        {
            const auto scope = fifo.read (fifo.getNumReady());
            scope.forEach ([&] (int i)
            {
                if (hits[(size_t) i].generation == gen && numPending < (int) pending.size())
                    pending[(size_t) numPending++] = hits[(size_t) i];
            });
        }
        for (int i = 0; i < numPending;)
        {
            if (pending[(size_t) i].generation != gen)
                pending[(size_t) i] = pending[(size_t) --numPending];
            else
                ++i;
        }

        const juce::ScopedTryLock sl (soundLock);
        if (! sl.isLocked())
            return;   // a sample is being swapped: start these hits next block

        for (int i = 0; i < numPending;)
        {
            const auto& h = pending[(size_t) i];
            if (h.time >= t1)
            {
                ++i;
                continue;
            }
            if (h.time > t0 - 0.05)   // much later than that (audio was stopped): skip it
                startVoice (h, juce::jlimit (0, numSamples - 1, (int) ((h.time - t0) * sampleRate)));
            pending[(size_t) i] = pending[(size_t) --numPending];
        }

        for (auto& v : voices)
            if (v.buffer != nullptr)
                mix (v, outputs, numOutputs, numSamples, sampleRate);
    }

private:
    struct Buffer
    {
        juce::AudioBuffer<float> audio;   // mono or stereo
        double rate = 48000.0;
    };

    struct Hit
    {
        double time = 0.0;
        juce::uint8 sound = 0, fallback = 0;
        float gain = 0.0f;
        int generation = 0;
    };

    struct Voice
    {
        const Buffer* buffer = nullptr;
        double pos = 0.0;
        float gain = 0.0f;
        int delay = 0;   // samples into this block before it starts
    };

    void startVoice (const Hit& h, int offset) noexcept
    {
        const Buffer* b = h.sound >= userP ? user[(size_t) (h.sound - userP)].get() : nullptr;
        if (b == nullptr)
            b = &builtIn[(size_t) (h.sound >= userP ? h.fallback : h.sound)];

        // a free voice, or the one that has played longest
        Voice* best = &voices[0];
        for (auto& v : voices)
        {
            if (v.buffer == nullptr) { best = &v; break; }
            if (v.pos > best->pos) best = &v;
        }
        *best = { b, 0.0, h.gain, offset };
    }

    static void mix (Voice& v, float* const* outputs, int numOutputs, int numSamples, double sampleRate) noexcept
    {
        const auto& a = v.buffer->audio;
        const int last = a.getNumSamples() - 1;
        const int channels = a.getNumChannels();
        const double step = v.buffer->rate / sampleRate;

        for (int i = v.delay; i < numSamples; ++i)
        {
            const int k = (int) v.pos;
            if (k >= last)
            {
                v.buffer = nullptr;
                return;
            }
            const float frac = (float) (v.pos - k);
            for (int ch = 0; ch < numOutputs; ++ch)
            {
                if (outputs[ch] == nullptr)
                    continue;
                const float* s = a.getReadPointer (juce::jmin (ch, channels - 1));
                outputs[ch][i] += v.gain * (s[k] + frac * (s[k + 1] - s[k]));
            }
            v.pos += step;
        }
        v.delay = 0;
    }

    //==============================================================================
    // The built-in sounds, made the same way the page's Web Audio version makes them.

    static constexpr double rate = 48000.0;

    // Web Audio's envelope: from silence up to `peak` in 3 ms, then an exponential fade over `dur`.
    static float envelope (double t, double peak, double dur) noexcept
    {
        constexpr double attack = 0.003, floor = 0.0001;
        if (t < 0.0 || t >= dur) return 0.0f;
        if (t < attack) return (float) (floor * std::pow (peak / floor, t / attack));
        return (float) (peak * std::pow (floor / peak, (t - attack) / (dur - attack)));
    }

    static juce::AudioBuffer<float> blank (double seconds)
    {
        juce::AudioBuffer<float> b (1, (int) (seconds * rate) + 1);
        b.clear();
        return b;
    }

    static void addSine (juce::AudioBuffer<float>& b, double freq, double peak, double dur)
    {
        auto* d = b.getWritePointer (0);
        for (int i = 0; i < b.getNumSamples() - 1; ++i)
        {
            const double t = i / rate;
            d[i] += (float) std::sin (juce::MathConstants<double>::twoPi * freq * t) * envelope (t, peak, dur);
        }
    }

    // White noise through a high-pass filter (hi-hat and snare).
    static void addNoise (juce::AudioBuffer<float>& b, double cutoff, double peak, double dur)
    {
        juce::Random random (12345);
        std::vector<float> n ((size_t) b.getNumSamples() - 1);
        for (auto& x : n)
            x = random.nextFloat() * 2.0f - 1.0f;
        juce::IIRFilter hp;
        hp.setCoefficients (juce::IIRCoefficients::makeHighPass (rate, cutoff, 1.122));   // Web Audio's Q of 1 dB
        hp.processSamples (n.data(), (int) n.size());
        auto* d = b.getWritePointer (0);
        for (size_t i = 0; i < n.size(); ++i)
            d[i] += n[i] * envelope ((double) i / rate, peak, dur);
    }

    void renderBuiltInSounds()
    {
        const auto set = [this] (Sound s, juce::AudioBuffer<float> b) { builtIn[(size_t) s].audio = std::move (b); };

        {   // kick: a sine falling from 150 Hz to 45 Hz
            auto b = blank (0.4);
            auto* d = b.getWritePointer (0);
            double phase = 0.0;
            for (int i = 0; i < b.getNumSamples() - 1; ++i)
            {
                const double t = i / rate;
                const double f = t < 0.12 ? 150.0 * std::pow (45.0 / 150.0, t / 0.12) : 45.0;
                d[i] = (float) std::sin (phase) * envelope (t, 1.0, 0.35);
                phase += juce::MathConstants<double>::twoPi * f / rate;
            }
            set (drumsP, std::move (b));
        }
        {   // snare: noise above 1.2 kHz and a 190 Hz triangle
            auto b = blank (0.2);
            addNoise (b, 1200.0, 0.7, 0.18);
            auto* d = b.getWritePointer (0);
            for (int i = 0; i < b.getNumSamples() - 1; ++i)
            {
                const double t = i / rate, f = std::fmod (190.0 * t, 1.0);
                d[i] += (float) (4.0 * std::abs (f - 0.5) - 1.0) * envelope (t, 0.4, 0.1);
            }
            set (drumsS, std::move (b));
        }
        {   // hi-hat: noise above 7 kHz
            auto b = blank (0.06);
            addNoise (b, 7000.0, 0.35, 0.045);
            set (drumsM, std::move (b));
        }

        const auto click = [] (double freq, double peak, double dur)
        {
            auto b = blank (0.08);
            addSine (b, freq, peak, dur);
            return b;
        };
        set (clickP, click (1600.0, 0.6, 0.05));
        set (clickS, click (1100.0, 0.6, 0.05));
        set (clickM, click (800.0, 0.35, 0.03));
        set (readyAccent, click (2000.0, 1.0, 0.035));
        set (readyBeat, click (1500.0, 1.0, 0.035));

        for (auto& b : builtIn)
            b.rate = rate;
    }

    //==============================================================================
    juce::AudioFormatManager formats;
    std::array<Buffer, userP> builtIn;
    std::array<std::unique_ptr<Buffer>, 3> user;
    juce::CriticalSection soundLock;   // guards `user` and the voices playing it

    static constexpr int fifoSize = 1024;
    juce::AbstractFifo fifo { fifoSize };
    std::array<Hit, fifoSize> hits {};
    std::array<Hit, 512> pending {};   // audio thread only
    int numPending = 0;
    std::array<Voice, 64> voices {};   // audio thread only (and setUserSample, under the lock)

    std::atomic<double> clock { 0.0 };
    std::atomic<int> generation { 0 };

    JUCE_DECLARE_NON_COPYABLE (Metronome)
};
