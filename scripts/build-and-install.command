#!/bin/zsh
# Configures, builds the Release AU/VST3 and the tests, runs the tests, creates the .pkg,
# installs into the user plug-in folders (backing up the previous copy) and runs auval.
# Logs and progress ("status.txt") go to Builds/logs/<date-time>/.
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/juce-setup.sh
LOG="$PWD/Builds/logs/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$LOG"
trap 'print "FAILED at line $LINENO" > "$LOG/status.txt"; print -u2 "FAILED - see $LOG"' ERR
step() { print "$1" > "$LOG/status.txt"; print "== $1"; }

step configure
"$CMAKE" -S . -B Builds -G Xcode -DJUCE_PATH="$LOCAL_JUCE" > "$LOG/configure.log" 2>&1
step build
xcodebuild -project Builds/SpectralStrip.xcodeproj -target SpectralStrip_AU -target SpectralStrip_VST3 \
    -target SpectralStripTests -configuration Release -jobs 6 > "$LOG/build.log" 2>&1
step test
Builds/SpectralStripTests_artefacts/Release/SpectralStripTests > "$LOG/test.log" 2>&1
step package
scripts/build-installer.command > "$LOG/package.log" 2>&1

step install
COMPONENTS="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
BACKUP="$PWD/Backups/before-install-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$COMPONENTS" "$VST3" "$BACKUP"
[[ -d "$COMPONENTS/Spectral Strip.component" ]] && ditto "$COMPONENTS/Spectral Strip.component" "$BACKUP/Spectral Strip.component"
[[ -d "$VST3/Spectral Strip.vst3" ]] && ditto "$VST3/Spectral Strip.vst3" "$BACKUP/Spectral Strip.vst3"
rm -rf "$COMPONENTS/Spectral Strip.component" "$VST3/Spectral Strip.vst3"
ditto 'Builds/SpectralStrip_artefacts/Release/AU/Spectral Strip.component' "$COMPONENTS/Spectral Strip.component"
ditto 'Builds/SpectralStrip_artefacts/Release/VST3/Spectral Strip.vst3' "$VST3/Spectral Strip.vst3"
{
    codesign --force --sign - "$COMPONENTS/Spectral Strip.component" "$VST3/Spectral Strip.vst3"
    codesign --verify --strict "$COMPONENTS/Spectral Strip.component" "$VST3/Spectral Strip.vst3"
    /usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$COMPONENTS/Spectral Strip.component/Contents/Info.plist"
    print "Backup: $BACKUP"
} > "$LOG/install.log" 2>&1

step auval
# Make the system rescan Audio Units, then validate.
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -v aufx SpSt GiAu > "$LOG/auval.log" 2>&1 || true
trap - ERR
print DONE > "$LOG/status.txt"
print "DONE - logs in $LOG"
