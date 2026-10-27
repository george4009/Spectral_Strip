# Sourced by the other scripts. Finds CMake and JUCE and applies the project's JUCE patch.
#   CMAKE      - CMake executable (PATH, Homebrew, or Builds/Dependencies/cmake)
#   LOCAL_JUCE - JUCE source tree ($JUCE_PATH, /Applications/JUCE, or Builds/Dependencies/JUCE)
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
CMAKE=$(command -v cmake || true)
[[ -n "$CMAKE" ]] || CMAKE=/opt/homebrew/bin/cmake
[[ -x "$CMAKE" ]] || CMAKE="$PWD/Builds/Dependencies/cmake/bin/cmake"
[[ -x "$CMAKE" ]] || { print -u2 "CMake not found."; exit 1; }
LOCAL_JUCE="${JUCE_PATH:-/Applications/JUCE}"
[[ -f "$LOCAL_JUCE/CMakeLists.txt" ]] || LOCAL_JUCE="$PWD/Builds/Dependencies/JUCE"
[[ -f "$LOCAL_JUCE/CMakeLists.txt" ]] || { print -u2 "JUCE not found: set JUCE_PATH."; exit 1; }
AU_WRAPPER="$LOCAL_JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm"
if ! grep -q "SpectralStrip: invalidate atomically" "$AU_WRAPPER"; then
  # An older revision of the patch may be applied: revert it first.
  if grep -q "SpectralStrip: list only automatable parameters" "$AU_WRAPPER"; then
    patch -R -p1 -d "$LOCAL_JUCE" < "$PWD/patches/juce-au-automatable-parameter-list-legacy.patch"
  fi
  patch -p1 -d "$LOCAL_JUCE" < "$PWD/patches/juce-au-automatable-parameter-list.patch"
fi
