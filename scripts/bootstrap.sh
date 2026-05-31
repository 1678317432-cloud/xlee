#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p external tools

if [ ! -d external/JUCE/.git ]; then
  git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git external/JUCE
fi

if [ ! -x tools/cmake-3.31.6-macos-universal/CMake.app/Contents/bin/cmake ]; then
  curl -L -o tools/cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-macos-universal.tar.gz
  tar -xzf tools/cmake.tar.gz -C tools
  rm tools/cmake.tar.gz
fi

echo "Dependencies ready."

