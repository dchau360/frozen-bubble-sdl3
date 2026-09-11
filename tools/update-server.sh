#!/usr/bin/env bash
#
# update-server.sh — TEMPLATE for the one script that fully updates a
# production frozen-bubble-sdl3 server: pulls the latest code, rebuilds and
# restarts the game containers, and keeps the SSL cert renewed and served,
# all in one run. Run by hand for an on-demand deploy, or on a schedule (see
# "Scheduling" below) so the cert renewal never depends on anyone
# remembering to do it.
#
# THIS IS A TEMPLATE, NOT MEANT TO RUN FROM INSIDE THE REPO. Copy it
# somewhere outside your checkout first, e.g.:
#
#   cp tools/update-server.sh ~/update-server.sh
#   chmod +x ~/update-server.sh
#
# Why it can't just live in and run from the repo: step 1 below does a
# `git pull` on the very checkout this script would be sitting in. If a pull
# changes this file mid-run, bash can end up executing a half-old/half-new
# script (bash reads a running script incrementally off disk, it doesn't
# load the whole thing into memory first) -- a self-modification hazard a
# plain `git pull` while a script from that repo is executing can hit at
# any time, not just on a release that happens to touch this file. Keeping
# your working copy outside the repo sidesteps it entirely. (Its sibling,
# tools/link-fb-certs.sh, does NOT have this problem -- it's invoked as a
# subprocess *after* the pull in step 1 has already finished, so it's safe
# to call directly from inside the checkout, and this script does exactly
# that below.)
#
# What it does, in order:
#   1. Pulls the latest frozen-bubble-sdl3 `main` into the checkout at
#      $FROZEN_BUBBLE_REPO, as the repo's actual owner (auto-detected, see
#      "Repo owner" below) -- not root -- see the comment at that step for
#      why the owner matters.
#   2. Rebuilds the fb-server and discord-relay Docker images from that
#      checkout and recreates both containers from them (`docker compose up
#      -d --build`). This is what actually ships new game/relay code; step 1
#      alone only updates the files on disk.
#   3. Runs `certbot renew --quiet`, which is a no-op unless the cert is
#      within 30 days of expiry (Let's Encrypt's own renewal window) --
#      calling it unconditionally on every run is what makes this script
#      "handle renewing the cert" with no separate reminder to run it.
#   4. Calls tools/link-fb-certs.sh from the checkout (unconditionally, not
#      only when step 3 actually renewed something) to point
#      docker/ssl/*.pem at whatever is currently live under
#      /etc/letsencrypt and force-recreate the nginx container. This step is
#      what makes a *new* cert actually get served: Docker resolves a
#      bind-mounted symlink once, at container creation, so a running nginx
#      keeps serving the old cert file even after certbot repoints the
#      live/ symlink -- and it's also what makes nginx pick up a changed
#      nginx.conf from step 1's pull, since that file is bind-mounted and
#      only re-read on a restart, never baked into an image a rebuild would
#      refresh.
#   5. Prints each of the three containers' RestartCount and State, so a
#      crash loop from a bad deploy is obvious immediately in the output
#      (or in the cron log -- see below) instead of only showing up the next
#      time someone happens to check `docker ps`.
#
# Every step above is safe to run when there is nothing to do: an
# already-current checkout, an image that hasn't changed, and a cert that
# isn't due all make their step a fast no-op rather than an error. That is
# what makes this script safe to run repeatedly and on a schedule, not just
# by hand right after a code change.
#
# Config (env vars, all optional except DOMAIN):
#   FROZEN_BUBBLE_REPO   Path to your checkout. Default: /home/$USER/gr/frozen-bubble-sdl3
#                        for a plain manual run, or -- since this script
#                        normally runs as root via cron/sudo, where $USER is
#                        "root" -- you almost always want to set this
#                        explicitly. See "Repo owner" below.
#   UPDATE_SERVER_STATE_FILE  Where --if-due's last-successful-run timestamp
#                        is recorded. Default: <repo owner's home>/.update-server-last-run
#
# Domain (required, one of):
#   FB_DOMAIN=yourdomain.com ./update-server.sh     # via env var
#   ./update-server.sh yourdomain.com                # or as a plain argument
# There is deliberately no default -- this is a shared template, not a copy
# of any one operator's own script, so it refuses to guess your domain
# rather than silently defaulting to someone else's.
#
# Repo owner (used for git pull and the state file, see those steps below):
#   Auto-detected as whoever owns $FROZEN_BUBBLE_REPO on disk
#   (`stat -c '%U' "$FROZEN_BUBBLE_REPO"`) -- not hardcoded to any particular
#   username, so this template works unmodified whether your checkout is
#   owned by `ubuntu`, `deploy`, your own login, or anything else. Override
#   with REPO_OWNER=someuser if your setup needs something other than the
#   checkout's actual on-disk owner.
#
# Flags:
#   --no-renew          Skip step 3 (certbot renew). Cert relinking in step 4
#                        still runs, so this only skips the *renewal* -- an
#                        already-current cert is still relinked/served fine.
#   --no-pull            Skip step 1 (git pull). Steps 2-5 still run, so this
#                        rebuilds+restarts from whatever is already checked
#                        out and still handles certs -- useful for testing a
#                        local change before it's pushed.
#   --if-due             For the cron/scheduled case (see "Scheduling"):
#                        instead of always running, first check how long it's
#                        been since this script last completed successfully
#                        (recorded in $STATE_FILE) and exit immediately,
#                        printing why, if that is under $INTERVAL_DAYS days.
#                        Ignored -- i.e. always runs -- when invoked without
#                        this flag, which is what a manual "update now" run
#                        wants: today's cron cadence should never block a
#                        deploy someone is doing by hand right now.
#
# Requires root (run via sudo, or from root's own crontab): certbot needs to
# read/write /etc/letsencrypt (0700, root-only), and on hosts where the
# repo owner isn't in the `docker` group, every docker command here needs
# root too. Checked once, up front, so a permission error surfaces
# immediately rather than after step 1 or 2 already did something.
#
# Scheduling ("every N weeks"):
#   cron has no field for "every N weeks" that stays exactly N*7 days apart
#   forever -- a day-of-month step like "*/28" drifts against real calendar
#   dates (months aren't 28 days long, so it does not land on the same
#   weekday and can double up near a month boundary), and a day-of-week
#   field can only mean "every week" or "this specific weekday every week",
#   never "every Nth occurrence of a weekday". So this script is meant to be
#   invoked from cron *daily* (any fixed time -- 1am is a reasonable choice
#   when player traffic is lowest) with --if-due, and --if-due is what
#   actually enforces the N-week spacing ($INTERVAL_DAYS below), self-timed
#   off its own last successful completion rather than off the calendar:
#
#     0 1 * * * /home/YOURUSER/update-server.sh --if-due yourdomain.com >> /home/YOURUSER/update-server.log 2>&1
#
#   in root's crontab (`sudo crontab -e`) -- root's, not the repo owner's,
#   since this script requires root regardless of who invokes it, and a
#   cron command cannot interactively answer a sudo password prompt.
#
#   This also self-heals a missed run: if the host is down or busy at the
#   scheduled time on the day it was due, --if-due simply runs the very next
#   day once cron fires again, rather than waiting a full extra cycle the
#   way a pure calendar-based schedule (had one been possible here) would.
#
# State file ($STATE_FILE, default <repo owner's home>/.update-server-last-run):
#   A single Unix timestamp, written only after every step above has
#   completed successfully (a failure partway through -- a bad git pull, a
#   build error -- leaves the previous timestamp in place, so a broken run
#   is retried the next day instead of being counted as done). Missing
#   entirely (first run on a new host, or after deleting it by hand) counts
#   as "due" -- --if-due always runs rather than erroring when it has never
#   run before. Written as $REPO_OWNER (see the git pull step), not root,
#   even though this whole script requires root -- so you can delete or
#   inspect it by hand later without needing sudo just for that.
#
# Usage:
#   sudo ./update-server.sh yourdomain.com             # full update: code + certs, right now
#   sudo ./update-server.sh --no-renew yourdomain.com   # skip certbot renew (still relinks)
#   sudo ./update-server.sh --no-pull yourdomain.com    # skip git pull (just rebuild + certs)
#   sudo ./update-server.sh --if-due yourdomain.com     # the cron form -- see "Scheduling"
#   sudo FB_DOMAIN=yourdomain.com ./update-server.sh    # domain via env var instead

