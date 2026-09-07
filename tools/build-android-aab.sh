#!/usr/bin/env bash
#
# Builds a signed release .aab (Android App Bundle, for Play Console upload)
# entirely locally and copies it to ~/Downloads. Nothing here touches git,
# CI, or any remote host -- the signing key and its passwords never leave
# this machine, unlike the CI workflow which needs them in repository
# secrets to produce a public release.
#
# Signing credentials come from, in order:
#   1. android/keystore.properties (gitignored -- see docs/ANDROID_SIGNING.md)
#      storeFile=/absolute/or/repo-relative/path/to/release.keystore
#      storePassword=...
#      keyAlias=frozenbubble
#      keyPassword=...
#   2. Environment variables KEYSTORE_PATH / KEYSTORE_PASSWORD / KEY_ALIAS /
#      KEY_PASSWORD (matches the names the CI workflow uses).
#   3. An interactive prompt (passwords read with echo off, never logged).
#
# Usage:
#   tools/build-android-aab.sh
#   tools/build-android-aab.sh --dest ~/Desktop   # copy somewhere other than Downloads

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DEST="$HOME/Downloads"

while [ $# -gt 0 ]; do
    case "$1" in
        --dest) DEST="$2"; shift ;;
        -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

PROPS_FILE="android/keystore.properties"

# Read a key=value pair out of keystore.properties without sourcing it (the
# file is untrusted input as far as shell injection goes, however unlikely).
# A value quoted like a shell variable ('pw' or "pw") has the quotes stripped,
# since unlike a real .properties parser, plain sed takes them as literal
# characters -- and a literal quote in the password is what actually broke
# signing the first time this ran.
prop() {
    [ -f "$PROPS_FILE" ] || return 0
    sed -n "s/^$1=//p" "$PROPS_FILE" | tail -n1 | sed -E "s/^'(.*)'\$/\1/; s/^\"(.*)\"\$/\1/"
}

KEYSTORE_PATH="${KEYSTORE_PATH:-$(prop storeFile)}"
KEYSTORE_PASSWORD="${KEYSTORE_PASSWORD:-$(prop storePassword)}"
KEY_ALIAS="${KEY_ALIAS:-$(prop keyAlias)}"
KEY_PASSWORD="${KEY_PASSWORD:-$(prop keyPassword)}"

if [ -z "$KEYSTORE_PATH" ]; then
    read -r -p "Path to release keystore: " KEYSTORE_PATH
fi
# A path from keystore.properties is conventionally relative to android/;
# a path typed at the prompt just now is more likely relative to $ROOT.
# Try both rather than guessing wrong and failing later with a confusing
# "keystore not found" from Gradle.
if [ ! -f "$KEYSTORE_PATH" ] && [ -f "android/$KEYSTORE_PATH" ]; then
    KEYSTORE_PATH="android/$KEYSTORE_PATH"
fi
if [ ! -f "$KEYSTORE_PATH" ]; then
    echo "error: keystore not found at '$KEYSTORE_PATH'" >&2
    exit 1
fi
KEYSTORE_PATH="$(cd "$(dirname "$KEYSTORE_PATH")" && pwd)/$(basename "$KEYSTORE_PATH")"

if [ -z "$KEYSTORE_PASSWORD" ]; then
    read -r -s -p "Keystore password: " KEYSTORE_PASSWORD; echo
fi
if [ -z "$KEY_ALIAS" ]; then
    read -r -p "Key alias [frozenbubble]: " KEY_ALIAS
    KEY_ALIAS="${KEY_ALIAS:-frozenbubble}"
fi
if [ -z "$KEY_PASSWORD" ]; then
    read -r -s -p "Key password [same as keystore password]: " KEY_PASSWORD; echo
    KEY_PASSWORD="${KEY_PASSWORD:-$KEYSTORE_PASSWORD}"
fi

echo "Building release .aab (this recompiles the native game for Android; the first run is slow)..."
cd android
chmod +x gradlew
./gradlew bundleRelease --no-daemon \
    -Pandroid.injected.signing.store.file="$KEYSTORE_PATH" \
    -Pandroid.injected.signing.store.password="$KEYSTORE_PASSWORD" \
    -Pandroid.injected.signing.key.alias="$KEY_ALIAS" \
    -Pandroid.injected.signing.key.password="$KEY_PASSWORD"
cd "$ROOT"

OUT="android/app/build/outputs/bundle/release/app-release.aab"
if [ ! -f "$OUT" ]; then
    echo "error: no bundle produced at $OUT" >&2
    exit 1
fi

VERSION_NAME="$(sed -n 's/^def appVersionName = "\(.*\)"/\1/p' android/app/build.gradle)"
VERSION_CODE="$(sed -n 's/^[[:space:]]*versionCode \([0-9]*\)/\1/p' android/app/build.gradle)"
STAMP="frozen-bubble-v${VERSION_NAME:-unknown}-code${VERSION_CODE:-0}.aab"

mkdir -p "$DEST"
cp "$OUT" "$DEST/$STAMP"

echo
echo "Signed by: $KEY_ALIAS"
jarsigner -verify "$DEST/$STAMP" >/dev/null 2>&1 \
    && echo "Signature verified." \
    || echo "warning: could not verify signature with jarsigner (check manually before uploading)."
echo "Saved to: $DEST/$STAMP"
ls -lh "$DEST/$STAMP"
