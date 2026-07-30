#!/usr/bin/env python3
"""Serve a WASM build locally with the headers the game needs.

Emscripten's pthread-backed audio needs SharedArrayBuffer, which browsers only
expose to cross-origin-isolated pages. That takes both COOP and COEP, so a plain
`python3 -m http.server` loads the page but comes up silent.

Usage:
    python3 tools/serve-wasm.py [directory] [--port N]

With no directory it looks for ./build-wasm then ./dist-wasm. The two differ:
a local emcmake build writes frozen-bubble-sdl3.html into build-wasm/, while CI
copies that file to dist-wasm/index.html when it packages the release zip. Both
layouts work here — the printed URL points at whichever landing page is present.
"""
import argparse
import http.server
import os
import socketserver
import sys

DEFAULT_DIRS = ("build-wasm", "dist-wasm")
LANDING_PAGES = ("index.html", "frozen-bubble-sdl3.html")


class Handler(http.server.SimpleHTTPRequestHandler):
    directory_override = "."

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=self.directory_override, **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # The .data bundle is tens of MB and changes on every rebuild; caching it
        # is the usual reason a fresh build appears to have no effect.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):
        # Keep the multi-MB asset requests from burying real failures.
        if args and str(args[1]).startswith(("4", "5")):
            super().log_message(fmt, *args)


def resolve_root(explicit):
    if explicit:
        if not os.path.isdir(explicit):
            sys.exit(f"error: {explicit} is not a directory")
        return os.path.abspath(explicit)
    for candidate in DEFAULT_DIRS:
        if os.path.isdir(candidate):
            return os.path.abspath(candidate)
    sys.exit(
        "error: no build found. Looked for "
        + " and ".join(DEFAULT_DIRS)
        + " in the current directory. Build first (see README, \"Building WASM "
        "locally\") or pass the directory explicitly."
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("directory", nargs="?", help="directory to serve")
    ap.add_argument("--port", type=int, default=8080, help="port (default 8080)")
    args = ap.parse_args()

    root = resolve_root(args.directory)
    Handler.directory_override = root
    # Python maps .wasm correctly, but be explicit: serving it as anything else
    # disables streaming compilation and the console error is not obvious.
    Handler.extensions_map[".wasm"] = "application/wasm"

    landing = next((p for p in LANDING_PAGES
                    if os.path.isfile(os.path.join(root, p))), None)
    if landing is None:
        print(f"warning: no {' or '.join(LANDING_PAGES)} in {root}", file=sys.stderr)

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", args.port), Handler) as httpd:
        url = f"http://localhost:{args.port}/{landing or ''}"
        # flush: stdout block-buffers when redirected, and this line is the only
        # confirmation the server came up when it is run in the background.
        print(f"Serving {root}", flush=True)
        print(f"  {url}   (COOP/COEP enabled)", flush=True)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print()


if __name__ == "__main__":
    main()
