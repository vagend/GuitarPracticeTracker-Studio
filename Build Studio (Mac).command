#!/bin/bash
# Builds Guitar Practice Studio and opens it.
# First run: downloads JUCE (and CMake if you don't have it) into the "external" folder,
# which is not kept in git. Needs Xcode installed and opened once.
set -euo pipefail
cd "$(dirname "$0")"

JUCE_VERSION="9.0.5"
CMAKE_VERSION="4.4.5"
CONFIG="${1:-Release}"

say() { printf '\n== %s\n' "$*"; }
fail() { printf '\n!! %s\n' "$*"; read -r -p "Press Return to close." _; exit 1; }

if ! xcode-select -p >/dev/null 2>&1 || ! xcodebuild -version >/dev/null 2>&1; then
  fail "Xcode isn't set up yet. Install Xcode from the App Store, open it once, then run this again."
fi

mkdir -p external

if [ ! -f external/JUCE/CMakeLists.txt ]; then
  say "Downloading JUCE $JUCE_VERSION (one time)..."
  rm -rf external/JUCE "external/JUCE-$JUCE_VERSION"
  curl -fL --retry 3 "https://github.com/juce-framework/JUCE/archive/refs/tags/$JUCE_VERSION.tar.gz" \
    | tar xz -C external || fail "Couldn't download JUCE."
  mv "external/JUCE-$JUCE_VERSION" external/JUCE
fi

if command -v cmake >/dev/null 2>&1; then
  CMAKE="cmake"
else
  CMAKE="external/cmake/CMake.app/Contents/bin/cmake"
  if [ ! -x "$CMAKE" ]; then
    say "Downloading CMake $CMAKE_VERSION (one time)..."
    rm -rf external/cmake "external/cmake-$CMAKE_VERSION-macos-universal"
    curl -fL --retry 3 "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-macos-universal.tar.gz" \
      | tar xz -C external || fail "Couldn't download CMake."
    mv "external/cmake-$CMAKE_VERSION-macos-universal" external/cmake
  fi
fi

say "Setting up the Xcode project..."
"$CMAKE" -S . -B build -G Xcode >build.log 2>&1 || { tail -40 build.log; fail "Setup failed. The details are in build.log."; }

say "Building ($CONFIG). The first build takes a few minutes..."
"$CMAKE" --build build --config "$CONFIG" >>build.log 2>&1 || { grep -E "error:|Error " build.log | head -40; fail "Build failed. The details are in build.log."; }

APP="build/GuitarPracticeStudio_artefacts/$CONFIG/Guitar Practice Studio.app"
[ -d "$APP" ] || fail "Build finished but the app wasn't where expected: $APP"

say "Done. Opening Guitar Practice Studio."
open "$APP"
