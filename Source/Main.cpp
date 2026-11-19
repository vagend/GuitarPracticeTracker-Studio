#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "AudioEngine.h"

#if JUCE_MAC
 #include <unistd.h>   // getpid, for the updater
#endif

#ifndef STUDIO_BUILD
 #define STUDIO_BUILD 0   // set by GitHub Actions; 0 means built on this computer
#endif

namespace
{
// Updates come from the newest GitHub release, built by .github/workflows/release.yml.
const juce::String releaseFiles ("https://github.com/vagend/GuitarPracticeTracker-Studio/releases/latest/download/");

#if JUCE_MAC
const juce::String updateAsset ("GuitarPracticeStudio-mac.zip");
#else
const juce::String updateAsset ("GuitarPracticeStudio-setup.exe");
#endif

std::unique_ptr<juce::InputStream> openDownload (const juce::String& name, int timeoutMs)
{
    return juce::URL (releaseFiles + name)
        .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                .withConnectionTimeoutMs (timeoutMs)
                                .withNumRedirectsToFollow (10));
}

// The build number of the newest release, or 0 if it can't be read.
int latestReleaseBuild()
{
    if (auto in = openDownload ("BUILD.txt", 10000))
        return in->readEntireStreamAsString().trim().getIntValue();
    return 0;
}

// Downloads the update and starts installing it. Returns an error message, or empty on success
// (the app should then quit so the new version can replace it).
juce::String downloadAndStartUpdate()
{
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("GuitarPracticeStudioUpdate");
    temp.deleteRecursively();
    temp.createDirectory();
    const auto file = temp.getChildFile (updateAsset);

    {
        auto in = openDownload (updateAsset, 20000);
        if (in == nullptr)
            return "Couldn't download the update.";
        juce::FileOutputStream out (file);
        if (! out.openedOk() || out.writeFromInputStream (*in, -1) <= 0)
            return "Couldn't save the update.";
    }

   #if JUCE_MAC
    // Unpack, then a small script waits for Studio to quit, swaps in the new app and opens it.
    const auto unpacked = temp.getChildFile ("new");
    juce::ChildProcess unzip;
    if (! unzip.start (juce::StringArray { "/usr/bin/ditto", "-x", "-k", file.getFullPathName(), unpacked.getFullPathName() })
        || ! unzip.waitForProcessToFinish (120000) || unzip.getExitCode() != 0)
        return "Couldn't unpack the update.";

    const auto newApp = unpacked.getChildFile ("Guitar Practice Studio.app");
    const auto oldApp = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    if (! newApp.isDirectory())
        return "The update didn't contain the app.";

    const auto q = [] (const juce::File& f) { return f.getFullPathName().quoted ('\'') ; };
    const auto script = temp.getChildFile ("install.sh");
    script.replaceWithText ("#!/bin/bash\n"
                            "while kill -0 " + juce::String ((int) getpid()) + " 2>/dev/null; do sleep 0.5; done\n"
                            "rm -rf " + q (oldApp) + " && /usr/bin/ditto " + q (newApp) + " " + q (oldApp) + "\n"
                            "/usr/bin/xattr -dr com.apple.quarantine " + q (oldApp) + " 2>/dev/null\n"
                            "open " + q (oldApp) + "\n");
    static juce::ChildProcess installer;   // kept alive; it finishes after Studio quits
    if (! installer.start (juce::StringArray { "/bin/bash", script.getFullPathName() }, 0))
        return "Couldn't start the update.";
   #else
    // The installer closes Studio, installs over it and opens it again.
    if (! file.startAsProcess ("/SILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS"))
        return "Couldn't start the installer.";
   #endif
    return {};
}

juce::File getWebFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
               .getChildFile ("Contents/Resources/web");
   #else
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
               .getSiblingFile ("web");
   #endif
}

juce::String mimeTypeFor (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".html")        return "text/html";
    if (ext == ".js")          return "text/javascript";
    if (ext == ".css")         return "text/css";
    if (ext == ".json")        return "application/json";
    if (ext == ".webmanifest") return "application/manifest+json";
    if (ext == ".svg")         return "image/svg+xml";
    if (ext == ".png")         return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".ico")         return "image/x-icon";
    if (ext == ".wav")         return "audio/wav";
    if (ext == ".mp3")         return "audio/mpeg";
    if (ext == ".woff2")       return "font/woff2";
    return "application/octet-stream";
}

