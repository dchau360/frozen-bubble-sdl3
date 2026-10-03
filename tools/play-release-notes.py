#!/usr/bin/env python3
"""Write Google Play's "What's new" text for one release, from CHANGELOG.md.

Usage: play-release-notes.py <version, e.g. v2.4.133 or 2.4.133> <out dir>

Writes <out dir>/whatsnew-en-US, the layout the deploy-play-store job in
.github/workflows/build.yml hands to upload-google-play. Play allows 500
characters per language, while a CHANGELOG entry runs to paragraphs, so each
bullet becomes its bold title plus its first sentence. If that is still too
long the extra sentences are dropped, then bullets from the end, so a long
release never fails the upload over its notes. A version with no section
gets a plain fallback line rather than an error.
"""

import re
import sys
from pathlib import Path

LIMIT = 500


def section(changelog: str, version: str) -> str:
    match = re.search(rf"^## v?{re.escape(version)}\s*$(.*?)(?=^## |\Z)",
                      changelog, re.MULTILINE | re.DOTALL)
    return match.group(1) if match else ""


def plain(text: str) -> str:
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)  # links -> their text
    text = text.replace("**", "").replace("`", "")
    return re.sub(r"\s+", " ", text).strip()


def bullets(body: str):
    """(title, first sentence) for each top-level bullet."""
    for item in re.split(r"^- ", body, flags=re.MULTILINE)[1:]:
        item = item.strip()
        bold = re.match(r"\*\*(.+?)\*\*\s*(.*)", item, re.DOTALL)
        title, rest = (bold.group(1), bold.group(2)) if bold else ("", item)
        rest = plain(rest)
        first = re.match(r"(.+?[.!?])(\s|$)", rest)
        yield plain(title), (first.group(1) if first else rest)


def notes(changelog: str, version: str) -> str:
    items = list(bullets(section(changelog, version)))
    if not items:
        return "Bug fixes and improvements."

    def render(full):
        lines = []
        for (title, sentence), long_form in zip(items, full):
            text = title if title else sentence
            if long_form and title and sentence:
                text = f"{title} {sentence}"
            lines.append(f"• {text}")
        return "\n".join(lines)

    full = [True] * len(items)
    text = render(full)
    # Shorten from the last bullet up: titles only, then fewer bullets.
    for i in reversed(range(len(items))):
        if len(text) <= LIMIT:
            return text
        full[i] = False
        text = render(full)
    while len(text) > LIMIT and len(items) > 1:
        items.pop()
        full.pop()
        text = render(full)
    return text[:LIMIT]


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    version = sys.argv[1].lstrip("v")
    repo = Path(__file__).resolve().parent.parent
    text = notes((repo / "CHANGELOG.md").read_text(encoding="utf-8"), version)
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    (out / "whatsnew-en-US").write_text(text + "\n", encoding="utf-8")
    print(text)
    print(f"({len(text)} characters)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
