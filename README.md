# Guitar Practice Studio (experimental)

A desktop version of [Guitar Practice Tracker](https://github.com/vagend/GuitarPracticeTracker)
that can also play your guitar through your own amp-sim and effect plugins (VST3 and AudioUnit).

Studio is its own project. Its screens (in `web/`) started as a copy of the Tracker's and are
now maintained here separately; they're shown in a native window built with
[JUCE](https://juce.com), which does the audio and the plugin hosting.

## Download

Get the newest build from [Releases](https://github.com/vagend/GuitarPracticeTracker-Studio/releases/latest):

- **Windows:** `GuitarPracticeStudio-setup.exe` (installs just for you, no admin needed).
- **Mac:** `GuitarPracticeStudio-mac.dmg` (Apple Silicon and Intel). Open it and drag the app onto
  Applications. The first time, macOS can't verify it: open **System Settings → Privacy & Security**
  and click **Open Anyway**.

GitHub builds both every time `main` changes. After installing, Studio offers new builds itself
(Settings → Studio → Check for updates).

## Status: 0.4

- The full practice app runs inside the window.
- **Effect slot** at the bottom of the Exercise card: click the plugin name to open the plugin,
  or ▾ to pick another (search, recent plugins, Remove plugin). Each exercise remembers its
  plugin and its settings; only one plugin runs at a time. An empty slot plays the raw guitar.
- **Mute** silences Studio's output (red while muted). **Input** shows your input level.
- **Settings → Audio:** Audio interface… (device, inputs, outputs; ASIO on Windows) and Scan for plugins….
- Each exercise has a **Metronome** setting; turn it off for jamming.
- The **Fretboard Trainer** listens through Studio's audio interface.
- Per-exercise plugin settings are kept in `exercise-plugins.json` in Studio's data folder
  (~/Library/Application Support/Guitar Practice Studio on a Mac, %APPDATA%\Guitar Practice Studio on Windows).

Not yet: a chain of several plugins.

## Building it yourself on a Mac

1. Install Xcode from the App Store and open it once.
2. Double-click **Build Studio (Mac).command**. The first run downloads JUCE (and CMake if
   needed) into `external/` and takes a few minutes. It then opens the app from
   `build/GuitarPracticeStudio_artefacts/Release/`.
3. If something fails, the details are in `build.log`.

## Licences

JUCE is used under its free Starter licence (JUCE 9 EULA). The VST3 SDK is MIT-licensed.
