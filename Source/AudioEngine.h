#pragma once

#include <array>
#include <juce_audio_utils/juce_audio_utils.h>

// Plays the guitar input through one hosted plugin (amp sim or effect).
// The guitar is the first active input channel chosen in Audio settings.
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine (juce::PropertiesFile& settingsFile)
        : settings (settingsFile)
    {
        juce::addDefaultFormatsToManager (formatManager);

        if (auto xml = settings.getXmlValue ("pluginList"))
            knownPlugins.recreateFromXml (*xml);

        toneOn = settings.getBoolValue ("toneOn", false);
        inputOn = settings.getBoolValue ("inputOn", true);

        auto deviceXml = settings.getXmlValue ("audioDevice");
        deviceManager.initialise (2, 2, deviceXml.get(), true);
        deviceManager.addAudioCallback (this);
    }

    ~AudioEngine() override
    {
        deviceManager.removeAudioCallback (this);
        const juce::ScopedLock sl (lock);
        plugin.reset();
    }

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    //==============================================================================
    // Message thread only.

    void restoreSavedPlugin (std::function<void (const juce::String&)> done)
    {
        auto desc = settings.getXmlValue ("pluginDescription");
        if (desc == nullptr)
            return;

        juce::PluginDescription pd;
        if (! pd.loadFromXml (*desc))
            return;

        juce::MemoryBlock state;
        state.fromBase64Encoding (settings.getValue ("pluginState"));
        loadPlugin (pd, std::move (state), std::move (done));
    }

    void loadPlugin (const juce::PluginDescription& pd,
                     juce::MemoryBlock stateToApply,
                     std::function<void (const juce::String&)> done)
    {
        formatManager.createPluginInstanceAsync (pd, sampleRate, blockSize,
            [this, pd, stateToApply, done] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                            const juce::String& error)
            {
                if (instance == nullptr)
                {
                    if (done) done (error.isNotEmpty() ? error : juce::String ("The plugin could not be loaded."));
                    return;
                }

                setUpChannels (*instance);

                if (stateToApply.getSize() > 0)
                    instance->setStateInformation (stateToApply.getData(), (int) stateToApply.getSize());

                instance->setRateAndBufferSizeDetails (sampleRate, blockSize);
                instance->prepareToPlay (sampleRate, blockSize);

                std::unique_ptr<juce::AudioPluginInstance> old;
                {
                    const juce::ScopedLock sl (lock);
                    old = std::move (plugin);
                    plugin = std::move (instance);
                }
                old.reset(); // released here, outside the audio lock

                if (auto xml = pd.createXml())
                    settings.setValue ("pluginDescription", xml.get());

                saveState();
                if (done) done ({});
            });
    }

    void removePlugin()
    {
        std::unique_ptr<juce::AudioPluginInstance> old;
        {
            const juce::ScopedLock sl (lock);
            old = std::move (plugin);
        }
        old.reset();
        settings.removeValue ("pluginDescription");
        settings.removeValue ("pluginState");
        settings.saveIfNeeded();
    }

    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    juce::String getPluginId() const
    {
        return plugin != nullptr ? plugin->getPluginDescription().createIdentifierString() : juce::String();
    }

    juce::MemoryBlock getPluginState() const
    {
        juce::MemoryBlock mb;
        if (plugin != nullptr)
            plugin->getStateInformation (mb);
        return mb;
    }

    void setPluginState (const juce::MemoryBlock& mb)
    {
        if (plugin != nullptr && mb.getSize() > 0)
            plugin->setStateInformation (mb.getData(), (int) mb.getSize());
    }

    juce::String getPluginName() const
    {
        return plugin != nullptr ? plugin->getName() : juce::String();
    }

    void setToneOn (bool on)
    {
        toneOn = on;
        settings.setValue ("toneOn", on);
        settings.saveIfNeeded();
    }

    bool isToneOn() const noexcept { return toneOn; }

    // Input mute: Studio ignores the audio interface's input completely (no sound, no level,
    // nothing for the tuner or trainer to hear).
    void setInputOn (bool on)
    {
        inputOn = on;
        settings.setValue ("inputOn", on);
        settings.saveIfNeeded();
    }
    bool isInputOn() const noexcept { return inputOn; }

    float getInputLevel() const noexcept { return inputLevel.load(); }

    // The Fretboard Trainer listens to the guitar here (the same input as in Audio settings),
    // at about 12 kHz: plenty for finding the note, and small enough to send to the page.
    void setTapOn (bool on) noexcept { tapOn.store (on); }
    double getTapRate() const noexcept { return sampleRate / tapDecim(); }

    // Copies the newest n samples (n <= tapSize) into out.
    void readTap (float* out, int n) const noexcept
    {
        const int pos = tapPos.load();
        for (int i = 0; i < n; ++i)
            out[i] = tap[(size_t) ((pos - n + i) & (tapSize - 1))];
    }

    static constexpr int tapSize = 4096;

    // Saves the audio device, plugin list and the plugin's current settings.
    void saveState()
    {
        if (auto xml = deviceManager.createStateXml())
            settings.setValue ("audioDevice", xml.get());

        if (auto xml = knownPlugins.createXml())
            settings.setValue ("pluginList", xml.get());

        if (plugin != nullptr)
        {
            juce::MemoryBlock mb;
            plugin->getStateInformation (mb);
            settings.setValue ("pluginState", mb.toBase64Encoding());
        }

        settings.saveIfNeeded();
    }

    //==============================================================================
    // Audio thread.

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        blockSize  = device->getCurrentBufferSizeSamples();

        const juce::ScopedLock sl (lock);
        if (work.getNumSamples() < blockSize)
            work.setSize (juce::jmax (2, work.getNumChannels()), blockSize, false, false, true);

        if (plugin != nullptr)
        {
            plugin->releaseResources();
            plugin->setRateAndBufferSizeDetails (sampleRate, blockSize);
            plugin->prepareToPlay (sampleRate, blockSize);
        }
    }

    void audioDeviceStopped() override
    {
        const juce::ScopedLock sl (lock);
        if (plugin != nullptr)
            plugin->releaseResources();
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        const float* guitar = (numInputs > 0 && inputOn.load()) ? inputs[0] : nullptr;

        float peak = 0.0f;
        if (guitar != nullptr)
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs (guitar[i]));
        inputLevel.store (juce::jmax (peak, inputLevel.load() * 0.9f));

        if (guitar != nullptr && tapOn.load())
        {
            const int d = tapDecim();
            int pos = tapPos.load();
            for (int i = 0; i < numSamples; ++i)
            {
                tapSum += guitar[i];
                if (++tapCount >= d)
                {
                    tap[(size_t) (pos & (tapSize - 1))] = tapSum / (float) d;
                    ++pos; tapSum = 0.0f; tapCount = 0;
                }
            }
            tapPos.store (pos);
        }

        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);

        if (! toneOn || guitar == nullptr)
            return; // muted

        const juce::ScopedTryLock sl (lock);
        if (! sl.isLocked())
            return;

        if (plugin == nullptr)
        {
            // Empty slot: the raw guitar straight through.
            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr)
                    juce::FloatVectorOperations::copy (outputs[ch], guitar, numSamples);
            return;
        }

        const int pluginIns  = plugin->getTotalNumInputChannels();
        const int pluginOuts = plugin->getTotalNumOutputChannels();
        const int channels   = juce::jmax (1, pluginIns, pluginOuts);

        if (work.getNumChannels() < channels || work.getNumSamples() < numSamples)
            return; // buffer not ready yet (sized on the message thread); skip one block

        juce::AudioBuffer<float> buf (work.getArrayOfWritePointers(), channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
        {
            if (ch < pluginIns)
                juce::FloatVectorOperations::copy (buf.getWritePointer (ch), guitar, numSamples);
            else
                buf.clear (ch, 0, numSamples);
        }

        midi.clear();
        plugin->processBlock (buf, midi);

        if (pluginOuts <= 0)
            return;

        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::copy (outputs[ch], buf.getReadPointer (juce::jmin (ch, pluginOuts - 1)), numSamples);
    }

