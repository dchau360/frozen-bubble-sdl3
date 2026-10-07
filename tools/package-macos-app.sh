#!/usr/bin/env bash
#
# package-macos-app.sh — turn a finished macOS build into a double-clickable
# .app: the executable, the game's art, its Homebrew dylibs, an icon and an
# Info.plist, ad-hoc signed. The same steps the macOS job in
# .github/workflows/build.yml runs inline, but driven by the build directory,
# so it packages whichever game that build is (FB_BRAND in its CMakeCache):
#
#   cmake -B build-boba-app -G Ninja -DCMAKE_BUILD_TYPE=Release \
#     -DFB_BRAND=boba -DFB_BRAND_ASSETS=/path/to/boba-buster-assets
#   cmake --build build-boba-app --parallel
#   tools/package-macos-app.sh build-boba-app            # -> build-boba-app/dist/BobaBuster.app
#   tools/package-macos-app.sh build-boba-app --dmg      # also boba-buster-macos-arm64.dmg
#
# CI's macOS job packages the release DMG with this script too, passing the
# tag's version in FB_APP_VERSION; otherwise the version is the build's
# CMAKE_PROJECT_VERSION, which a release keeps equal to the tag.
#
# Needs dylibbundler (and create-dmg for --dmg) from Homebrew. The art is the
# build's merged brand-share/ when it has one (a brand build laid over share/,
# see cmake/StageBrandAssets.cmake), else this repo's share/; the icon is that
# art's icons/frozen-bubble-icon-1024x1024.png, so a brand's own icon master
# is the app's icon. The app finds its art in Contents/Resources/share
# (InitDataDir in src/platform.cpp).
set -euo pipefail

die() { echo "package-macos-app: $*" >&2; exit 1; }

BUILD=${1:-}
[ -n "$BUILD" ] && [ -f "$BUILD/CMakeCache.txt" ] || die "usage: $0 <build-dir> [--dmg]"
MAKE_DMG=0
[ "${2:-}" = "--dmg" ] && MAKE_DMG=1
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(cd "$BUILD" && pwd)

cache() { sed -n "s/^$1:[A-Z]*=//p" "$BUILD/CMakeCache.txt" | head -1; }
BRAND=$(cache FB_BRAND)
VERSION=${FB_APP_VERSION:-$(cache CMAKE_PROJECT_VERSION)}
[ -n "$VERSION" ] || die "no CMAKE_PROJECT_VERSION in $BUILD/CMakeCache.txt"

# Names per game; kBrandName in src/brand.h is the in-game counterpart.
case "${BRAND:-frozenbubble}" in
    boba)
        NAME="Boba Buster"; BUNDLE="BobaBuster"; EXE="boba-buster"
        BUNDLE_ID="net.llmfinder.bobabuster"; DMG="boba-buster-macos-arm64.dmg" ;;
    frozenbubble)
        NAME="Frozen Bubble"; BUNDLE="FrozenBubble"; EXE="frozen-bubble-sdl3"
        BUNDLE_ID="org.frozenbubble.sdl3"; DMG="frozen-bubble-macos-arm64.dmg" ;;
    *) die "unknown FB_BRAND '$BRAND'" ;;
esac

[ -x "$BUILD/$EXE" ] || die "$BUILD/$EXE not found -- build first"
ART="$BUILD/brand-share"
[ -d "$ART" ] || ART="$ROOT/share"
ICON_SRC="$ART/icons/frozen-bubble-icon-1024x1024.png"
[ -f "$ICON_SRC" ] || die "no icon master at $ICON_SRC"
command -v dylibbundler >/dev/null || die "dylibbundler not installed (brew install dylibbundler)"

DIST="$BUILD/dist"
APP="$DIST/$BUNDLE.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp "$BUILD/$EXE" "$APP/Contents/MacOS/$EXE"
# -L: brand-share is a real copy, but share/ may hold symlinks; the app must
# carry files, not links into a checkout that is not on the player's Mac.
cp -RL "$ART" "$APP/Contents/Resources/share"
find "$APP/Contents/Resources/share" -name .DS_Store -delete

dylibbundler -od -b \
    -x "$APP/Contents/MacOS/$EXE" \
    -d "$APP/Contents/Frameworks" \
    -p @executable_path/../Frameworks/ \
    -s /usr/local/lib \
    -s /opt/homebrew/lib

ICONSET=$(mktemp -d)/"$BUNDLE".iconset
mkdir -p "$ICONSET"
for spec in "16:icon_16x16" "32:icon_16x16@2x" "32:icon_32x32" \
            "64:icon_32x32@2x" "128:icon_128x128" "256:icon_128x128@2x" \
            "256:icon_256x256" "512:icon_256x256@2x" \
            "512:icon_512x512" "1024:icon_512x512@2x"; do
    size="${spec%%:*}"
    sips -z "$size" "$size" "$ICON_SRC" --out "$ICONSET/${spec##*:}.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/$BUNDLE.icns"

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleExecutable</key><string>$EXE</string>
  <key>CFBundleIconFile</key><string>$BUNDLE</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.puzzle-games</string>
  <key>NSHighResolutionCapable</key><true/>
</dict></plist>
EOF

# Ad-hoc: runs on this Mac; another Mac's Gatekeeper still wants a Developer
# ID signature and notarization before it opens without a warning.
find "$APP/Contents/Frameworks" -name "*.dylib" -exec codesign --sign - --force {} \;
codesign --sign - --force --deep "$APP"
echo "wrote $APP"

if [ "$MAKE_DMG" -eq 1 ]; then
    command -v create-dmg >/dev/null || die "create-dmg not installed (brew install create-dmg)"
    rm -f "$BUILD/$DMG"
    STAGE=$(mktemp -d)
    cp -R "$APP" "$STAGE/"
    create-dmg --volname "$NAME" --window-size 540 380 --icon-size 128 \
        --icon "$BUNDLE.app" 140 190 --app-drop-link 400 190 \
        "$BUILD/$DMG" "$STAGE/"
    echo "wrote $BUILD/$DMG"
fi
