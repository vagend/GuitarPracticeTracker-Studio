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

        wantedDevice = settings.getXmlValue ("audioDevice");
        // Only fall back to the computer's default device if you never picked one.
        deviceManager.initialise (2, 2, wantedDevice.get(), wantedDevice == nullptr);
        deviceManager.addAudioCallback (this);
        checkDevice();
    }

    //==============================================================================
    // The audio interface you chose. If it can't be opened right away (for example the driver is
    // still held by the copy of Studio that just closed for an update), Studio keeps trying for a
    // while and never saves the stand-in device over your choice.
    bool isWantedDeviceOpen() const noexcept { return deviceOk; }

    // Called when devices come and go, and now and then while your interface is missing.
    // Studio never runs on a stand-in device: the computer's own microphone and speakers
    // through an amp plugin can howl. So it reopens your interface when it's there again,
    // and otherwise leaves audio off.
    void followWantedDevice (bool devicesChanged)
    {
        if (wantedDevice == nullptr)
            return;
        checkDevice();
        if (deviceOk)
            return;
        if (devicesChanged)
            reopenAttempts = 0;
        const auto now = juce::Time::getMillisecondCounter();
        if (wantedDeviceListed() && reopenAttempts < 20 && now - lastReopen > 1500)
        {
            ++reopenAttempts;
            lastReopen = now;
            deviceManager.initialise (2, 2, wantedDevice.get(), false);
            checkDevice();
            if (deviceOk)
                return;
        }
        if (deviceManager.getCurrentAudioDevice() != nullptr)
            deviceManager.closeAudioDevice();
    }

    bool reopenGaveUp() const noexcept { return reopenAttempts >= 20; }

    // You picked a device in Audio settings: that's the new choice.
    void deviceChosen()
    {
        wantedDevice = deviceManager.createStateXml();
        deviceOk = true;
        standIn.store (false);
        saveState();
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
        if (deviceOk)
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
        standIn.store (! isWantedDevice (device));

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
        if (standIn.load())
        {
            // Not your interface (the computer's own mic and speakers): stay silent.
            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr)
                    juce::FloatVectorOperations::clear (outputs[ch], numSamples);
            inputLevel.store (0.0f);
            return;
        }

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

    std::unique_ptr<juce::XmlElement> wantedDevice;
    bool deviceOk = true;
    int reopenAttempts = 0;
    std::atomic<bool> standIn { false };

    // Is this the interface you picked? (Always yes if you never picked one.)
    bool isWantedDevice (juce::AudioIODevice* dev) const
    {
        if (wantedDevice == nullptr || dev == nullptr)
            return true;
        const auto type = wantedDevice->getStringAttribute ("deviceType");
        const auto out  = wantedDevice->getStringAttribute ("audioOutputDeviceName");
        const auto in   = wantedDevice->getStringAttribute ("audioInputDeviceName");
        if (type.isNotEmpty() && dev->getTypeName() != type)
            return false;
        // Separate input and output devices open as one device named after both.
        const auto name = dev->getName();
        return (out.isEmpty() && in.isEmpty())
               || (out.isNotEmpty() && name.contains (out))
               || (in.isNotEmpty() && name.contains (in));
    }
    juce::uint32 lastReopen = 0;

    // Is your interface in the computer's device list right now?
    bool wantedDeviceListed()
    {
        const auto type = wantedDevice->getStringAttribute ("deviceType");
        const auto out  = wantedDevice->getStringAttribute ("audioOutputDeviceName");
        const auto in   = wantedDevice->getStringAttribute ("audioInputDeviceName");
        for (auto* t : deviceManager.getAvailableDeviceTypes())
            if (type.isEmpty() || t->getTypeName() == type)
                if ((out.isEmpty() || t->getDeviceNames (false).contains (out))
                    && (in.isEmpty() || t->getDeviceNames (true).contains (in)))
                    return true;
        return false;
    }

    void checkDevice()
    {
        if (wantedDevice == nullptr) { deviceOk = true; return; }
        auto* dev = deviceManager.getCurrentAudioDevice();
        const auto type = wantedDevice->getStringAttribute ("deviceType");
        const auto out  = wantedDevice->getStringAttribute ("audioOutputDeviceName");
        const auto in   = wantedDevice->getStringAttribute ("audioInputDeviceName");
        deviceOk = dev != nullptr
                   && (type.isEmpty() || dev->getTypeName() == type)
                   && (out.isEmpty() || dev->getName() == out || deviceManager.getAudioDeviceSetup().outputDeviceName == out)
                   && (in.isEmpty()  || deviceManager.getAudioDeviceSetup().inputDeviceName == in);
    }
    std::atomic<float> inputLevel { 0.0f };

    int tapDecim() const noexcept { return juce::jmax (1, juce::roundToInt (sampleRate / 12000.0)); }
    std::array<float, tapSize> tap {};
    std::atomic<int> tapPos { 0 };
    std::atomic<bool> tapOn { false };
    float tapSum = 0.0f;
    int tapCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