set -euo pipefail

DOMAIN=""
DO_RENEW=1
DO_PULL=1
IF_DUE=0
INTERVAL_DAYS="${UPDATE_SERVER_INTERVAL_DAYS:-28}"

for arg in "$@"; do
    case "$arg" in
        --no-renew) DO_RENEW=0 ;;
        --no-pull)  DO_PULL=0 ;;
        --if-due)   IF_DUE=1 ;;
        --*)        echo "ERROR: unknown flag: $arg" >&2; exit 2 ;;
        *)          DOMAIN="$arg" ;;
    esac
done
DOMAIN="${DOMAIN:-${FB_DOMAIN:-}}"

die() { echo "ERROR: $*" >&2; exit 1; }
log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"; }

[ -n "$DOMAIN" ] || die "no domain given -- pass one as an argument or set FB_DOMAIN (e.g. $0 yourdomain.com)"

REPO_DIR="${FROZEN_BUBBLE_REPO:-/home/${SUDO_USER:-$USER}/gr/frozen-bubble-sdl3}"
COMPOSE_DIR="$REPO_DIR/docker"
LINK_CERTS="$REPO_DIR/tools/link-fb-certs.sh"

# certbot renew reads/writes /etc/letsencrypt (0700 root), and docker on
# hosts where the repo owner isn't in the docker group needs sudo too --
# same requirement link-fb-certs.sh's own preflight already states, checked
# once here up front instead of failing halfway through a run.
[ "$(id -u)" -eq 0 ] || die "run as root (sudo) -- needed for certbot and (on most hosts) docker"
[ -d "$COMPOSE_DIR" ] || die "compose dir not found: $COMPOSE_DIR (set FROZEN_BUBBLE_REPO?)"
[ -x "$LINK_CERTS" ] || die "missing or not executable: $LINK_CERTS"

