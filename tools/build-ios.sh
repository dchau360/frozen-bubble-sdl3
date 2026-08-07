#!/usr/bin/env bash
#
# Builds an UNSIGNED iOS .ipa.
#
# The result installs only on a jailbroken device or through a sideloading tool
# that re-signs it (AltStore, Sideloadly, TrollStore, or `ideviceinstaller` with
# a signature applied first). A stock device will refuse it: iOS verifies the
# code signature at install time, and there is nothing here to verify. Signing
# is a separate step -- see docs/IOS.md.
#
# Usage:
#   tools/build-ios.sh                 # device build (arm64, iphoneos)
#   tools/build-ios.sh --simulator     # simulator build (no .ipa; .app only)
#   tools/build-ios.sh --clean         # discard the build directory first

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SDK="iphoneos"
ARCHS="arm64"
BUILD_DIR="build-ios"
CLEAN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --simulator) SDK="iphonesimulator"; BUILD_DIR="build-ios-sim" ;;
        --clean)     CLEAN=1 ;;
        -h|--help)   sed -n '2,20p' "$0"; exit 0 ;;
        *)           echo "Unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

# The simulator runs on the host CPU, so an Apple Silicon Mac needs an arm64
# simulator slice and an Intel Mac an x86_64 one. Hardcoding arm64 here would
# build something an Intel host cannot launch.
if [ "$SDK" = "iphonesimulator" ]; then
    ARCHS="$(uname -m)"
fi

command -v xcodebuild >/dev/null 2>&1 || {
    echo "error: xcodebuild not found. Install Xcode (the Command Line Tools" >&2
    echo "       alone do not ship the iPhoneOS SDK)." >&2
    exit 1
}

SYSROOT="$(xcrun --sdk "$SDK" --show-sdk-path)"

for sub in SDL3 SDL3_image SDL3_mixer SDL3_ttf; do
    if [ ! -f "android/app/jni/$sub/CMakeLists.txt" ]; then
        echo "error: android/app/jni/$sub is not checked out." >&2
        echo "       git submodule update --init --recursive android/app/jni/SDL3*" >&2
        exit 1
    fi
done

[ "$CLEAN" = "1" ] && rm -rf "$BUILD_DIR"

# Ninja rather than the Xcode generator: this build is deliberately unsigned, and
# Ninja never invokes codesign at all, whereas the Xcode generator has to be
# talked out of it. Bundle layout is identical either way.
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_OSX_SYSROOT="$SYSROOT" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF

cmake --build "$BUILD_DIR" --parallel

APP="$BUILD_DIR/FrozenBubble.app"
[ -d "$APP" ] || { echo "error: $APP was not produced" >&2; exit 1; }

if [ "$SDK" = "iphonesimulator" ]; then
    echo
    echo "Simulator build: $APP"
    echo "Install with:  xcrun simctl install booted $APP"
    exit 0
fi

# An .ipa is a zip whose single top-level entry is Payload/<name>.app.
STAGE="$BUILD_DIR/ipa-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/Payload"
cp -R "$APP" "$STAGE/Payload/"

IPA="$ROOT/$BUILD_DIR/frozen-bubble-sdl3-unsigned.ipa"
rm -f "$IPA"
# -y preserves symlinks rather than following them; SDL and the asset tree have
# none today, but resolving one into a duplicate file would silently bloat the
# archive and break anything that expects a link.
(cd "$STAGE" && zip -qry "$IPA" Payload)

echo
echo "Unsigned IPA: $IPA"
echo "Size: $(du -h "$IPA" | cut -f1)"
echo
echo "This is unsigned and will NOT install on a stock iPhone as-is."
echo "Re-sign it first (Sideloadly / AltStore / codesign with your own"
echo "certificate and provisioning profile) -- see docs/IOS.md."
