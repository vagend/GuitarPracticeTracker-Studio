#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "AudioEngine.h"

#if JUCE_MAC
 #include <unistd.h>   // getpid, for the updater
 #include <spawn.h>
 #include <fcntl.h>
 extern char** environ;
#endif

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dwmapi.h>   // dark title bars to match dark colour schemes
 #pragma comment(lib, "dwmapi.lib")
#endif

#ifndef STUDIO_BUILD
 #define STUDIO_BUILD 0   // set by GitHub Actions; 0 means built on this computer
#endif

namespace
{
// Updates come from the GitHub release built by .github/workflows/release.yml: "latest" for
// the public app, "test-build" for Guitar Practice Studio Test (see STUDIO_CHANNEL in CMakeLists.txt).
#ifndef STUDIO_RELEASE_URL
 #define STUDIO_RELEASE_URL "https://github.com/vagend/GuitarPracticeTracker-Studio/releases/latest/download/"
 #define STUDIO_FILES "GuitarPracticeStudio"
 #define STUDIO_TEST 0
#endif
const juce::String appName (JUCE_APPLICATION_NAME_STRING);   // "Guitar Practice Studio" or "... Test"
const juce::String releaseFiles (STUDIO_RELEASE_URL);

#if JUCE_MAC
const juce::String updateAsset (juce::String (STUDIO_FILES) + "-mac.zip");
#else
const juce::String updateAsset (juce::String (STUDIO_FILES) + "-setup.exe");
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
                    .getChildFile (juce::String (STUDIO_FILES) + "Update");
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

    const auto newApp = unpacked.getChildFile (appName + ".app");
    const auto oldApp = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    if (! newApp.isDirectory())
        return "The update didn't contain the app.";

    const auto q = [] (const juce::File& f) { return f.getFullPathName().quoted ('\'') ; };
    const auto old = oldApp.getSiblingFile ("." + appName + " (previous).app");
    const auto log = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                         .getChildFile ("Library/Logs/" + appName + " update.log");
    log.getParentDirectory().createDirectory();
    const auto script = temp.getChildFile ("install.sh");
    // Move the old app aside, copy the new one in, put the old one back if that fails, then open it.
    script.replaceWithText ("#!/bin/bash\n"
                            "echo \"$(date) updating " + oldApp.getFullPathName() + "\"\n"
                            "while kill -0 " + juce::String ((int) getpid()) + " 2>/dev/null; do sleep 0.5; done\n"
                            "rm -rf " + q (old) + "\n"
                            "if mv " + q (oldApp) + " " + q (old) + "; then\n"
                            "  if /usr/bin/ditto " + q (newApp) + " " + q (oldApp) + "; then rm -rf " + q (old) + "; echo installed\n"
                            "  else echo 'copy failed, keeping the old version'; rm -rf " + q (oldApp) + "; mv " + q (old) + " " + q (oldApp) + "; fi\n"
                            "else echo \"couldn't move the old app, nothing changed\"; fi\n"
                            "/usr/bin/xattr -dr com.apple.quarantine " + q (oldApp) + " 2>/dev/null\n"
                            "open " + q (oldApp) + " && echo reopened\n",
                            false, false, "\n");   // Unix line endings: bash can't run a CRLF script

