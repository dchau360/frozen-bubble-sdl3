# Stops the helper started by keepawake-start.ps1 (same -PidFile), releasing
# the ES_SYSTEM_REQUIRED hold so normal idle-sleep behavior resumes once the
# job is done. Safe to call even if the start step never ran or already
# cleaned up -- a missing PID file is a no-op, not an error, since this runs
# as a job's last step with `if: always()` and must not itself fail the job.
param(
    [Parameter(Mandatory = $true)][string]$PidFile
)

if (Test-Path $PidFile) {
    $targetId = Get-Content $PidFile
    Stop-Process -Id $targetId -Force -ErrorAction SilentlyContinue
    Remove-Item $PidFile -ErrorAction SilentlyContinue
    Write-Host "Stopped keep-awake helper (PID $targetId)"
} else {
    Write-Host "No keep-awake PID file at $PidFile, nothing to stop"
}
