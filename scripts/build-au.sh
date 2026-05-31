#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE="$ROOT_DIR/tools/cmake-3.31.6-macos-universal/CMake.app/Contents/bin/cmake"

if [ ! -x "$CMAKE" ] || [ ! -d "$ROOT_DIR/external/JUCE" ]; then
  "$ROOT_DIR/scripts/bootstrap.sh"
fi

"$CMAKE" -S "$ROOT_DIR/source/SuperBass" -B "$ROOT_DIR/build/SuperBass" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64

"$CMAKE" --build "$ROOT_DIR/build/SuperBass" --config Release --parallel 4

xattr -cr "$HOME/Library/Audio/Plug-Ins/Components/Super Bass.component"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/Components/Super Bass.component"
auval -v aufx SuBs Xlee