// Studio is its own app: give the shared page Studio's name, version and "What's new"
// list (from studio.js). Each change is skipped if the page no longer contains its text.
juce::MemoryBlock brandAsStudio (juce::String html)
{
    html = html.replace ("<title>Guitar Practice</title>",
                         "<title>Guitar Practice Studio</title><script src=\"studio.js\"></script>")
               .replace ("<h1>Guitar Practice</h1>", "<h1>Guitar Practice Studio</h1>")
               .replace ("const CHANGES=[", "const CHANGES=window.STUDIO_CHANGES||[");

    const juce::String key ("const APP_VERSION='");
    const auto start = html.indexOf (key);
    if (start >= 0)
    {
        const auto valueStart = start + key.length();
        const auto end = html.indexOfChar (valueStart, '\'');
        if (end > valueStart)
            html = html.replaceSection (valueStart, end - valueStart, JUCE_APPLICATION_VERSION_STRING);
    }

    const auto utf8 = html.toUTF8();
    return juce::MemoryBlock (utf8.getAddress(), utf8.sizeInBytes() - 1);
}

// Serves the practice app's files (index.html, trainer.html, ...) to the web view.
std::optional<juce::WebBrowserComponent::Resource> serveWebFile (const juce::String& requestPath)
{
    auto path = requestPath.upToFirstOccurrenceOf ("?", false, false)
                           .upToFirstOccurrenceOf ("#", false, false)
                           .trimCharactersAtStart ("/");
    if (path.isEmpty())
        path = "index.html";

    const auto root = getWebFolder();
    const auto file = root.getChildFile (juce::URL::removeEscapeChars (path));

    if (! file.isAChildOf (root) || ! file.existsAsFile())
        return std::nullopt;

    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
        return std::nullopt;

    if (path == "index.html")
        mb = brandAsStudio (mb.toString());

    std::vector<std::byte> data (mb.getSize());
    std::memcpy (data.data(), mb.getData(), mb.getSize());
    return juce::WebBrowserComponent::Resource { std::move (data), mimeTypeFor (file) };
}

//==============================================================================
// A plain window that hides or deletes itself when its close button is clicked.
class ToolWindow final : public juce::DocumentWindow
{
public:
    ToolWindow (const juce::String& title, juce::Component* content, bool ownsContent,
                std::function<void()> onCloseIn)
        : DocumentWindow (title, juce::Colour (0xff1e2328), DocumentWindow::allButtons),
          onClose (std::move (onCloseIn))
    {
        setUsingNativeTitleBar (true);
        if (ownsContent) setContentOwned (content, true);
        else             setContentNonOwned (content, true);
        setResizable (true, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        toFront (true);
    }

    void closeButtonPressed() override
    {
        if (onClose) onClose();
    }

private:
    std::function<void()> onClose;
};

//==============================================================================
// Plugin list with scan options and a Load button.
class PluginChooser final : public juce::Component
{
public:
    PluginChooser (AudioEngine& e, juce::PropertiesFile& props, juce::File deadMansPedal,
                   std::function<void (const juce::PluginDescription&)> onLoadIn)
        : list (e.formatManager, e.knownPlugins, deadMansPedal, &props),
          onLoad (std::move (onLoadIn)),
          engine (e)
    {
        list.getTableListBox().setMultipleSelectionEnabled (false);
        list.setOptionsButtonText ("Scan for plugins...");
        addAndMakeVisible (list);

        hint.setText ("First time: click \"Scan for plugins...\" and scan for VST3 and AudioUnit plug-ins. "
                      "Then pick your amp sim or effect and click Load.",
                      juce::dontSendNotification);
        hint.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (hint);

        loadButton.onClick = [this]
        {
            const auto row = list.getTableListBox().getSelectedRow();
            const auto types = engine.knownPlugins.getTypes();
            if (juce::isPositiveAndBelow (row, types.size()) && onLoad)
                onLoad (types.getReference (row));
        };
        addAndMakeVisible (loadButton);

        setSize (720, 520);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto bottom = r.removeFromBottom (36);
        loadButton.setBounds (bottom.removeFromRight (140));
        bottom.removeFromRight (8);
        hint.setBounds (bottom);
        r.removeFromBottom (8);
        list.setBounds (r);
    }

private:
    juce::PluginListComponent list;
    juce::Label hint;
    juce::TextButton loadButton { "Load" };
    std::function<void (const juce::PluginDescription&)> onLoad;
    AudioEngine& engine;
};

//==============================================================================
// The web view, plus a flag for when the first page has loaded (events can't be sent before).
class StudioWebView final : public juce::WebBrowserComponent
{
public:
    using WebBrowserComponent::WebBrowserComponent;
    bool ready = false;
    void pageFinishedLoading (const juce::String&) override { ready = true; }
};

//==============================================================================
// The whole window is the practice app. Studio's controls live in the app's Settings tab
// (added by web/studio.js), which talks to this class through "studio" events.
class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    MainComponent (AudioEngine& e, juce::PropertiesFile& props, juce::File dataFolder)
        : engine (e), settings (props), appData (dataFolder)
    {
        const juce::Component::SafePointer<MainComponent> self (this);
        auto options = juce::WebBrowserComponent::Options{}
                           .withNativeIntegrationEnabled()
                           .withKeepPageLoadedWhenBrowserIsHidden()
                           .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                           .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                        .withUserDataFolder (dataFolder.getChildFile ("WebView"))
                                                        .withStatusBarDisabled())
                           .withResourceProvider ([] (const juce::String& p) { return serveWebFile (p); })
                           .withEventListener ("studio", [self] (const juce::var& msg)
                           {
                               juce::MessageManager::callAsync ([safe = self, msg]
                               {
                                   if (safe != nullptr)
                                       safe->handleAction (msg);
                               });
                           });
        web = std::make_unique<StudioWebView> (options);
        addAndMakeVisible (*web);
        web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

