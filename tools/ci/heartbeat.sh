#!/bin/bash
# Writes the current Unix timestamp to a GitHub Actions repository variable,
# from a stable, permanent location on the WSL2 box -- deliberately NOT from
# inside a CI job's ephemeral _work checkout, since this needs to keep
# running independent of whether any job is currently executing.
#
# Scheduled from the WINDOWS side, not from inside WSL2: a Windows Scheduled
# Task ("FBCIHeartbeatLinux") runs `wsl.exe -d Ubuntu -- .../heartbeat.sh`
# every minute. A systemd timer running this on a schedule INSIDE WSL2 was
# tried first and doesn't work reliably -- WSL2's own VM can idle-shut-down
# between invocations even with a supposedly-always-on background service
# running (observed live: the timer's OnBootSec/OnUnitActiveSec schedule is
# monotonic relative to the VM's boot time, so every idle-shutdown silently
# resets it, and nothing then wakes the VM to let it fire again until some
# unrelated process happens to touch WSL2). Driving it from Windows sidesteps
# this entirely, since `wsl.exe` auto-boots the VM on demand regardless of
# whether it was idled out -- confirmed by watching three consecutive
# unattended ticks land exactly 60s apart with no manual WSL invocation in
# between.
#
# Exists because GitHub's own self-hosted-runner status API lags reality by
# several minutes after a runner actually goes offline (observed directly:
# the box was fully asleep, but the API still reported this runner as
# "online"), which defeats build.yml's runner-status job during exactly the
# window it matters most. The workflow reads this variable directly via the
# `vars` context -- no token needed for that read -- and treats it as stale
# (offline) once it's more than ~2.5 heartbeat intervals old.
#
# Requires a fine-grained PAT with Variables: Read and write (repo-scoped)
# authenticated locally on this box via `gh auth login`, separate from the
# RUNNER_STATUS_PAT stored as a repo secret (that one lives inside workflow
# runs; this one runs from the box itself, so it can't be a repo secret).
set -euo pipefail
gh variable set FB_CI_HEARTBEAT_LINUX --body "$(date +%s)" --repo dchau360/frozen-bubble-sdl3
