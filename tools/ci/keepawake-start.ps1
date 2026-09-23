# Starts a detached helper process that holds ES_SYSTEM_REQUIRED for as long
# as it stays alive, so the self-hosted runner's Windows host doesn't sleep
# mid-build. Windows' idle-sleep timer is driven by user input, not CPU/disk
# activity -- a compiler running in the background never resets it -- and a
# full sleep/resume cycle can leave GitHub Actions' runner service unable to
# reconnect (its local process survives suspend, but its session to GitHub's
# backend doesn't always recover on its own), stranding whatever job was
# mid-flight. Pair with keepawake-stop.ps1 (same -PidFile) as a job's last
# step, `if: always()`, so a normal idle-sleep timeout still applies the rest
# of the time -- this only inhibits sleep while a job is actually running.
param(
    [Parameter(Mandatory = $true)][string]$PidFile
)

$helperScript = @'
Add-Type -Name Power -Namespace Native -MemberDefinition @"
[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
public static extern uint SetThreadExecutionState(uint esFlags);
"@
# ES_CONTINUOUS (0x80000000) | ES_SYSTEM_REQUIRED (0x00000001): keeps the
# system (not just the display) from sleeping for as long as this process's
# calling thread stays alive to hold it. Built via Convert.ToUInt32 from a
# hex string, not the 0x80000001 literal directly -- PowerShell parses that
# literal as a signed Int32 (it exceeds Int32.MaxValue, so the literal comes
# out negative), and a plain [uint32] cast on a negative Int32 is a checked
# numeric conversion that throws (out of UInt32's range) rather than
# reinterpreting the bits the way an unchecked C-style cast would. Left
# broken, this line threw every time, but Start-Process doesn't wait for the
# child it launches, so the caller (keepawake-start.ps1 itself) always
# reported success regardless -- the helper was silently dying on launch.
[Native.Power]::SetThreadExecutionState([Convert]::ToUInt32('80000001', 16)) | Out-Null
while ($true) { Start-Sleep -Seconds 30 }
'@

$helperPath = [System.IO.Path]::GetTempFileName() + '.ps1'
Set-Content -Path $helperPath -Value $helperScript -Encoding UTF8

$proc = Start-Process -FilePath powershell.exe `
    -ArgumentList '-NoProfile', '-WindowStyle', 'Hidden', '-File', $helperPath `
    -PassThru
$proc.Id | Set-Content -Path $PidFile -Encoding ASCII
Write-Host "Started keep-awake helper (PID $($proc.Id)), recorded at $PidFile"
