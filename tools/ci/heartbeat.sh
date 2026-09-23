#!/bin/bash
# Writes the current Unix timestamp to a GitHub Actions repository variable,
# run on a 1-minute schedule (systemd timer) from a stable, permanent
# location on the WSL2 box -- deliberately NOT from inside a CI job's
# ephemeral _work checkout, since this needs to keep running independent of
# whether any job is currently executing.
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
