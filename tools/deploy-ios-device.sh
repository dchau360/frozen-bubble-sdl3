#!/usr/bin/env bash
#
# Builds a Debug iOS build signed with your local Apple Development identity
# and installs (+ launches) it on a connected iPhone/iPad, via Xcode-managed
# automatic signing. Companion to tools/build-ios.sh, which produces the
# deliberately-unsigned build CI and everyone else uses -- see docs/IOS.md.
# This one is for the one machine that already has a working personal
# signing identity and just wants the current source on the device.
#
# One-time setup this script assumes:
#   - Full Xcode, with your Apple ID already added (Xcode > Settings >
#     Accounts) so a "Apple Development: ..." identity exists in your
#     keychain (check with: security find-identity -v -p codesigning).
#   - android/app/jni/SDL3* submodules checked out (same as build-ios.sh).
#   - Your device connected and paired with this Mac at least once (Xcode >
#     Window > Devices and Simulators, or just accept the "Trust This
#     Computer?" prompt on the device).
#   - build-ios-device/ already configured with FB_IOS_XCODE_MANAGED_SIGNING
#     and a team selected once, either by a prior run of this script with
#     --team, or by opening build-ios-device/frozen-bubble-sdl3.xcodeproj in
#     Xcode and picking a team in Signing & Capabilities. First run on a new
#     machine needs one of those.
#
# Usage:
#   tools/deploy-ios-device.sh                  # auto-detect device + team
#   tools/deploy-ios-device.sh --device <UDID>   # target a specific device
#   tools/deploy-ios-device.sh --team <TEAMID>   # set/override the dev team
#   tools/deploy-ios-device.sh --no-launch       # install only, don't launch
#   tools/deploy-ios-device.sh --clean           # reconfigure from scratch

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="build-ios-device"
TEAM_FILE="$BUILD_DIR/.devteam"
DEVICE_UDID=""
DEV_TEAM=""
LAUNCH=1
CLEAN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --device)    DEVICE_UDID="$2"; shift ;;
        --team)      DEV_TEAM="$2"; shift ;;
        --no-launch) LAUNCH=0 ;;
        --clean)     CLEAN=1 ;;
        -h|--help)   sed -n '2,29p' "$0"; exit 0 ;;
        *)           echo "Unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

command -v xcodebuild >/dev/null 2>&1 || {
    echo "error: xcodebuild not found. Install full Xcode (the Command Line" >&2
    echo "       Tools alone do not ship the iPhoneOS SDK)." >&2
    exit 1
}

for sub in SDL3 SDL3_image SDL3_mixer SDL3_ttf; do
    if [ ! -f "android/app/jni/$sub/CMakeLists.txt" ]; then
        echo "error: android/app/jni/$sub is not checked out." >&2
        echo "       git submodule update --init --recursive android/app/jni/SDL3*" >&2
        exit 1
    fi
done

# --- Pick the device -------------------------------------------------------
if [ -z "$DEVICE_UDID" ]; then
    # xcrun devicectl list devices prints a table; State is "connected" for a
    # device plugged in / on the network and paired. Identifier is a
    # dash-separated hex UUID, which is what -destination id=... wants.
    # (mapfile/readarray isn't available under macOS's stock bash 3.2, hence
    # the plain while-read loop instead.)
    connected=()
    while IFS= read -r udid; do
        connected+=("$udid")
    done < <(xcrun devicectl list devices 2>/dev/null \
        | awk '$0 ~ /connected/ { for (i=1;i<=NF;i++) if ($i ~ /^[0-9A-Fa-f-]{20,}$/) print $i }')
    if [ "${#connected[@]}" -eq 0 ]; then
        echo "error: no connected device found (xcrun devicectl list devices)." >&2
        echo "       Plug in / unlock your iPhone and trust this Mac if prompted," >&2
        echo "       or pass --device <UDID>." >&2
        exit 1
    elif [ "${#connected[@]}" -gt 1 ]; then
        echo "error: more than one connected device -- pass --device <UDID>:" >&2
        xcrun devicectl list devices
        exit 1
    fi
    DEVICE_UDID="${connected[0]}"
fi
echo "Target device: $DEVICE_UDID"

# --- Configure / reconfigure -------------------------------------------------
# Re-running cmake -B on an existing tree just refreshes the Xcode project's
# file list against current CMakeLists.txt -- needed every time a source file
# has been added or removed since the tree was last configured, and cheap
# otherwise. A fresh tree (or --clean) needs the full option set once.
[ "$CLEAN" = "1" ] && rm -rf "$BUILD_DIR"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    SYSROOT="$(xcrun --sdk iphoneos --show-sdk-path)"
    cmake -B "$BUILD_DIR" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_SYSROOT="$SYSROOT" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DFB_IOS_XCODE_MANAGED_SIGNING=ON \
        -DBUILD_TESTING=OFF
else
    cmake -B "$BUILD_DIR" -S .
fi

# --- Pick the team -----------------------------------------------------------
if [ -z "$DEV_TEAM" ] && [ -f "$TEAM_FILE" ]; then
    DEV_TEAM="$(cat "$TEAM_FILE")"
fi
if [ -z "$DEV_TEAM" ]; then
    # Self-bootstrap from a previously signed build in this tree, if one
    # exists -- avoids asking for --team again on a machine that has already
    # done this once, even if .devteam itself got deleted.
    PREV_APP="$BUILD_DIR/Debug-iphoneos/FrozenBubble.app"
    if [ -d "$PREV_APP" ]; then
        DEV_TEAM="$(codesign -dvvv "$PREV_APP" 2>&1 | sed -n 's/^TeamIdentifier=//p')"
    fi
fi
if [ -z "$DEV_TEAM" ] || [ "$DEV_TEAM" = "not set" ]; then
    echo "error: no development team known yet." >&2
    echo "       Pass --team <TEAMID> once (find it at" >&2
    echo "       https://developer.apple.com/account under Membership, or in" >&2
    echo "       Xcode > Settings > Accounts > your Apple ID > team list)," >&2
    echo "       or open $BUILD_DIR/frozen-bubble-sdl3.xcodeproj in Xcode and" >&2
    echo "       pick a team in the target's Signing & Capabilities pane." >&2
    exit 1
fi
echo "$DEV_TEAM" > "$TEAM_FILE"
echo "Development team: $DEV_TEAM"

# --- Build + sign --------------------------------------------------------
xcodebuild -project "$BUILD_DIR/frozen-bubble-sdl3.xcodeproj" \
    -target frozen-bubble-sdl3 -configuration Debug \
    -destination "id=$DEVICE_UDID" \
    -allowProvisioningUpdates \
    DEVELOPMENT_TEAM="$DEV_TEAM" \
    build

APP="$BUILD_DIR/Debug-iphoneos/FrozenBubble.app"
[ -d "$APP" ] || { echo "error: $APP was not produced" >&2; exit 1; }

VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$APP/Info.plist" 2>/dev/null || echo "?")"
echo
echo "Built and signed FrozenBubble v$VERSION"

# --- Install (+ launch) -----------------------------------------------------
xcrun devicectl device install app --device "$DEVICE_UDID" "$APP"

if [ "$LAUNCH" = "1" ]; then
    xcrun devicectl device process launch --device "$DEVICE_UDID" org.frozenbubble.sdl3
fi

echo
echo "Installed FrozenBubble v$VERSION on device $DEVICE_UDID"
