#!/bin/bash
# Builds Guitar Practice Studio Test on this Mac (Apple Silicon only), puts it in Applications and opens it.
# The Test app is separate from the real one: its own name, practice log and settings.
# After the first build, only what changed is rebuilt, so later builds are quick.
# A Test app built here doesn't update itself from GitHub; run this again to get your latest changes.
# Needs JUCE in "external" (Build Studio (Mac).command downloads it) and Xcode installed.
set -euo pipefail
cd "$(dirname "$0")"

NAME="Guitar Practice Studio Test"
BUILD="build-test"
LOG="build-test.log"
DEST="/Applications/$NAME.app"

say() { printf '\n== %s\n' "$*"; }
fail() { printf '\n!! %s\n' "$*"; [ -t 0 ] && read -r -p "Press Return to close." _; exit 1; }

[ -f external/JUCE/CMakeLists.txt ] || fail "JUCE isn't downloaded yet. Run 'Build Studio (Mac).command' once first."
if command -v cmake >/dev/null 2>&1; then CMAKE="cmake"; else CMAKE="external/cmake/CMake.app/Contents/bin/cmake"; fi
[ -x "$(command -v "$CMAKE")" ] || fail "CMake not found. Run 'Build Studio (Mac).command' once first."

say "Setting up..."
"$CMAKE" -S . -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DSTUDIO_CHANNEL=test \
  -DCMAKE_OSX_ARCHITECTURES=arm64 >"$LOG" 2>&1 || { tail -40 "$LOG"; fail "Setup failed. The details are in $LOG."; }

say "Building. The first build takes a few minutes; after that only changes are rebuilt..."
"$CMAKE" --build "$BUILD" --parallel "$(sysctl -n hw.ncpu)" >>"$LOG" 2>&1 \
  || { grep -E "error:" "$LOG" | head -40; fail "Build failed. The details are in $LOG."; }

APP="$BUILD/GuitarPracticeStudio_artefacts/Release/$NAME.app"
[ -d "$APP" ] || fail "Build finished but the app wasn't where expected: $APP"
# The build copies web/ into the app only when the C++ changed, so copy it every time here:
# a change to only the pages (web/) would otherwise not reach the app.
rsync -a --delete web/ "$APP/Contents/Resources/web/"

# Quit the Test app if it's open, so it saves and its files can be replaced.
if pgrep -x "$NAME" >/dev/null; then
  say "Closing $NAME..."
  osascript -e "quit app \"$NAME\"" >/dev/null 2>&1 || true
  for _ in $(seq 1 20); do pgrep -x "$NAME" >/dev/null || break; sleep 0.5; done
  pgrep -x "$NAME" >/dev/null && fail "$NAME didn't close. Quit it, then run this again."
fi

say "Putting it in Applications..."
mkdir -p "$DEST"
rsync -a --delete "$APP/" "$DEST/"
# when it was built: the app shows "TEST built here <date>" at the top, next to the version
date '+%b %-d, %-I:%M %p' > "$DEST/Contents/Resources/web/local-build.txt"
# ad-hoc signature, as on GitHub (macOS may ask again for microphone access after a rebuild)
codesign --force --deep --sign - "$DEST" >>"$LOG" 2>&1 || fail "Couldn't sign the app. The details are in $LOG."

say "Done. Opening $NAME."
open "$DEST"
