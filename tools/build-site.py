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

# (source markdown, output filename, <title>)
PAGES = [
    (os.path.join(ROOT, "docs", "PRIVACY_POLICY.md"),
     "index.html",
     "Privacy Policy — Frozen Bubble: SDL3"),
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
        html = template.replace("{{TITLE}}", title).replace("{{CONTENT}}", body)
        with open(os.path.join(out, dest), "w", encoding="utf-8") as f:
            f.write(html)
        print("rendered %s -> %s (%d bytes)" % (
            os.path.relpath(src, ROOT), dest, len(html)))

    for name in VERBATIM:
        shutil.copyfile(os.path.join(SITE, name), os.path.join(out, name))
        print("copied   %s" % name)


if __name__ == "__main__":
    main()