    // The script must outlive Studio. macOS kills every process left in a quitting app's
    // process group, so start it in a group of its own, with its output going to the log.
    posix_spawnattr_t attr;
    posix_spawnattr_init (&attr);
    posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup (&attr, 0);
    posix_spawn_file_actions_t io;
    posix_spawn_file_actions_init (&io);
    const auto logPath = log.getFullPathName().toStdString();
    posix_spawn_file_actions_addopen (&io, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen (&io, 1, logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2 (&io, 1, 2);
    auto scriptPath = script.getFullPathName().toStdString();
    char bash[] = "/bin/bash";
    char* args[] = { bash, scriptPath.data(), nullptr };
    pid_t pid = 0;
    const auto rc = posix_spawn (&pid, "/bin/bash", &io, &attr, args, environ);
    posix_spawn_file_actions_destroy (&io);
    posix_spawnattr_destroy (&attr);
    if (rc != 0)
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
    if (ext == ".woff")        return "font/woff";
    if (ext == ".otf")         return "font/otf";
    if (ext == ".mjs")         return "text/javascript";
    return "application/octet-stream";
}

// Studio is its own app: give the shared page Studio's name, version and "What's new"
// list (from studio.js). Each change is skipped if the page no longer contains its text.
juce::MemoryBlock brandAsStudio (juce::String html)
{
    html = html.replace ("<title>Guitar Practice</title>",
                         "<title>" + appName + "</title><script src=\"studio.js\"></script>")
               .replace ("<h1>Guitar Practice</h1>", "<h1>" + appName + "</h1>")
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
// Studio's own windows (Audio settings, Scan for plugins, plugin windows without their own
// look) use the page's colour scheme; the page sends its colours whenever the scheme changes.
struct StudioLookAndFeel final : public juce::LookAndFeel_V4
{
    juce::Colour bg { 0xff0d0a07 }, card { 0xff17120c }, line { 0xff3a2c1a }, fg { 0xfff1e6d2 },
                 acc { 0xffd4a64a }, accFg { 0xff1a1205 }, soft { 0xff1f180f };

    StudioLookAndFeel() { apply(); }

    void apply()
    {
        setColourScheme ({ bg, card, card, line, fg, soft, accFg, acc, fg });
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::DocumentWindow::textColourId, fg);
        setColour (juce::TextButton::buttonColourId, soft);
        setColour (juce::TextButton::buttonOnColourId, acc);
        setColour (juce::TextButton::textColourOffId, fg);
        setColour (juce::TextButton::textColourOnId, accFg);
        setColour (juce::ComboBox::backgroundColourId, card);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ListBox::backgroundColourId, card);
        setColour (juce::ListBox::outlineColourId, line);
        setColour (juce::TableHeaderComponent::backgroundColourId, soft);
        setColour (juce::TableHeaderComponent::textColourId, fg);
        setColour (juce::TableHeaderComponent::outlineColourId, line);
        setColour (juce::Label::textColourId, fg);
        setColour (juce::ToggleButton::textColourId, fg);
        setColour (juce::ToggleButton::tickColourId, acc);
        setColour (juce::Slider::thumbColourId, acc);
        setColour (juce::Slider::trackColourId, acc.withAlpha (0.6f));
        setColour (juce::PopupMenu::backgroundColourId, card);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, acc);
        setColour (juce::PopupMenu::highlightedTextColourId, accFg);
        setColour (juce::AlertWindow::backgroundColourId, card);
        setColour (juce::AlertWindow::textColourId, fg);
        setColour (juce::ScrollBar::thumbColourId, line);
        setColour (juce::TextEditor::backgroundColourId, card);
        setColour (juce::TextEditor::outlineColourId, line);
        setColour (juce::TextEditor::textColourId, fg);
    }

    // colours arrive as "#rrggbb" strings from the page
    void setFrom (const juce::var& t)
    {
        const auto get = [&t] (const char* key, juce::Colour fallback)
        {
            const auto v = t.getProperty (key, {}).toString().trim().trimCharactersAtStart ("#");
            return v.length() == 6 ? juce::Colour (0xff000000 | (juce::uint32) v.getHexValue32()) : fallback;
        };
        bg = get ("bg", bg); card = get ("card", card); line = get ("line", line); fg = get ("fg", fg);
        acc = get ("acc", acc); accFg = get ("accfg", accFg); soft = get ("soft", soft);
        apply();
    }
};

StudioLookAndFeel& studioLook()
{
    static StudioLookAndFeel lf;
    return lf;
}

// Windows draws title bars light unless asked; match a dark colour scheme.
void matchTitleBar (juce::Component& window)
{
   #if JUCE_WINDOWS
    if (auto* hwnd = (HWND) window.getWindowHandle())
    {
        BOOL dark = studioLook().bg.getPerceivedBrightness() < 0.5f ? TRUE : FALSE;
        DwmSetWindowAttribute (hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof (dark));
    }
   #else
    juce::ignoreUnused (window);
   #endif
}

