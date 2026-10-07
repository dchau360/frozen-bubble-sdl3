#!/usr/bin/env python3
"""Render the published site from its single source in the repo.

The privacy policy used to exist as three hand-synced copies: this repo's
docs/PRIVACY_POLICY.md, plus an index.md and a hand-written index.html on the
gh-pages branch. Only the last of those was ever served, so the published
policy silently fell behind the source -- it was still describing the app
before the two ad-removal products existed, which is exactly the sort of thing
a store review reads. Generating it removes the chance to forget.

Usage: tools/build-site.py <output-dir> [--brand-site <dir> <prefix>]

--brand-site adds another game's pages under /<prefix>/ -- Boba Buster's at
/bb/, from site/ in the private boba-buster-assets repo (its art is not GPL,
so its pages live with its art, not here). See render_brand_site().
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
    # The world highscore board. Static like everything else here: the page's
    # own script asks the world-board server (fb.servequake.com) over a
    # WebSocket (see site/scores.md), so nothing on this site has to know the
    # scores and every origin that serves it shows the same live board. The
    # game's "Open in browser" button points here (kWorldScoresUrl). It has
    # its own template, dressed as the game's High Scores screen, rather than
    # the plain document look the other two pages share.
    #
    # The weekly rankings page reads the server's WEEKLY board the same way
    # and shares that template. The fifth field fills the template's
    # per-page slots: BAR is what sits on the wood strip at the top (the
    # High Scores logo is baked into the game's art, so only that page can
    # use it), PAGE marks which of the two pages is current in the nav.
    (os.path.join(SITE, "scores.md"),
     os.path.join("scores", "index.html"),
     "Highscores — Frozen Bubble: SDL3",
     "template-scores.html",
     {"BAR": '<img src="{{ROOT}}img/scores-header.png" alt="High Scores" width="558" height="30">',
      "PAGE": "scores"}),
    (os.path.join(SITE, "weekly.md"),
     os.path.join("weekly", "index.html"),
     "Weekly rankings — Frozen Bubble: SDL3",
     "template-scores.html",
     {"BAR": '<span class="bar-title">Weekly Rankings</span>',
      "PAGE": "weekly"}),
]

# (source path, path within the site)
#
# docs/ is not published wholesale -- only what this list names -- so a new
# screenshot on the landing page has to be added here or it renders as a
# broken image. They are the README's own screenshots, so the two stay in
# step when one is retaken.
ASSETS = [
    *[(os.path.join(ROOT, "docs", "screenshots", name), os.path.join("screenshots", name))
      for name in ("modern-gameplay.jpg", "title-menu.jpg", "how-to-play.jpg",
                   "two-player-win.jpg", "game-room-modern.jpg", "round-stats.png")],
    # Google's official "Get it on Google Play" badge, served from here rather
    # than hotlinked so opening the page makes no request to Google.
    (os.path.join(SITE, "img", "google-play-badge.png"),
     os.path.join("img", "google-play-badge.png")),
    # The world highscores page wears the game's own High Scores screen:
    # pieces cut from share/gfx/back_hiscores.png (the logo on its wood
    # strip, a plain stretch of that wood, and the artwork inside the frame)
    # and the font that screen is drawn in, served from here like the badge.
    (os.path.join(SITE, "img", "scores-header.png"),
     os.path.join("img", "scores-header.png")),
    (os.path.join(SITE, "img", "scores-wood.png"),
     os.path.join("img", "scores-wood.png")),
    (os.path.join(SITE, "img", "scores-back.jpg"),
     os.path.join("img", "scores-back.jpg")),
    (os.path.join(ROOT, "share", "gfx", "DroidSans.ttf"),
     os.path.join("fonts", "DroidSans.ttf")),
    (os.path.join(ROOT, "share", "gfx", "Baloo2-ExtraBold.ttf"),
     os.path.join("fonts", "Baloo2-ExtraBold.ttf")),
]

# Copied through verbatim. The Google verification token must keep its exact
# bytes and filename or the site stops being verified.
#
# app-ads.txt authorizes who may sell ad inventory for the Android build, and
# ad networks' crawlers look for it at the ROOT of whatever developer website
# the app declares. It is served at the apex of dchau360.github.io from a separate
# repo (that domain's root, which this project's Pages site is only a
# subdirectory of), and shipping a copy here puts it at the root of the
# fb.servequake.com deployment too -- so naming either origin as the developer
# website satisfies the check. Its contents come from the ad mediator's
# dashboard (Appodeal's list, one line per network seller); whenever it
# changes, the copy in the dchau360.github.io repo has to change with it.
VERBATIM = ["google034c4b2cf8d147df.html", "app-ads.txt"]

# This generator's output is deployed byte-for-byte identical to three
# origins -- llmfinder.net and fb.servequake.com (both via docker/Dockerfile.site,
# nginx's server_name _; catch-all) and dchau360.github.io/frozen-bubble-sdl3
# (via .github/workflows/pages.yml) -- with no canonical signal between them.
# Google Search Console folded llmfinder.net's copy into "Duplicate without
# user-selected canonical" as a result. llmfinder.net is the domain actually
# verified for Play/AdMob's website-ownership check (see the superseded note
# on fb.servequake.com in tools/build-site.py's git history / project memory),
# so every rendered copy -- including the GitHub Pages one -- declares it as
# canonical, telling Google which origin to index.
CANONICAL_BASE = "https://llmfinder.net"


def render_brand_site(src_dir, prefix, out):
    """Render a second game's pages under /<prefix>/ of the same site.

    By convention rather than a list like PAGES, so the brand's repo needs no
    change here to add a page: <src_dir>/index.md is /<prefix>/, any other
    <name>.md is /<prefix>/<name>/, each page's <title> is its first "# "
    heading, and <src_dir>/template.html wraps them all (same {{TITLE}},
    {{ROOT}}, {{CANONICAL}} and {{CONTENT}} slots as ours, {{ROOT}} being the
    brand's own root). Its img/ and fonts/ directories are copied as they are.
    Frozen Bubble's root pages are untouched, and app-ads.txt stays at the
    domain root, which is the only place ad crawlers look for it.
    """
    template = open(os.path.join(src_dir, "template.html"), encoding="utf-8").read()
    for name in sorted(os.listdir(src_dir)):
        if not name.endswith(".md") or name == "README.md":
            continue
        stem = name[:-3]
        sub_path = "" if stem == "index" else stem + "/"
        text = open(os.path.join(src_dir, name), encoding="utf-8").read()
        title = next((line[2:].strip() for line in text.splitlines()
                      if line.startswith("# ")), stem)
        body = markdown.markdown(text, extensions=["extra", "sane_lists"])
        html = (template
                .replace("{{TITLE}}", title)
                .replace("{{ROOT}}", "../" if sub_path else "./")
                .replace("{{CANONICAL}}", "%s/%s/%s" % (CANONICAL_BASE, prefix, sub_path))
                .replace("{{CONTENT}}", body))
        dest = os.path.join(out, prefix, sub_path, "index.html")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8") as f:
            f.write(html)
        print("rendered %s -> %s/%sindex.html (%d bytes)" % (name, prefix, sub_path, len(html)))
    for sub in ("img", "fonts"):
        if os.path.isdir(os.path.join(src_dir, sub)):
            shutil.copytree(os.path.join(src_dir, sub), os.path.join(out, prefix, sub),
                            dirs_exist_ok=True)
            print("copied   %s/ -> %s/%s/" % (sub, prefix, sub))


def main():
    args = sys.argv[1:]
    brand = None
    if len(args) == 4 and args[1] == "--brand-site":
        brand = (args[2], args[3])
        args = args[:1]
    if len(args) != 1:
        sys.exit("usage: build-site.py <output-dir> [--brand-site <dir> <prefix>]")
    out = args[0]
    os.makedirs(out, exist_ok=True)

    templates = {}

    for src, dest, title, *rest in PAGES:
        template_name = rest[0] if rest else "template.html"
        if template_name not in templates:
            templates[template_name] = open(os.path.join(SITE, template_name), encoding="utf-8").read()
        template = templates[template_name]
        # Per-page slots first, so a slot's own {{ROOT}} is rewritten below.
        for key, value in (rest[1] if len(rest) > 1 else {}).items():
            template = template.replace("{{%s}}" % key, value)
        text = open(src, encoding="utf-8").read()
        # The first heading becomes the page's <h1>, which the template does
        # not supply -- so the markdown's own "# ..." line is kept, not stripped.
        body = markdown.markdown(text, extensions=["extra", "sane_lists"])
        # Pages one level down need their relative links to the site root
        # rewritten; the template is shared and written from the root's view.
        depth = len(os.path.dirname(dest).split(os.sep)) if os.path.dirname(dest) else 0
        # dest is "index.html" or "<subdir>/index.html"; strip the filename
        # so "privacy/index.html" canonicalizes to ".../privacy/", not
        # ".../privacy/index.html" (a distinct URL Google would treat as a
        # second duplicate of the one just declared canonical).
        canonical_path = dest[: -len("index.html")] if dest.endswith("index.html") else dest
        canonical_url = CANONICAL_BASE + "/" + canonical_path
        html = (template
                .replace("{{TITLE}}", title)
                .replace("{{ROOT}}", "../" * depth if depth else "./")
                .replace("{{CANONICAL}}", canonical_url)
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

    if brand:
        render_brand_site(brand[0], brand[1], out)


if __name__ == "__main__":
    main()
