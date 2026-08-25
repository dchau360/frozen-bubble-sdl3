#!/usr/bin/env python3
"""Render the published site from its single source in the repo.

The privacy policy used to exist as three hand-synced copies: this repo's
docs/PRIVACY_POLICY.md, plus an index.md and a hand-written index.html on the
gh-pages branch. Only the last of those was ever served, so the published
policy silently fell behind the source -- it was still describing the app
before the two ad-removal products existed, which is exactly the sort of thing
a store review reads. Generating it removes the chance to forget.

Usage: tools/build-site.py <output-dir>
"""
import os
import shutil
import sys

import markdown

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SITE = os.path.join(ROOT, "site")

# (source markdown, output path, <title>)
#
# The policy lives at /privacy/ rather than at the root so the root can be an
# actual landing page: the store listing's "website" link pointing at a legal
# document is a poor landing for someone who just wants to know what the game
# is. Verification is tied to the URL prefix, not to any one page, so moving
# the policy does not affect it.
PAGES = [
    (os.path.join(SITE, "index.md"),
     "index.html",
     "Frozen Bubble: SDL3"),
    (os.path.join(ROOT, "docs", "PRIVACY_POLICY.md"),
     os.path.join("privacy", "index.html"),
     "Privacy Policy — Frozen Bubble: SDL3"),
]

# (source path, path within the site)
ASSETS = [
    (os.path.join(ROOT, "docs", "screenshots", "game-room.png"),
     os.path.join("screenshots", "game-room.png")),
]

# Copied through verbatim. The Google verification token must keep its exact
# bytes and filename or the site stops being verified.
VERBATIM = ["google034c4b2cf8d147df.html"]


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: build-site.py <output-dir>")
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)

    template = open(os.path.join(SITE, "template.html"), encoding="utf-8").read()

    for src, dest, title in PAGES:
        text = open(src, encoding="utf-8").read()
        # The first heading becomes the page's <h1>, which the template does
        # not supply -- so the markdown's own "# ..." line is kept, not stripped.
        body = markdown.markdown(text, extensions=["extra", "sane_lists"])
        # Pages one level down need their relative links to the site root
        # rewritten; the template is shared and written from the root's view.
        depth = len(os.path.dirname(dest).split(os.sep)) if os.path.dirname(dest) else 0
        html = (template
                .replace("{{TITLE}}", title)
                .replace("{{ROOT}}", "../" * depth if depth else "./")
                .replace("{{CONTENT}}", body))
        dest_path = os.path.join(out, dest)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        with open(dest_path, "w", encoding="utf-8") as f:
            f.write(html)
        print("rendered %s -> %s (%d bytes)" % (
            os.path.relpath(src, ROOT), dest, len(html)))

    for src, dest in ASSETS:
        dest_path = os.path.join(out, dest)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        shutil.copyfile(src, dest_path)
        print("copied   %s -> %s" % (os.path.relpath(src, ROOT), dest))

    for name in VERBATIM:
        shutil.copyfile(os.path.join(SITE, name), os.path.join(out, name))
        print("copied   %s" % name)


if __name__ == "__main__":
    main()
