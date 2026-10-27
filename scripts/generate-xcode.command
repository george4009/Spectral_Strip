#!/bin/zsh
# Generates Builds/SpectralStrip.xcodeproj and opens it.
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/juce-setup.sh
"$CMAKE" -S . -B Builds -G Xcode -DJUCE_PATH="$LOCAL_JUCE"
open Builds/SpectralStrip.xcodeproj
