#!/bin/zsh
# Creates "Spectral Strip <version>.pkg" from the Release AU and VST3 builds.
# The package installs into /Library/Audio/Plug-Ins. Signed ad hoc, not notarised.
set -euo pipefail
cd "$(dirname "$0")/.."
AU='Builds/SpectralStrip_artefacts/Release/AU/Spectral Strip.component'
VST='Builds/SpectralStrip_artefacts/Release/VST3/Spectral Strip.vst3'
VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$AU/Contents/Info.plist")
VST_VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$VST/Contents/Info.plist")
[[ "$VERSION" == "$VST_VERSION" ]] || { print -u2 'AU and VST3 versions differ. Rebuild both Release targets.'; exit 1; }
STAGE=$(mktemp -d "$PWD/Builds/pkg-staging.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT
ROOT="$STAGE/root/Library/Audio/Plug-Ins"
mkdir -p "$ROOT/Components" "$ROOT/VST3"
ditto --norsrc --noextattr --noacl "$AU" "$ROOT/Components/Spectral Strip.component"
ditto --norsrc --noextattr --noacl "$VST" "$ROOT/VST3/Spectral Strip.vst3"
for BUNDLE in "$ROOT/Components/Spectral Strip.component" "$ROOT/VST3/Spectral Strip.vst3"; do
    codesign --force --sign - "$BUNDLE"
    codesign --verify --strict "$BUNDLE"
done
# Installed files are owned by root; every user's DAW must be able to read them.
find "$STAGE/root" -type d -exec chmod 755 {} +
find "$STAGE/root" -type f -exec chmod 644 {} +
chmod 755 "$ROOT/Components/Spectral Strip.component/Contents/MacOS/Spectral Strip" \
          "$ROOT/VST3/Spectral Strip.vst3/Contents/MacOS/Spectral Strip"
pkgbuild --analyze --root "$STAGE/root" "$STAGE/components.plist"
/usr/bin/plutil -replace 0.BundleIsRelocatable -bool NO "$STAGE/components.plist"
/usr/bin/plutil -replace 1.BundleIsRelocatable -bool NO "$STAGE/components.plist"
pkgbuild --root "$STAGE/root" --component-plist "$STAGE/components.plist" \
    --identifier audio.giorgio.spectralstrip.pkg --version "$VERSION" \
    --ownership recommended --install-location / "Spectral Strip $VERSION.pkg"
print "Created: $PWD/Spectral Strip $VERSION.pkg (ad hoc signature, not notarised)"
