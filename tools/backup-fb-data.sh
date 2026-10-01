#!/bin/bash
# Copy a server's fb-data volume (weekly rankings, world highscores, their
# weekly snapshots and the ban file) to this machine as one dated tarball,
# keeping the last KEEP days of them. Meant to run daily from cron or launchd;
# see SetupServer.md, "Backing up the scores".
#
#   tools/backup-fb-data.sh [user@host] [dest-dir]
#
# Env: FB_BACKUP_KEEP (default 30), FB_BACKUP_VOLUME (default docker_fb-data).
# Needs key-based SSH to the host and passwordless sudo there (for docker).
set -euo pipefail

HOST="${1:-ubuntu@fb.servequake.com}"
DEST="${2:-$HOME/Documents/fb-backups}"
KEEP="${FB_BACKUP_KEEP:-30}"
VOLUME="${FB_BACKUP_VOLUME:-docker_fb-data}"

mkdir -p "$DEST"
out="$DEST/fb-data-$(date +%Y-%m-%d).tgz"
tmp="$out.part"
trap 'rm -f "$tmp"' EXIT

# Read-only mount: the backup can never change what it copies. The server
# keeps running; every file in the volume is replaced by write-then-rename,
# so a copy taken mid-save still gets a whole file, old or new.
ssh -o BatchMode=yes -o ConnectTimeout=20 "$HOST" \
    "sudo docker run --rm -v $VOLUME:/d:ro alpine tar czf - -C /d ." > "$tmp"

# A truncated or empty download must not replace a good backup.
if ! tar tzf "$tmp" | grep -qx './hiscores.dat\|./weekly.dat'; then
    echo "$(date '+%F %T') backup FAILED: no score files in the download" >&2
    exit 1
fi
mv "$tmp" "$out"
echo "$(date '+%F %T') saved $out ($(wc -c < "$out" | tr -d ' ') bytes)"

# Drop the backups from KEEP to KEEP+120 days ago by name. Not by listing the
# folder: macOS lets a launchd job write into ~/Documents but not read the
# folder's contents, so a glob there silently matches nothing.
days_ago() {
    date -v-"$1"d +%Y-%m-%d 2>/dev/null || date -d "$1 days ago" +%Y-%m-%d
}
for ((d = KEEP; d <= KEEP + 120; d++)); do
    old="$DEST/fb-data-$(days_ago "$d").tgz"
    if [ -e "$old" ]; then rm -f "$old" && echo "removed $old"; fi
done
