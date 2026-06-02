#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE="$ROOT_DIR/tools/cmake-3.31.6-macos-universal/CMake.app/Contents/bin/cmake"
BUILD_DIR="$ROOT_DIR/build/SuperBass"

"$ROOT_DIR/scripts/build-au.sh"

VERSION="$(awk '/^[[:space:]]*project[[:space:]]*[(]/ { for (i = 1; i <= NF; ++i) if ($i == "VERSION") { print $(i + 1); exit } }' "$ROOT_DIR/source/SuperBass/CMakeLists.txt" | tr -d ')')"

if [ -z "${VERSION:-}" ]; then
  VERSION="$("$CMAKE" -LA "$BUILD_DIR" 2>/dev/null | sed -n 's/^CMAKE_PROJECT_VERSION:[^=]*=//p' | head -1)"
fi

if [ -z "${VERSION:-}" ]; then
  VERSION="0.1.0"
fi

RELEASE_DIR="$ROOT_DIR/build/release/SuperBass-$VERSION-macOS"
ROOT_STAGE="$RELEASE_DIR/root"
PKG_PATH="$RELEASE_DIR/SuperBass-$VERSION-macOS.pkg"
DMG_PATH="$HOME/Downloads/SuperBass-$VERSION-macOS.dmg"

AU_SRC="$BUILD_DIR/SuperBass_artefacts/Release/AU/Super Bass.component"
VST3_SRC="$BUILD_DIR/SuperBass_artefacts/Release/VST3/Super Bass.vst3"

if [ ! -d "$AU_SRC" ]; then
  echo "Missing AU build at: $AU_SRC" >&2
  exit 1
fi

if [ ! -d "$VST3_SRC" ]; then
  echo "Missing VST3 build at: $VST3_SRC" >&2
  exit 1
fi

rm -rf "$RELEASE_DIR"
mkdir -p "$ROOT_STAGE/Library/Audio/Plug-Ins/Components"
mkdir -p "$ROOT_STAGE/Library/Audio/Plug-Ins/VST3"

ditto "$AU_SRC" "$ROOT_STAGE/Library/Audio/Plug-Ins/Components/Super Bass.component"
ditto "$VST3_SRC" "$ROOT_STAGE/Library/Audio/Plug-Ins/VST3/Super Bass.vst3"

xattr -cr "$ROOT_STAGE/Library/Audio/Plug-Ins/Components/Super Bass.component"
xattr -cr "$ROOT_STAGE/Library/Audio/Plug-Ins/VST3/Super Bass.vst3"
codesign --force --deep --sign - "$ROOT_STAGE/Library/Audio/Plug-Ins/Components/Super Bass.component"
codesign --force --deep --sign - "$ROOT_STAGE/Library/Audio/Plug-Ins/VST3/Super Bass.vst3"

pkgbuild \
  --root "$ROOT_STAGE" \
  --identifier "com.xleeaudio.superbass.pkg" \
  --version "$VERSION" \
  --install-location "/" \
  "$PKG_PATH"

rm -f "$DMG_PATH"
hdiutil create \
  -volname "Super Bass $VERSION" \
  -srcfolder "$PKG_PATH" \
  -ov \
  -format UDZO \
  "$DMG_PATH"

echo "macOS release DMG exported to: $DMG_PATH"
