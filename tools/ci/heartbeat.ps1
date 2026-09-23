# Writes the current Unix timestamp to a GitHub Actions repository variable,
# run on a 1-minute schedule (Windows Scheduled Task) from a stable,
# permanent location on this box -- deliberately NOT from inside a CI job's
# ephemeral _work checkout, since this needs to keep running independent of
# whether any job is currently executing.
#
# See tools/ci/heartbeat.sh (the WSL2/Linux counterpart) for why this exists:
# GitHub's own self-hosted-runner status API lags reality by several minutes
# after a runner actually goes offline, which this heartbeat sidesteps by
# giving build.yml's runner-status job a much fresher, self-controlled signal
# to read instead (via the `vars` context -- no token needed for that read).
#
# Requires a fine-grained PAT with Variables: Read and write (repo-scoped)
# authenticated locally on this box via `gh auth login`, separate from the
# RUNNER_STATUS_PAT stored as a repo secret (that one lives inside workflow
# runs; this one runs from the box itself, so it can't be a repo secret).
$epochSeconds = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
gh variable set FB_CI_HEARTBEAT_WINDOWS --body "$epochSeconds" --repo dchau360/frozen-bubble-sdl3