# The repo's actual on-disk owner, not hardcoded -- so this template works
# unmodified regardless of what user actually owns the checkout. Used both
# for the git pull (below) and the state file, so root-run steps never
# leave root-owned artifacts the real operator can't manage without sudo.
REPO_OWNER="${REPO_OWNER:-$(stat -c '%U' "$REPO_DIR")}"
STATE_FILE="${UPDATE_SERVER_STATE_FILE:-$(eval echo "~$REPO_OWNER")/.update-server-last-run}"

if [ "$IF_DUE" -eq 1 ] && [ -f "$STATE_FILE" ]; then
    last_run="$(cat "$STATE_FILE")"
    now="$(date +%s)"
    age_days=$(( (now - last_run) / 86400 ))
    if [ "$age_days" -lt "$INTERVAL_DAYS" ]; then
        log "skipping: last successful run was $age_days day(s) ago, due at $INTERVAL_DAYS (see $STATE_FILE)"
        exit 0
    fi
    log "due: last successful run was $age_days day(s) ago (>= $INTERVAL_DAYS) -- proceeding"
elif [ "$IF_DUE" -eq 1 ]; then
    log "due: no record of a previous run at $STATE_FILE -- proceeding"
fi

if [ "$DO_PULL" -eq 1 ]; then
    log "== pulling latest main =="
    # As the repo's actual owner, not root -- a root-owned .git index entry
    # from running this unqualified would make the next plain 'git pull' the
    # operator runs by hand (no sudo) fail with "dubious ownership" or a
    # permission error on whatever root just touched.
    sudo -u "$REPO_OWNER" git -C "$REPO_DIR" pull --ff-only origin main
else
    log "== skipping git pull (--no-pull) =="
fi

log "== rebuilding and restarting fb-server + discord-relay =="
( cd "$COMPOSE_DIR" && docker compose up -d --build fb-server discord-relay )

if [ "$DO_RENEW" -eq 1 ]; then
    log "== renewing certs (no-op unless within 30 days of expiry) =="
    certbot renew --quiet
else
    log "== skipping certbot renew (--no-renew) =="
fi

# Unconditional, not just "if certbot actually renewed something": this is
# also what makes nginx pick up a changed nginx.conf from the pull above,
# since that file is bind-mounted (read at nginx startup, not baked into an
# image), and it's already a no-op when nothing changed (see its own
# "unchanged:" path) -- cheap either way.
log "== relinking certs and ensuring nginx serves the current one =="
"$LINK_CERTS" "$DOMAIN"

log "== status =="
for c in docker-fb-server-1 docker-discord-relay-1 docker-nginx-1; do
    docker inspect "$c" --format "$c: RestartCount={{.RestartCount}} State={{.State.Status}}" 2>/dev/null \
        || echo "$c: not found"
done

# Written last, and only on success -- everything above ran without a single
# failing command (set -e), so this run counts toward the next --if-due
# check. A run that dies partway through leaves the previous timestamp in
# place, which is what makes tomorrow's cron retry it instead of silently
# treating a broken deploy as done for another cycle.
#
# As $REPO_OWNER, not root -- same reasoning as the git pull above: this
# whole script runs as root, but the state file lives in the repo owner's
# home directory, and a root-owned file there would need sudo to delete or
# reset by hand later. Writing it as that user once now keeps it that way
# forever after too: an existing file's owner doesn't change just because
# a later run (root, via cron) opens it for writing again.
date +%s | sudo -u "$REPO_OWNER" tee "$STATE_FILE" >/dev/null
log "done."
