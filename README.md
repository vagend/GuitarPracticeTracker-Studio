# Guitar Practice Studio (experimental)

A desktop version of [Guitar Practice Tracker](https://github.com/vagend/GuitarPracticeTracker)
that can also play your guitar through your own amp-sim and effect plugins (VST3 and AudioUnit).

Studio is its own project. Its screens (in `web/`) started as a copy of the Tracker's and are
now maintained here separately; they're shown in a native window built with
[JUCE](https://juce.com), which does the audio and the plugin hosting.

## Status: 0.2

- The full practice app runs inside the window.
- **Effect slot** at the bottom of the Exercise card: click the plugin name to open the plugin,
  or ▾ to pick another (search, recent plugins, Remove plugin). Each exercise remembers its
  plugin and its settings; only one plugin runs at a time. An empty slot plays the raw guitar.
- **Mute** silences Studio's output (red while muted). **Input** shows your input level.
- **Settings → Audio:** Audio interface… (device, inputs, outputs) and Scan for plugins….
- Per-exercise plugin settings are kept in `exercise-plugins.json` in
  ~/Library/Application Support/Guitar Practice Studio.

Not yet: a chain of several plugins, and native tuner/trainer input.

## Building on a Mac

1. Install Xcode from the App Store and open it once.
2. Double-click **Build Studio (Mac).command**. The first run downloads JUCE (and CMake if
   needed) into `external/` and takes a few minutes. It then opens the app from
   `build/GuitarPracticeStudio_artefacts/Release/`.
3. If something fails, the details are in `build.log`.

## Licences

JUCE is used under its free Starter licence (JUCE 9 EULA). The VST3 SDK is MIT-licensed.
