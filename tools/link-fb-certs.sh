#!/usr/bin/env bash
#
# link-fb-certs.sh — point docker/ssl at your auto-renewing Let's Encrypt
# certs, and make nginx actually serve them.
#
# What problem this solves: docker-compose.yml's nginx service bind-mounts
# docker/ssl/fullchain.pem and docker/ssl/privkey.pem (see that file) --
# plain files that, left alone, are stale COPIES made once during initial
# setup. Certbot's own renewal only ever touches
# /etc/letsencrypt/live/<domain>/*, so those two docker/ssl/*.pem files
# never change on their own, and nginx keeps serving whatever cert was
# copied in at setup time, silently, forever -- with nothing that checks
# its expiry until a browser starts rejecting it.
#
# What it does, in order:
#   1. For each of fullchain.pem and privkey.pem: if docker/ssl/<file> is
#      already a symlink resolving to the same real file as
#      /etc/letsencrypt/live/<domain>/<file> (readlink -f, which follows
#      the live/ -> archive/ hop certbot itself uses), leave it alone and
#      print "unchanged". Otherwise -- a stale copy, a symlink to an old
#      archive file, or nothing at all -- remove whatever is there and
#      create a fresh symlink to the live/ path. Symlinking to live/,
#      never to archive/<n> directly, is what makes this step-1 check keep
#      passing across every future renewal: certbot repoints live/ at a
#      new archive file each time but never changes the live/ path itself.
#   2. Force-recreates the nginx container (`docker compose up -d
#      --force-recreate nginx`), unconditionally -- even when step 1
#      printed "unchanged" for both files. This is not redundant: Docker
#      resolves a bind-mounted symlink once, at container *creation*, so
#      an already-running nginx keeps serving whatever archive file was
#      live at ITS creation time even after step 1 repoints the docker/ssl
#      symlinks at a newer one. Recreating is what makes the new target
#      actually take effect; relinking alone would silently do nothing
#      until nginx happened to restart for some unrelated reason.
#
# Idempotent: running it twice in a row with nothing having changed prints
# "unchanged" for both files and still recreates nginx (a few seconds, no
# real effect) rather than erroring -- safe to run from cron, from a
# certbot --deploy-hook, or by hand at any time, as often as you like.
#
# One cert pair per run: this project's nginx.conf (docker/nginx.conf) is a
# single `server { listen 443 ssl; ... }` block with one hardcoded
# certificate path, not a multi-SNI/multi-vhost config -- so a deployment
# serves exactly one domain's cert at a time. Running this script against a
# different <domain> repoints that same single pair at a different live/
# directory; it does not add a second, simultaneously-served domain. If you
# need that, you'll need to extend nginx.conf yourself with additional
# `server {}` blocks and SNI-based cert selection.
#
# Where this is typically invoked from:
#   - update-server.sh (see tools/update-server.sh in this repo for a
#     template) calls this unconditionally as its last step, after its own
#     `certbot renew` -- see that script for the full update flow this is
#     one piece of.
#   - Can also be wired in directly as certbot's own deploy hook, so a
#     renewal triggers the relink+recreate the moment it happens rather
#     than waiting for update-server.sh's next scheduled run:
#       sudo certbot renew --deploy-hook /path/to/link-fb-certs.sh
#   - Or just run by hand any time nginx seems to be serving a
#     stale/expired cert and you want to fix it immediately.
#
# Requires root (run via sudo): /etc/letsencrypt/live is 0700 root-only, and
# on hosts where the operator's own user isn't in the `docker` group, the
# `docker compose` call in step 2 needs sudo as well. Checked up front with
# a useful message, rather than failing confusingly partway through, or with
# certbot's own misleading "run certbot first" if the real issue is just
# missing permission to read a directory that does exist.
#
# Usage:
#   sudo ./link-fb-certs.sh yourdomain.com        # required: your cert's domain
#   FROZEN_BUBBLE_REPO=/path/repo sudo ./link-fb-certs.sh yourdomain.com  # non-default checkout path
#
# There is deliberately no default domain here -- this script lives in the
# repo and is meant to work for any hoster's own domain, not just the
# project's own production host.

set -euo pipefail

# ── Helpers ────────────────────────────────────────────────────────────────────
die() { echo "ERROR: $*" >&2; exit 1; }

# ── Config ─────────────────────────────────────────────────────────────────────
DOMAIN="${1:-}"
[ -n "$DOMAIN" ] || die "usage: $0 <domain>  (e.g. $0 yourdomain.com) -- no default domain, this script is shared across hosters"

REPO_DIR="${FROZEN_BUBBLE_REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
COMPOSE_DIR="$REPO_DIR/docker"
SSL_DIR="$COMPOSE_DIR/ssl"
LIVE_DIR="/etc/letsencrypt/live/$DOMAIN"

# ── Preflight ──────────────────────────────────────────────────────────────────
[ -d "$COMPOSE_DIR" ] || die "compose dir not found: $COMPOSE_DIR"

# The Let's Encrypt live/ tree is 0700 root, so this must run as root (or via
# sudo). Give a useful hint instead of the misleading 'run certbot first'.
if [ ! -r "/etc/letsencrypt/live" ]; then
    die "cannot read /etc/letsencrypt/live — run this script as root (sudo)"
fi
[ -d "$LIVE_DIR" ] || die "no live certs for '$DOMAIN' at $LIVE_DIR (run certbot first)"

# ── Symlink the certs ──────────────────────────────────────────────────────────
for f in fullchain.pem privkey.pem; do
    src="$LIVE_DIR/$f"
    dst="$SSL_DIR/$f"

    [ -e "$src" ] || die "missing live cert: $src"

    # Already the correct symlink? readlink -f resolves live -> archive so this
    # stays true across renewals (the symlink always targets the live/ path).
    if [ -L "$dst" ] && [ "$(readlink -f "$dst")" = "$(readlink -f "$src")" ]; then
        echo "unchanged: $dst -> $(readlink "$dst")"
        continue
    fi

    # Remove whatever is there (stale copy, old symlink, stray dir) and relink.
    rm -rf -- "$dst"
    ln -s "$src" "$dst"
    echo "linked:    $dst -> $src"
done

# ── Serve the current cert ─────────────────────────────────────────────────────
# Docker resolves bind-mount symlinks at container-creation time, so a running
# nginx still holds the previous archive file after certbot re-points the live
# symlinks. Force a recreate so the mount re-resolves to the new file.
cd "$COMPOSE_DIR"
echo "recreating nginx to pick up the current cert..."
docker compose up -d --force-recreate nginx
echo "done."