        loadExercisePlugins();

        engine.knownPlugins.addChangeListener (this);
        engine.deviceManager.addChangeListener (this);
        startTimerHz (20);
        setSize (1280, 860);
    }

    ~MainComponent() override
    {
        storeCurrentExercisePlugin();
        engine.knownPlugins.removeChangeListener (this);
        engine.deviceManager.removeChangeListener (this);
        editorWindow.reset();
        engine.saveState();
    }

    void resized() override { web->setBounds (getLocalBounds()); }

private:
    // Requests from the page (web/studio.js).
    void handleAction (const juce::var& msg)
    {
        const auto action = msg.getProperty ("action", {}).toString();

        if      (action == "audio")  showAudioSettings();
        else if (action == "scan")   showPluginChooser();
        else if (action == "show")   showPluginEditor();
        else if (action == "mute")   engine.setToneOn (! (bool) msg.getProperty ("on", false));
        else if (action == "listen") { listening = (bool) msg.getProperty ("on", false); engine.setTapOn (listening); }
        else if (action == "unload") { editorWindow.reset(); engine.removePlugin(); message = {}; clearExercisePlugin(); }
        else if (action == "exercise") switchExercise (msg.getProperty ("id", {}).toString());
        else if (action == "list")   sendPluginList();
        else if (action == "checkUpdate") checkForUpdate (true);
        else if (action == "update") startUpdate();
        else if (action == "load")
        {
            const auto id = msg.getProperty ("id", {}).toString();
            for (const auto& pd : engine.knownPlugins.getTypes())
                if (pd.createIdentifierString() == id)
                {
                    loadPlugin (pd);
                    break;
                }
        }

        sendStatus();
    }

    juce::String currentPluginId() const
    {
        auto* p = engine.getPlugin();
        return p != nullptr ? p->getPluginDescription().createIdentifierString() : juce::String();
    }

    //==============================================================================
    // Each exercise remembers its plugin and that plugin's settings, in
    // "exercise-plugins.json" in Studio's data folder. Only one plugin runs at a time.

    juce::File exercisePluginsFile() const { return appData.getChildFile ("exercise-plugins.json"); }

    void loadExercisePlugins()
    {
        exercisePlugins = juce::JSON::parse (exercisePluginsFile());
        if (! exercisePlugins.isObject())
            exercisePlugins = juce::var (new juce::DynamicObject());
    }

    void saveExercisePlugins()
    {
        exercisePluginsFile().replaceWithText (juce::JSON::toString (exercisePlugins));
    }

    void storeCurrentExercisePlugin()
    {
        auto* obj = exercisePlugins.getDynamicObject();
        if (obj == nullptr || currentExercise.isEmpty() || engine.getPlugin() == nullptr || loadingFor.isNotEmpty())
            return; // nothing loaded, or a plugin is still loading for some exercise

        auto* entry = new juce::DynamicObject();
        entry->setProperty ("plugin", engine.getPluginId());
        entry->setProperty ("name", engine.getPluginName());
        entry->setProperty ("state", engine.getPluginState().toBase64Encoding());
        obj->setProperty (currentExercise, juce::var (entry));
        saveExercisePlugins();
    }

    void clearExercisePlugin()
    {
        if (auto* obj = exercisePlugins.getDynamicObject(); obj != nullptr && currentExercise.isNotEmpty())
        {
            obj->removeProperty (currentExercise);
            saveExercisePlugins();
        }
    }

    void switchExercise (const juce::String& exId)
    {
        if (exId == currentExercise)
            return;

        storeCurrentExercisePlugin();
        currentExercise = exId;

        if (loadingFor.isNotEmpty())
        {
            sendStatus();
            return; // applyExercise runs again when the load in progress finishes
        }
        applyExercise();
    }

    void applyExercise()
    {
        const auto exId = currentExercise;

        // One time: the plugin from before plugins were per exercise goes to the first exercise shown.
        if (! settings.getBoolValue ("perExerciseMigrated", false))
        {
            settings.setValue ("perExerciseMigrated", true);
            settings.saveIfNeeded();
            if (auto desc = settings.getXmlValue ("pluginDescription"); desc != nullptr && exId.isNotEmpty())
            {
                juce::PluginDescription pd;
                if (pd.loadFromXml (*desc))
                {
                    auto* entry = new juce::DynamicObject();
                    entry->setProperty ("plugin", pd.createIdentifierString());
                    entry->setProperty ("name", pd.name);
                    entry->setProperty ("state", settings.getValue ("pluginState"));
                    exercisePlugins.getDynamicObject()->setProperty (exId, juce::var (entry));
                    saveExercisePlugins();
                }
            }
        }

        const auto entry = exercisePlugins.getProperty (exId, {});
        const auto pluginId = entry.getProperty ("plugin", {}).toString();
        juce::MemoryBlock state;
        state.fromBase64Encoding (entry.getProperty ("state", {}).toString());

        message = {};
        if (pluginId.isEmpty())
        {
            editorWindow.reset();
            engine.removePlugin(); // empty slot: raw guitar
        }
        else if (pluginId == engine.getPluginId())
        {
            engine.setPluginState (state); // same plugin, this exercise's settings
        }
        else
        {
            const juce::PluginDescription* found = nullptr;
            for (const auto& pd : engine.knownPlugins.getTypes())
                if (pd.createIdentifierString() == pluginId) { found = &pd; break; }

            editorWindow.reset();
            if (found == nullptr)
            {
                engine.removePlugin();
                message = "Couldn't find " + entry.getProperty ("name", "this exercise's plugin").toString() + ". Scan for plugins in Settings.";
            }
            else
            {
                const auto pd = *found;
                message = "Loading " + pd.name + "...";
                loadingFor = exId;
                engine.loadPlugin (pd, state, [this, exId, name = pd.name] (const juce::String& error)
                {
                    loadingFor = {};
                    message = error.isNotEmpty() ? "Couldn't load " + name + ": " + error : juce::String();
                    if (exId != currentExercise)
                        applyExercise(); // you switched exercises while it was loading
                    sendStatus();
                });
            }
        }
        sendStatus();
    }

    // "Interface · In: Input 1 · Out: Output 1, Output 2"
    juce::String deviceSummary() const
    {
        auto* dev = engine.deviceManager.getCurrentAudioDevice();
        if (dev == nullptr)
            return "No audio device";

        auto names = [] (const juce::StringArray& all, const juce::BigInteger& active)
        {
            juce::StringArray on;
            for (int i = 0; i < all.size(); ++i)
                if (active[i]) on.add (all[i]);
            return on.isEmpty() ? juce::String ("none") : on.joinIntoString (", ");
        };

        const auto dot = " " + juce::String::charToString ((juce::juce_wchar) 0x00b7) + " ";
        return dev->getName()
             + dot + "In: "  + names (dev->getInputChannelNames(),  dev->getActiveInputChannels())
             + dot + "Out: " + names (dev->getOutputChannelNames(), dev->getActiveOutputChannels());
    }

    // Tells the page which plugin is loaded, whether it's muted, and the audio setup.
    void sendStatus()
    {
        if (web == nullptr || ! web->ready)
            return;

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("plugin", engine.getPluginName());
        obj->setProperty ("pluginId", currentPluginId());
        obj->setProperty ("muted", ! engine.isToneOn());
        obj->setProperty ("message", message);
        obj->setProperty ("device", deviceSummary());
        obj->setProperty ("build", STUDIO_BUILD);
        web->emitEventIfBrowserIsVisible ("studioStatus", juce::var (obj));
    }

    //==============================================================================
    // Updates: the page shows "Update to build N" when GitHub has a newer build.
    void sendUpdate (const juce::String& state, int build = 0, const juce::String& error = {})
    {
        if (web == nullptr || ! web->ready)
            return;
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("state", state);   // checking, none, available, downloading, failed, local
        obj->setProperty ("build", build);
        obj->setProperty ("current", STUDIO_BUILD);
        obj->setProperty ("error", error);
        web->emitEventIfBrowserIsVisible ("studioUpdate", juce::var (obj));
    }

    void checkForUpdate (bool asked)
    {
        if (STUDIO_BUILD == 0)
        {
            if (asked) sendUpdate ("local");   // built on this computer: updates come from rebuilding
            return;
        }
        if (asked) sendUpdate ("checking");
        const juce::Component::SafePointer<MainComponent> self (this);
        juce::Thread::launch ([safe = self, asked]
        {
            const int latest = latestReleaseBuild();
            juce::MessageManager::callAsync ([safe, asked, latest]
            {
                if (safe == nullptr) return;
                if (latest > STUDIO_BUILD)  safe->sendUpdate ("available", latest);
                else if (asked)             safe->sendUpdate (latest > 0 ? "none" : "failed", latest,
                                                              latest > 0 ? juce::String() : "Couldn't reach GitHub.");
            });
        });
    }

    void startUpdate()
    {
        if (updating || STUDIO_BUILD == 0)
            return;
        updating = true;
        sendUpdate ("downloading");
        const juce::Component::SafePointer<MainComponent> self (this);
        juce::Thread::launch ([safe = self]
        {
            const auto error = downloadAndStartUpdate();
            juce::MessageManager::callAsync ([safe, error]
            {
                if (safe == nullptr) return;
                if (error.isEmpty())
                {
                    safe->storeCurrentExercisePlugin();
                    safe->engine.saveState();
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    return;
                }
                safe->updating = false;
                safe->sendUpdate ("failed", 0, error);
            });
        });
    }

    // Every scanned plugin, plus the recently used ones (newest first).
    void sendPluginList()
    {
        if (web == nullptr || ! web->ready)
            return;

        juce::Array<juce::var> list;
        for (const auto& pd : engine.knownPlugins.getTypes())
        {
            if (pd.isInstrument)
                continue; // only effects take a guitar input

            auto* o = new juce::DynamicObject();
            o->setProperty ("id", pd.createIdentifierString());
            o->setProperty ("name", pd.name);
            o->setProperty ("maker", pd.manufacturerName);
            o->setProperty ("format", pd.pluginFormatName);
            list.add (juce::var (o));
        }

        juce::Array<juce::var> recent;
        for (const auto& id : juce::StringArray::fromLines (settings.getValue ("recentPlugins")))
            if (id.isNotEmpty())
                recent.add (id);

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("plugins", list);
        obj->setProperty ("recent", recent);
        web->emitEventIfBrowserIsVisible ("studioPlugins", juce::var (obj));
    }

    void rememberRecent (const juce::String& id)
    {
        auto ids = juce::StringArray::fromLines (settings.getValue ("recentPlugins"));
        ids.removeString (id);
        ids.removeEmptyStrings();
        ids.insert (0, id);
        while (ids.size() > 6)
            ids.remove (ids.size() - 1);
        settings.setValue ("recentPlugins", ids.joinIntoString ("\n"));
        settings.saveIfNeeded();
    }

    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        if (source == &engine.knownPlugins)
            sendPluginList();
        else
            sendStatus();
    }

    void timerCallback() override
    {
        if (web == nullptr || ! web->ready)
            return;

        if (! sentFirstStatus)
        {
            sentFirstStatus = true;
            sendStatus();
            sendPluginList();
            checkForUpdate (false);
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("level", juce::jlimit (0.0f, 1.0f, engine.getInputLevel()));
        web->emitEventIfBrowserIsVisible ("studioLevel", juce::var (obj));

        if (listening)
        {
            // The newest guitar samples for the Fretboard Trainer's note finder and tuner.
            constexpr int n = 1024;
            float data[n];
            engine.readTap (data, n);
            juce::Array<juce::var> arr;
            arr.ensureStorageAllocated (n);
            for (float v : data)
                arr.add (std::round (v * 100000.0f) / 100000.0f);
            auto* in = new juce::DynamicObject();
            in->setProperty ("sr", engine.getTapRate());
            in->setProperty ("d", arr);
            web->emitEventIfBrowserIsVisible ("studioInput", juce::var (in));
        }
    }

    void showAudioSettings()
    {
        if (audioWindow == nullptr)
        {
            auto* selector = new juce::AudioDeviceSelectorComponent (engine.deviceManager,
                                                                     1, 2,   // inputs: the guitar is the first one turned on
                                                                     2, 2,   // stereo out
                                                                     false, false, false, false);
            selector->setSize (520, 440);
            audioWindow = std::make_unique<ToolWindow> ("Audio settings", selector, true,
                                                        [this] { audioWindow.reset(); engine.saveState(); });
        }
        audioWindow->toFront (true);
    }

    void showPluginChooser()
    {
        if (chooserWindow == nullptr)
        {
            auto* chooser = new PluginChooser (engine, settings, appData.getChildFile ("plugin-scan-crash-check.txt"),
                                               [this] (const juce::PluginDescription& pd) { loadPlugin (pd); });
            chooserWindow = std::make_unique<ToolWindow> ("Scan for plugins", chooser, true,
                                                          [this] { chooserWindow.reset(); engine.saveState(); });
        }
        chooserWindow->toFront (true);
    }

    void loadPlugin (const juce::PluginDescription& pd)
    {
        editorWindow.reset(); // the old plugin's window must close before the plugin goes away
        message = "Loading " + pd.name + "...";
        sendStatus();

        loadingFor = currentExercise.isNotEmpty() ? currentExercise : juce::String ("-");
        engine.loadPlugin (pd, {}, [this, id = pd.createIdentifierString(), forEx = currentExercise] (const juce::String& error)
        {
            loadingFor = {};
            message = error.isNotEmpty() ? "Couldn't load the plugin: " + error : juce::String();

            if (forEx != currentExercise)
            {
                applyExercise(); // you switched exercises while it was loading
                sendStatus();
                return;
            }

            if (error.isEmpty())
            {
                storeCurrentExercisePlugin();
                rememberRecent (id);
                sendPluginList();
                chooserWindow.reset();
                showPluginEditor();
            }

            sendStatus();
        });
    }

    void showPluginEditor()
    {
        auto* plugin = engine.getPlugin();
        if (plugin == nullptr)
            return;

        if (editorWindow == nullptr)
        {
            juce::Component* content = plugin->hasEditor() ? plugin->createEditorAndMakeActive() : nullptr;
            if (content == nullptr)
                content = new juce::GenericAudioProcessorEditor (*plugin);

            editorWindow = std::make_unique<ToolWindow> (plugin->getName(), content, true,
                                                         [this] { editorWindow.reset(); storeCurrentExercisePlugin(); engine.saveState(); });
            // Let the green button and window edges resize the plugin when its editor allows it
            // (as in the plugin's own standalone app).
            const bool canResize = dynamic_cast<juce::AudioProcessorEditor*> (content) != nullptr
                                   && static_cast<juce::AudioProcessorEditor*> (content)->isResizable();
            editorWindow->setResizable (canResize, false);
        }
        editorWindow->toFront (true);
    }

    AudioEngine& engine;
    juce::PropertiesFile& settings;
    juce::File appData;
    juce::String message;
    juce::String currentExercise, loadingFor;
    juce::var exercisePlugins;
    bool sentFirstStatus = false;
    bool listening = false;
    bool updating = false;

    std::unique_ptr<StudioWebView> web;
    std::unique_ptr<ToolWindow> audioWindow, chooserWindow, editorWindow;
};
} // namespace

//==============================================================================
class GuitarPracticeStudioApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        juce::PropertiesFile::Options opt;
        opt.applicationName     = "Guitar Practice Studio";
        opt.filenameSuffix      = ".settings";
        opt.folderName          = "Guitar Practice Studio";
        opt.osxLibrarySubFolder = "Application Support";
        opt.storageFormat       = juce::PropertiesFile::storeAsXML;
        settings = std::make_unique<juce::PropertiesFile> (opt);

        const auto dataFolder = settings->getFile().getParentDirectory();
        dataFolder.createDirectory();

        engine = std::make_unique<AudioEngine> (*settings);
        mainWindow = std::make_unique<MainWindow> (getApplicationName(),
                                                   new MainComponent (*engine, *settings, dataFolder));
    }

    void shutdown() override
    {
        mainWindow.reset();
        engine.reset();
        settings.reset();
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::Component* content)
            : DocumentWindow (name, juce::Colour (0xff15181d), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, true);
            setResizeLimits (900, 600, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (GuitarPracticeStudioApplication)
