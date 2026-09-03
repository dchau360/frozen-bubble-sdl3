#!/usr/bin/env bash
#
# Copies share/ into an already-linked iOS .app bundle.
# Invoked from CMakeLists.txt as a post-build step; not meant to be run by hand.
#
#   ios-copy-assets.sh <cmake-binary> <share-dir> <app-bundle-dir>
#
# <app-bundle-dir> is CMake's best guess at configure time (via
# $<TARGET_BUNDLE_CONTENT_DIR:>), and it is correct for Ninja. Under the
# Xcode generator it can't be -- EFFECTIVE_PLATFORM_NAME is chosen per build,
# inside Xcode -- and CMake has no way to hand this script a value that
# resolves later, since VERBATIM escapes every $ in a COMMAND argument before
# it reaches the shell. Xcode always exports BUILT_PRODUCTS_DIR/WRAPPER_NAME
# into this process's own environment, though, regardless of that escaping,
# so prefer those when present and fall back to the argument otherwise.

set -euo pipefail

CMAKE="$1"
SHARE="$2"
APP="$3"
if [ -n "${BUILT_PRODUCTS_DIR:-}" ] && [ -n "${WRAPPER_NAME:-}" ]; then
    APP="$BUILT_PRODUCTS_DIR/$WRAPPER_NAME"
fi

"$CMAKE" -E copy_directory_if_different "$SHARE" "$APP/share"