private:
    void setUpChannels (juce::AudioPluginInstance& p)
    {
        p.enableAllBuses();

        // Prefer stereo in/out, then mono in/stereo out, then mono/mono.
        const juce::AudioChannelSet mono = juce::AudioChannelSet::mono();
        const juce::AudioChannelSet stereo = juce::AudioChannelSet::stereo();

        for (auto [in, out] : { std::pair { stereo, stereo }, std::pair { mono, stereo }, std::pair { mono, mono } })
        {
            auto layout = p.getBusesLayout();
            if (layout.inputBuses.size() > 0)  layout.inputBuses.getReference (0)  = in;
            if (layout.outputBuses.size() > 0) layout.outputBuses.getReference (0) = out;

            if (p.checkBusesLayoutSupported (layout) && p.setBusesLayout (layout))
                break;
        }

        // Size the work buffer generously here, so the audio thread never allocates.
        const int channels = juce::jmax (2, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
        const juce::ScopedLock sl (lock);
        work.setSize (juce::jmax (channels, work.getNumChannels()), juce::jmax (blockSize, 4096), false, false, true);
        midi.ensureSize (2048);
    }

    juce::PropertiesFile& settings;
    juce::CriticalSection lock;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> work;
    juce::MidiBuffer midi;
    double sampleRate = 48000.0;
    int blockSize = 512;
    std::atomic<bool> toneOn { false };
    std::atomic<bool> inputOn { true };
    std::atomic<float> inputLevel { 0.0f };

    int tapDecim() const noexcept { return juce::jmax (1, juce::roundToInt (sampleRate / 12000.0)); }
    std::array<float, tapSize> tap {};
    std::atomic<int> tapPos { 0 };
    std::atomic<bool> tapOn { false };
    float tapSum = 0.0f;
    int tapCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