//==============================================================================
// A plain window that hides or deletes itself when its close button is clicked.
class ToolWindow final : public juce::DocumentWindow
{
public:
    ToolWindow (const juce::String& title, juce::Component* content, bool ownsContent,
                std::function<void()> onCloseIn)
        : DocumentWindow (title, studioLook().bg, DocumentWindow::allButtons),
          onClose (std::move (onCloseIn))
    {
        setUsingNativeTitleBar (true);
        if (ownsContent) setContentOwned (content, true);
        else             setContentNonOwned (content, true);
        setResizable (true, false);
        centreWithSize (getWidth(), getHeight());
        keepOnScreen();
        setVisible (true);
        matchTitleBar (*this);
        toFront (true);
    }

    // A plugin can reopen bigger than the screen (it remembers its size). Keep the title bar
    // on screen so the window can always be moved, and shrink it if it can be resized.
    void keepOnScreen()
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* d = displays.getDisplayForRect (getBounds());
        if (d == nullptr) d = displays.getPrimaryDisplay();
        if (d == nullptr) return;
        const auto area = d->userBounds.toNearestInt();
        auto b = getBounds();
        if (isResizable())
            b.setSize (juce::jmin (b.getWidth(), area.getWidth()), juce::jmin (b.getHeight(), area.getHeight()));
        b.setX (juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - b.getWidth()), b.getX()));
        b.setY (juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - b.getHeight()), b.getY()));
        setBounds (b);
    }

    void lookAndFeelChanged() override
    {
        DocumentWindow::lookAndFeelChanged();
        setBackgroundColour (studioLook().bg);
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
// Both web views (the practice page and the tab window) share these: same files, same data folder.
juce::WebBrowserComponent::Options studioWebOptions (const juce::File& dataFolder)
{
    return juce::WebBrowserComponent::Options{}
               .withNativeIntegrationEnabled()
               .withKeepPageLoadedWhenBrowserIsHidden()
               .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                            .withUserDataFolder (dataFolder.getChildFile ("WebView"))
                                            .withStatusBarDisabled())
               .withResourceProvider ([] (const juce::String& p) { return serveWebFile (p); });
}

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    MainComponent (AudioEngine& e, juce::PropertiesFile& props, juce::File dataFolder)
        : engine (e), settings (props), appData (dataFolder)
    {
        const juce::Component::SafePointer<MainComponent> self (this);
        auto options = studioWebOptions (dataFolder)
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
        engine.tabSynth.load (getWebFolder().getChildFile ("vendor/alphatab/soundfont/sonivox.sf2"));

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
        closeTabWindow (false);
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
        else if (action == "inputMute") engine.setInputOn (! (bool) msg.getProperty ("on", false));
        else if (action == "listen") { listening = (bool) msg.getProperty ("on", false); engine.setTapOn (listening); }
        else if (action == "unload") { editorWindow.reset(); engine.removePlugin(); message = {}; clearExercisePlugin(); }
        else if (action == "exercise") switchExercise (msg.getProperty ("id", {}).toString());
        else if (action == "list")   sendPluginList();
        else if (action == "checkUpdate") checkForUpdate (true);
        else if (action == "theme")
        {
            studioLook().setFrom (msg);
            for (auto* w : { audioWindow.get(), chooserWindow.get(), editorWindow.get(), tabWindow.get() })
                if (w != nullptr) { w->sendLookAndFeelChange(); matchTitleBar (*w); w->repaint(); }
            if (auto* top = getTopLevelComponent())
                matchTitleBar (*top);
        }
        else if (action == "tab")    { tabMessage (msg.getProperty ("msg", {})); return; }
        else if (action == "tabPick") { pickTabFile(); return; }
        else if (action == "pickFile") { pickFileFor (msg); return; }
        else if (action == "tabEdit") { editTabFile (msg.getProperty ("path", {}).toString()); return; }
        else if (action == "tabWatch") { watchTabFiles (msg.getProperty ("files", {})); return; }
        else if (action == "tabEditorPick") { pickTabEditor (msg.getProperty ("then", {}).toString()); return; }
        else if (action == "tabEditorClear") { settings.setValue ("tabEditor", juce::String()); settings.saveIfNeeded(); }
        else if (action == "metHits") { scheduleMetronome (msg.getProperty ("hits", {})); return; }
        else if (action == "metClear") { engine.metronome.clearScheduled(); engine.tabSynth.clearScheduled(); return; }
        else if (action == "tabNotes") { scheduleTab (msg.getProperty ("ev", {})); return; }
        else if (action == "tabVol")   { engine.tabSynth.setVolume ((float) (double) msg.getProperty ("v", 0.8)); return; }
        else if (action == "metSample") { setMetronomeSample (msg); return; }
        else if (action == "showData") appData.revealToUser();
        else if (action == "openSource") juce::URL ("https://github.com/vagend/GuitarPracticeTracker-Studio").launchInDefaultBrowser();
        else if (action == "autoUpdate") { settings.setValue ("autoUpdateCheck", (bool) msg.getProperty ("on", true)); settings.saveIfNeeded(); }
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
        obj->setProperty ("test", STUDIO_TEST != 0);   // Guitar Practice Studio Test
        obj->setProperty ("autoUpdate", settings.getBoolValue ("autoUpdateCheck", true));
        obj->setProperty ("inputMuted", ! engine.isInputOn());
        obj->setProperty ("tabEditor", tabEditorName());
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
                else if (asked)           safe->sendUpdate (latest > 0 ? "none" : "failed", latest,
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
        {
            // a change while Audio settings is open is your choice of device
            if (audioWindow != nullptr && ! retryingDevice)
            {
                engine.deviceChosen();
                if (engine.getPlugin() == nullptr && loadingFor.isEmpty())
                    applyExercise(); // a plugin that couldn't load without a device
            }
            else if (! retryingDevice)
            {
                followDevice (true);   // unplugged, switched off or back again
            }
            sendStatus();
        }
    }

    void followDevice (bool devicesChanged)
    {
        const bool wasOpen = engine.isWantedDeviceOpen();
        retryingDevice = true;
        engine.followWantedDevice (devicesChanged);
        retryingDevice = false;
        if (engine.isWantedDeviceOpen())
        {
            if (! wasOpen)
            {
                message = {};
                if (engine.getPlugin() == nullptr && loadingFor.isEmpty())
                    applyExercise();
            }
        }
        else if (engine.reopenGaveUp())
            message = "Couldn't open your audio interface, so sound plays through your computer and there's no guitar input. Check it's on and connected, or pick another in Settings > Audio interface.";
        else
            message = "Your audio interface isn't connected: sound plays through your computer and there's no guitar input until it's back. Studio switches back on its own.";
    }

    void timerCallback() override
    {
        if (web == nullptr || ! web->ready)
            return;

        // Your audio interface is missing (busy at start-up, unplugged or switched off):
        // look for it every 2 s.
        if (! engine.isWantedDeviceOpen() && ++deviceRetryTicks >= 40)
        {
            deviceRetryTicks = 0;
            followDevice (false);
            sendStatus();
        }

        // Tab files saved in a tab editor: reload them about once a second.
        if (++tabWatchTicks >= 20)
        {
            tabWatchTicks = 0;
            checkTabFiles();
        }

        if (! sentFirstStatus)
        {
            sentFirstStatus = true;
            sendStatus();
            sendPluginList();
            if (settings.getBoolValue ("autoUpdateCheck", true))
                checkForUpdate (false);
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("level", juce::jlimit (0.0f, 1.0f, engine.getInputLevel()));
        // The engine's clock, for the metronome: the page schedules its hits on it. "ok" means the
        // clock is running (on your interface, or the computer's output without it); otherwise the page uses Web Audio.
        const double clock = engine.metronome.now();
        obj->setProperty ("t", clock);
        obj->setProperty ("ok", engine.isOutputPlaying() && clock > lastClock);
        obj->setProperty ("lat", engine.outputLatencySeconds());
        lastClock = clock;
        web->emitEventIfBrowserIsVisible ("studioLevel", juce::var (obj));

        if (listening)
        {
            // The newest guitar samples for the Fretboard Trainer's note finder and tuner.
            constexpr int n = 2048;   // about 170 ms: enough for low notes in the tuner
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

    // The metronome's hits from the page: [time on the engine's clock, sound, fallback sound, gain].
    void scheduleMetronome (const juce::var& hits)
    {
        if (auto* list = hits.getArray())
            for (auto& h : *list)
                if (auto* a = h.getArray(); a != nullptr && a->size() >= 4)
                    engine.metronome.schedule ((double) (*a)[0], (int) (*a)[1], (int) (*a)[2], (float) (double) (*a)[3]);
    }

    // The tab's notes for the next bar: [time on the engine's clock, type (0 on, 1 off, 2 program), channel, key, velocity 0..1].
    void scheduleTab (const juce::var& ev)
    {
        if (auto* list = ev.getArray())
            for (auto& e : *list)
                if (auto* a = e.getArray(); a != nullptr && a->size() >= 5)
                    engine.tabSynth.schedule ((double) (*a)[0], (int) (*a)[1], (int) (*a)[2], (int) (*a)[3], (float) (double) (*a)[4]);
    }

    // Your own sample for a metronome voice ("p", "s" or "m"), as base64 file bytes; empty removes it.
    void setMetronomeSample (const juce::var& msg)
    {
        const auto v = msg.getProperty ("v", {}).toString();
        const int voice = juce::StringArray { "p", "s", "m" }.indexOf (v);
        if (voice < 0)
            return;
        juce::MemoryOutputStream bytes;
        const auto data = msg.getProperty ("data", {}).toString();
        if (data.isNotEmpty() && ! juce::Base64::convertFromBase64 (bytes, data))
            bytes.reset();
        const auto error = engine.metronome.setUserSample (voice, bytes.getMemoryBlock());
        if (error.isNotEmpty() && web != nullptr)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("v", v);
            o->setProperty ("error", error);
            web->emitEventIfBrowserIsVisible ("studioMet", juce::var (o));
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
            editorWindow->keepOnScreen();
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
    double lastClock = -1.0;   // the metronome clock at the last tick (is it moving?)
    bool retryingDevice = false;
    int deviceRetryTicks = 0;
    bool updating = false;

    std::unique_ptr<StudioWebView> web;
    std::unique_ptr<ToolWindow> audioWindow, chooserWindow, editorWindow;

    //==========================================================================
    // The tab window: web/tab.html in a window of its own. This class only passes messages
    // between it and the practice page ("tab" events to it, "studioTab" events back).
    std::unique_ptr<StudioWebView> tabWeb;
    std::unique_ptr<ToolWindow> tabWindow;

    //==========================================================================
    // Tab files: Studio's own file picker (so it knows where the file is), opening a tab in your
    // tab editor, and reloading it when you save it there. Results go to the page as
    // "studioTabFile" events: { path, name, data (base64), mtime, reload } or { error, path }.
    std::unique_ptr<juce::FileChooser> chooser;
    struct WatchedTab { juce::String ex, path; juce::int64 mtime = 0; };
    std::vector<WatchedTab> watchedTabs;
    int tabWatchTicks = 0;
    static constexpr juce::int64 maxTabBytes = 2 * 1024 * 1024;

    void sendTabFile (const juce::File& f, const juce::String& ex, bool reload)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("path", f.getFullPathName());
        o->setProperty ("name", f.getFileName());
        o->setProperty ("ex", ex);
        o->setProperty ("reload", reload);
        juce::MemoryBlock mb;
        if (! f.existsAsFile())                        o->setProperty ("error", "missing");
        else if (f.getSize() > maxTabBytes)            o->setProperty ("error", "big");
        else if (! f.loadFileAsData (mb))              o->setProperty ("error", "read");
        else
        {
            o->setProperty ("data", juce::Base64::toBase64 (mb.getData(), mb.getSize()));
            o->setProperty ("mtime", (double) f.getLastModificationTime().toMilliseconds());
        }
        web->emitEventIfBrowserIsVisible ("studioTabFile", juce::var (o));
    }

    // Each browse button opens in the folder it was last used in ("lastFolder.<button>" in settings).
    juce::File lastFolder (const juce::String& key, const juce::File& fallback) const
    {
        const auto path = settings.getValue ("lastFolder." + key);
        if (path.isEmpty() || ! juce::File::isAbsolutePath (path))
            return fallback;
        const juce::File folder (path);
        return folder.isDirectory() ? folder : fallback;
    }

    void rememberFolder (const juce::String& key, const juce::File& chosen)
    {
        settings.setValue ("lastFolder." + key, chosen.getParentDirectory().getFullPathName());
        settings.saveIfNeeded();
    }

    // A file for one of the page's browse buttons (your metronome samples, restoring a backup), with
    // Studio's own picker so it can open where that button was last used. The answer goes back as a
    // "studioPickedFile" event: { key, name, data (base64) }, { key, cancel } or { key, error }.
    void pickFileFor (const juce::var& msg)
    {
        const auto key = msg.getProperty ("key", {}).toString();
        if (key.isEmpty())
            return;
        const auto maxBytes = (juce::int64) (double) msg.getProperty ("max", 20.0 * 1024 * 1024);
        chooser = std::make_unique<juce::FileChooser> (msg.getProperty ("title", "Choose a file").toString(),
                      lastFolder (key, juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)),
                      msg.getProperty ("patterns", "*").toString());
        const juce::Component::SafePointer<MainComponent> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [self, key, maxBytes] (const juce::FileChooser& fc)
        {
            if (self == nullptr) return;
            auto* o = new juce::DynamicObject();
            o->setProperty ("key", key);
            const auto f = fc.getResult();
            juce::MemoryBlock mb;
            if (f == juce::File())                 o->setProperty ("cancel", true);
            else if (f.getSize() > maxBytes)       o->setProperty ("error", "big");
            else if (! f.loadFileAsData (mb))      o->setProperty ("error", "read");
            else
            {
                o->setProperty ("name", f.getFileName());
                o->setProperty ("data", juce::Base64::toBase64 (mb.getData(), mb.getSize()));
            }
            if (f != juce::File())
                self->rememberFolder (key, f);
            self->web->emitEventIfBrowserIsVisible ("studioPickedFile", juce::var (o));
        });
    }

    void pickTabFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a tab",
                      lastFolder ("tab", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)),
                      "*.gp;*.gp3;*.gp4;*.gp5;*.gpx;*.xml;*.musicxml;*.mxl;*.capx");
        const juce::Component::SafePointer<MainComponent> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [self] (const juce::FileChooser& fc)
        {
            if (self == nullptr) return;
            const auto f = fc.getResult();
            if (f == juce::File())
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("cancel", true);
                self->web->emitEventIfBrowserIsVisible ("studioTabFile", juce::var (o));
                return;
            }
            self->rememberFolder ("tab", f);
            self->sendTabFile (f, {}, false);
        });
    }

    // Your chosen tab editor (Settings), or whatever your computer opens this kind of file with.
    // If nothing opens it, the page says so and offers to choose a tab editor.
    void editTabFile (const juce::String& path)
    {
        const juce::File f (path);
        auto tell = [this, &path] (const char* error)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("error", error);
            o->setProperty ("path", path);
            o->setProperty ("edit", true);
            web->emitEventIfBrowserIsVisible ("studioTabFile", juce::var (o));
        };
        if (! juce::File::isAbsolutePath (path) || ! f.existsAsFile()) { tell ("missing"); return; }

        const juce::File app (settings.getValue ("tabEditor"));
        bool opened = false;
       #if JUCE_MAC
        // "open" reports whether anything could open the file (macOS shows nothing itself)
        juce::StringArray cmd { "/usr/bin/open" };
        if (app.exists()) cmd.addArray ({ "-a", app.getFullPathName() });
        cmd.add (f.getFullPathName());
        juce::ChildProcess open;
        if (open.start (cmd, juce::ChildProcess::wantStdErr))
        {
            open.readAllProcessOutput();
            opened = open.waitForProcessToFinish (10000) && open.getExitCode() == 0;
        }
       #else
        opened = app.exists() ? app.startAsProcess (f.getFullPathName().quoted())
                              : f.startAsProcess();
       #endif
        if (! opened)
            tell (app.exists() ? "editor" : "noapp");
    }

    void pickTabEditor (const juce::String& thenOpen = {})
    {
       #if JUCE_MAC
        const auto start = juce::File ("/Applications");
        const juce::String pattern ("*.app");
       #else
        const auto start = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);
        const juce::String pattern ("*.exe");
       #endif
        chooser = std::make_unique<juce::FileChooser> ("Choose your tab editor", lastFolder ("tabEditor", start), pattern);
        const juce::Component::SafePointer<MainComponent> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [self, thenOpen] (const juce::FileChooser& fc)
        {
            if (self == nullptr) return;
            const auto f = fc.getResult();
            if (f != juce::File())
            {
                self->rememberFolder ("tabEditor", f);
                self->settings.setValue ("tabEditor", f.getFullPathName());
                self->settings.saveIfNeeded();
            }
            self->sendStatus();
            if (f != juce::File() && thenOpen.isNotEmpty())
                self->editTabFile (thenOpen);
        });
    }

    juce::String tabEditorName() const
    {
        const juce::File app (settings.getValue ("tabEditor"));
        return app.exists() ? app.getFileNameWithoutExtension() : juce::String();
    }

    // The page lists the tab files exercises are linked to, with the time of the copy it has.
    void watchTabFiles (const juce::var& files)
    {
        watchedTabs.clear();
        if (auto* arr = files.getArray())
            for (const auto& v : *arr)
            {
                WatchedTab w;
                w.ex = v.getProperty ("ex", {}).toString();
                w.path = v.getProperty ("path", {}).toString();
                w.mtime = (juce::int64) (double) v.getProperty ("mtime", 0.0);
                if (w.path.isNotEmpty() && juce::File::isAbsolutePath (w.path))
                    watchedTabs.push_back (w);
            }
        checkTabFiles();   // also picks up files saved while Studio was closed
    }

    void checkTabFiles()
    {
        for (auto& w : watchedTabs)
        {
            const juce::File f (w.path);
            if (! f.existsAsFile()) continue;
            const auto t = f.getLastModificationTime().toMilliseconds();
            // reload once the save has finished (the time hasn't changed in the last half second)
            if (t > w.mtime && juce::Time::currentTimeMillis() - t > 500)
            {
                w.mtime = t;
                sendTabFile (f, w.ex, true);
            }
        }
    }

    void tabMessage (const juce::var& m)
    {
        const auto type = m.getProperty ("type", {}).toString();
        if (type == "close") { closeTabWindow (true); return; }
        if (type == "open" && tabWindow == nullptr) openTabWindow();
        if (tabWindow == nullptr) return;
        if (type == "open")
        {
            const auto name = m.getProperty ("name", {}).toString();
            tabWindow->setName (name.isNotEmpty() ? name + " - Tab" : juce::String ("Tab"));
            tabWindow->toFront (true);
        }
        if (tabWeb->ready)            // before the page is ready it asks for the tab itself ("ready")
            tabWeb->emitEventIfBrowserIsVisible ("tab", m);
    }

    void openTabWindow()
    {
        const juce::Component::SafePointer<MainComponent> self (this);
        auto options = studioWebOptions (appData)
                           .withEventListener ("tabwin", [self] (const juce::var& m)
                           {
                               juce::MessageManager::callAsync ([safe = self, m]
                               {
                                   if (safe != nullptr)
                                       safe->fromTabWindow (m);
                               });
                           });
        tabWeb = std::make_unique<StudioWebView> (options);
        tabWeb->setSize (1100, 720);
        tabWindow = std::make_unique<ToolWindow> ("Tab", tabWeb.get(), false, [self]
        {
            juce::MessageManager::callAsync ([safe = self] { if (safe != nullptr) safe->closeTabWindow (true); });
        });
        tabWindow->setResizeLimits (480, 320, 10000, 10000);
        const auto saved = settings.getValue ("tabWindow");
        if (saved.isNotEmpty())
            tabWindow->restoreWindowStateFromString (saved);
        tabWindow->keepOnScreen();
        tabWeb->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "tab.html");
    }

    void fromTabWindow (const juce::var& m)
    {
        if (tabWeb == nullptr) return;
        if (m.getProperty ("type", {}).toString() == "ready")
            tabWeb->ready = true;
        web->emitEventIfBrowserIsVisible ("studioTab", m);
    }

    void closeTabWindow (bool tellPage)
    {
        if (tabWindow == nullptr) return;
        settings.setValue ("tabWindow", tabWindow->getWindowStateAsString());
        tabWindow.reset();
        tabWeb.reset();
        if (tellPage && web != nullptr)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("type", "closed");
            web->emitEventIfBrowserIsVisible ("studioTab", juce::var (o));
        }
    }
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
        juce::LookAndFeel::setDefaultLookAndFeel (&studioLook());
        juce::PropertiesFile::Options opt;
        opt.applicationName     = appName;
        opt.filenameSuffix      = ".settings";
        opt.folderName          = appName;   // the test app keeps its own settings and data
        opt.osxLibrarySubFolder = "Application Support";
        opt.storageFormat       = juce::PropertiesFile::storeAsXML;
        settings = std::make_unique<juce::PropertiesFile> (opt);

        const auto dataFolder = settings->getFile().getParentDirectory();
        dataFolder.createDirectory();

        engine = std::make_unique<AudioEngine> (*settings);
        mainWindow = std::make_unique<MainWindow> (getApplicationName(),
                                                   new MainComponent (*engine, *settings, dataFolder),
                                                   *settings);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
            mainWindow->saveSize();
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        engine.reset();
        settings.reset();
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::Component* content, juce::PropertiesFile& s)
            : DocumentWindow (name, juce::Colour (0xff15181d), DocumentWindow::allButtons), props (s)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, true);

            // Fit the screen Studio opens on (laptops with display scaling can be smaller than
            // the app's preferred 1280 x 860), and reopen at whatever size you left it.
            const auto area = screenArea();
            setResizeLimits (juce::jmin (760, area.getWidth()), juce::jmin (520, area.getHeight()), 10000, 10000);

            const auto saved = props.getValue ("windowState");
            if (saved.isEmpty() || ! restoreWindowStateFromString (saved))
                centreWithSize (juce::jmin (1280, area.getWidth() * 92 / 100),
                                juce::jmin (860, area.getHeight() * 92 / 100));

            // Never larger than the screen or off it, even if the screen changed since last time.
            if (! isFullScreen())
                setBounds (getBounds().constrainedWithin (screenArea()));
            setVisible (true);
        }

        void saveSize()
        {
            props.setValue ("windowState", getWindowStateAsString());
            props.saveIfNeeded();
        }

        juce::Rectangle<int> screenArea() const
        {
            const auto& displays = juce::Desktop::getInstance().getDisplays();
            if (auto* d = displays.getDisplayForRect (getBounds()))
                return d->userBounds.toNearestInt();
            if (auto* d = displays.getPrimaryDisplay())
                return d->userBounds.toNearestInt();
            return { 0, 0, 1280, 860 };
        }

        void closeButtonPressed() override
        {
            saveSize();
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        juce::PropertiesFile& props;
    };

private:
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (GuitarPracticeStudioApplication)
