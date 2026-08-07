#!/usr/bin/env bash
#
# Compiles the app-icon asset catalog into an already-linked iOS .app bundle.
# Invoked from CMakeLists.txt as a post-build step; not meant to be run by hand.
#
#   ios-appicon.sh <xcassets-dir> <app-bundle-dir> <platform> <min-os> <work-dir>
#
# iOS 11 and later find the icon in a compiled Assets.car, not in loose PNG
# files, and only actool can produce one -- which is why this is a build step
# rather than a handful of images copied into the bundle.

set -euo pipefail

XCASSETS="$1"
APP="$2"
PLATFORM="$3"
MIN_OS="$4"
WORK="$5"

rm -rf "$WORK"
mkdir -p "$WORK"

PARTIAL="$WORK/assetcatalog_generated_info.plist"

xcrun actool "$XCASSETS" \
    --compile "$APP" \
    --platform "$PLATFORM" \
    --minimum-deployment-target "$MIN_OS" \
    --app-icon AppIcon \
    --output-partial-info-plist "$PARTIAL" \
    > "$WORK/actool.log" 2>&1 || {
        echo "actool failed; output follows:" >&2
        cat "$WORK/actool.log" >&2
        exit 1
    }

if [ ! -f "$APP/Assets.car" ]; then
    echo "error: actool produced no Assets.car (see $WORK/actool.log)" >&2
    exit 1
fi

# actool reports which icon variants exist (CFBundleIcons, and a separate
# CFBundleIcons~ipad) in a partial plist. Merging its output is deliberate:
# hardcoding those keys would silently drift the moment the catalog gains or
# drops a size, leaving the plist advertising an icon that is not in Assets.car.
if [ -f "$PARTIAL" ]; then
    /usr/libexec/PlistBuddy -c "Merge $PARTIAL" "$APP/Info.plist"
fi

# Copying the catalog in and then shipping an Info.plist that does not name it
# yields a blank home-screen tile with no error anywhere, so fail loudly instead.
if ! /usr/libexec/PlistBuddy -c "Print :CFBundleIconName" "$APP/Info.plist" >/dev/null 2>&1; then
    echo "error: CFBundleIconName missing from $APP/Info.plist" >&2
    exit 1
fi
