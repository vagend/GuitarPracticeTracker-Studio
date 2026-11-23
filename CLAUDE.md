# Guitar Practice Studio — notes for Claude Code

## What this is
- Desktop version of Guitar Practice Tracker (separate repo: vagend/GuitarPracticeTracker). Studio is its own app.
- C++ with JUCE (Source/Main.cpp, Source/AudioEngine.h): window, audio interface (ASIO on Windows), VST3/AU plugin hosting.
- The screens are HTML/JS in web/, shown in JUCE's web view (WebView2 on Windows).
- Built with CMake. GitHub Actions (.github/workflows/release.yml) builds the Windows installer and the Mac .dmg on every push to main; the app updates itself.
- Licence: AGPLv3 (JUCE used under AGPLv3). Studio must stay free and fully functional for free: no paid tools or sounds required.
- Must work on both Mac and Windows.

## Hard requirements
- Every sound (guitar through the plugin, metronome, tab playback) goes out the SAME audio interface Studio uses, on ONE clock, so everything stays in sync.
- Rejected ideas, don't reintroduce: MIDI out to an external drum app; MIDI into a guitar/amp VST to "play" tabs (it doesn't sound like a real guitar through an amp sim).

## Current state (as of Nov 23, 2026)
- The metronome (clicks, drum sounds, user-uploaded samples) is played by the web page with Web Audio (AudioContext in web/index.html), NOT by the JUCE engine. So it goes to the system default output, not necessarily the interface.
- Tab feature (alphaTab 1.9.0, MPL-2.0, vendored in web/vendor/alphatab) is on the `test` branch: web/tab.html draws the tab and gets the metronome position from Studio. Playback is off (`enablePlayer:false`). The free SoundFont (web/vendor/alphatab/soundfont/sonivox.sf2) is already in the repo, unused.
- Keep alphaTab's "alphaTab.net" attribution in rendered scores.

## Plan, in order
1. Move the metronome into the C++ audio engine: built-in sounds, user samples, patterns, triplets, speed curve. The page only sends commands (start/stop, BPM, pattern) and receives the beat position. The JUCE engine owns the clock.
2. Add tab playback: export the tab from alphaTab as MIDI, play it in C++ with the bundled SoundFont (e.g. TinySoundFont, the engine alphaTab's synth is based on), synced to the metronome clock, out the same interface. alphaTab only draws the tab and moves the cursor.

## Working rules
- The user verifies by playing guitar through the real interface; say what you could and couldn't test.
- Tell the user when you make an assumption they didn't ask for.
- Free tab editing for users without Guitar Pro: TuxGuitar (separate app, LGPL); it can't be embedded in Studio.
