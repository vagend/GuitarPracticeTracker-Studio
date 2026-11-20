# Guitar Practice Studio

A desktop practice app for guitar, for Windows and Mac. It times and logs your practice, runs a
metronome and tempo ladder, shows tab, tunes, trains the fretboard, and plays your guitar through your
own amp-sim and effect plugins (VST3, and AudioUnit on Mac).

Studio grew out of [Guitar Practice Tracker](https://github.com/vagend/GuitarPracticeTracker) and is now
its own project, maintained here separately. Its screens (in `web/`) are shown in a native window built
with [JUCE](https://juce.com), which does the audio and the plugin hosting.

## Download

Get the newest build from [Releases](https://github.com/vagend/GuitarPracticeTracker-Studio/releases/latest):

- **Windows:** `GuitarPracticeStudio-setup.exe` (installs just for you, no admin needed).
- **Mac:** `GuitarPracticeStudio-mac.dmg` (Apple Silicon and Intel). Open it and drag the app onto
  Applications. The first time, macOS can't verify it: open **System Settings → Privacy & Security**
  and click **Open Anyway**.

After installing, Studio offers new builds itself (Settings → Studio → Check for updates).

## What it does

- **Practice and log:** pick an exercise, time it with a stopwatch or countdown, and log the minutes,
  tempo and how it felt. Stats show your progress per exercise and per day.
- **Metronome:** Basic, Backbeat or your own pattern per exercise, with subdivisions and count-ins.
  Turn it off per exercise for jamming.
- **Tempo ladder:** steps from 70% up to 110% of your goal and back down to 90%, repeating the exercise
  at each tempo. Stay on a step, skip, or start from any step.
- **Tab:** give an exercise a Guitar Pro, MusicXML or Capella file, or type one. It opens in its own
  window; select bars to practise just those. With **Use tab** on, the tab sets the goal tempo, the
  ladder's length and the metronome's time signature.
- **Your plugins:** each exercise has an effect slot that remembers its plugin and settings. Studio
  plays your guitar through your audio interface (ASIO on Windows); **Mute input** turns the input off.
- **Tuner and Fretboard Trainer,** both listening through your audio interface.
- **Color schemes,** including Brass (the default). Every window and list follows the scheme.

If your audio interface is switched off or unplugged, Studio turns audio off rather than falling back
to the computer's microphone and speakers, and reconnects when the interface is back.

No account, no ads, no tracking: your practice log stays on your computer, and Studio itself only goes
online to check for updates (you can turn that off). Settings → Privacy lists everything Studio uses,
including what's outside its control: the plugins you load, and Windows voices marked "Online".

## Test builds

New features are tried out on the `test` branch first. Each push there builds **Guitar Practice
Studio Test**, a separate app with its own practice log, exercises and settings that installs next to
the normal one. It's published as the `test` pre-release, and only the test app updates from it. The
normal app only ever updates from the latest release, which is built from `main` when a change is
ready.

## Building it yourself on a Mac

1. Install Xcode from the App Store and open it once.
2. Double-click **Build Studio (Mac).command**. The first run downloads JUCE (and CMake if
   needed) into `external/` and takes a few minutes. It then opens the app from
   `build/GuitarPracticeStudio_artefacts/Release/`.
3. If something fails, the details are in `build.log`.

## Licence

Guitar Practice Studio is free software under the [GNU AGPLv3](LICENSE.md): anyone can use, change
and share it, and shared changes must stay free under the same licence.

It's built with [JUCE](https://juce.com), used under the AGPLv3. On Windows it includes Steinberg's
ASIO interface, used under the GPLv3. The VST3 SDK is MIT-licensed. Tab is drawn by
[alphaTab](https://github.com/CoderLine/alphaTab) (MPL-2.0), included unmodified in `web/vendor/alphatab`.
