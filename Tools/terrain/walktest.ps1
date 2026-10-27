# Walk test on L_Island: real character movement from spawns to the passes, and into the ridge wall.
# WalkA (north spawn) and WalkB (middle spawn) must reach their pass; WalkC walks east into the ridge
# wall, which must stop it. Ends early once every walker has arrived or got stuck.
param([int]$Seconds = 420)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-walk-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\world_walktest.json') -ErrorAction SilentlyContinue

$walkers = @(
	@{ Name = 'WalkA_NorthPass'; Cmds = 'WRBotPause 1|WRTeleport -100400 -129600|wait 3|WRWalkTo 60000 -129600' },
	@{ Name = 'WalkB_MiddleSpawn'; Cmds = 'WRBotPause 1|WRTeleport -131200 -51600|wait 3|WRWalkTo -1300 -129600' },
	@{ Name = 'WalkC_RidgeWall'; Cmds = 'WRBotPause 1|WRTeleport -60000 0|wait 3|WRWalkTo 60000 0' }
)
$procs = @()
$procs += Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7800 -wrworld=walktest -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
foreach ($w in $walkers) {
	$procs += Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7800?Path=fire?WRName=$($w.Name) -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($w.Cmds)`" -abslog=`"$out\$($w.Name).log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 2
}
function Walk([string]$name) {
	$log = "$out\$name.log"
	if (Test-Path $log) { @(Select-String -Path $log -Pattern 'WALK ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ', '').Trim() }) } else { @() }
}
function Finished([string]$name) { @(Walk $name | Where-Object { $_ -match '^WALK (arrived|stuck) ' }).Count -ge 1 }
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline -and @($walkers | Where-Object { -not (Finished $_.Name) }).Count -gt 0) { Start-Sleep -Seconds 5 }
foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
foreach ($w in $walkers) { "--- $($w.Name)"; Walk $w.Name }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Last([string]$name) { Walk $name | Select-Object -Last 1 }
$tps = @(Select-String -Path "$out\server.log" -Pattern 'CMD .*: tp .*\(ground found\)' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' })
Check 'every spawn teleport finds the ground' ($tps.Count -eq $walkers.Count) "$($tps.Count) of $($walkers.Count)"
Check 'WalkA walks from the north spawn to the north pass' ((Last 'WalkA_NorthPass') -match '^WALK arrived ') (Last 'WalkA_NorthPass')
Check 'WalkB walks from the middle spawn to the pass' ((Last 'WalkB_MiddleSpawn') -match '^WALK arrived ') (Last 'WalkB_MiddleSpawn')
$c = Last 'WalkC_RidgeWall'
Check 'the ridge wall stops WalkC well short of the far side' (@(Walk 'WalkC_RidgeWall' | Where-Object { $_ -match '^WALK arrived ' }).Count -eq 0 -and $c -match ', (\d+) m from target' -and [int]$Matches[1] -ge 500) $c
$logs = @("$out\server.log") + @($walkers | ForEach-Object { "$out\$($_.Name).log" } | Where-Object { Test-Path $_ })
$errors = @(Select-String -Path $logs -Pattern 'Error:|Ensure condition failed|Accessed None')
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
