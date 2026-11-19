# Net walk test: joining players (clients) walk on runtime-built ground (the Desert, the Forest, the Graveyard) and
# must not throw the server's movement corrections away. Before 11-18 every correction there was relative to a ground
# piece the client couldn't name ("could not resolve the new relative movement base actor, ignoring server
# correction"), so a friend rubber-banded and drifted from where the server had them. Counts those lines and every
# correction (p.NetShowCorrections) per walker.
#   netwalktest.ps1 [-Seconds 240]
param([int]$Seconds = 240)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-netwalk-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\Worlds\netwalktest\world.json') -ErrorAction SilentlyContinue
# Each walks about 150 m across its land's ground and back.
$walkers = @(
	@{ Name = 'NetDesert'; Cmds = 'WRBotPause 1|wait 5|WRTeleport -72720 -664080|wait 12|WRWalkTo -58000 -664080|wait 40|WRWalkTo -72720 -664080|wait 40' },
	@{ Name = 'NetForest'; Cmds = 'WRBotPause 1|wait 5|WRTeleport 56600 371200|wait 12|WRWalkTo 71600 371200|wait 40|WRWalkTo 56600 371200|wait 40' },
	@{ Name = 'NetGraveyard'; Cmds = 'WRBotPause 1|wait 5|WRTeleport -122600 312400|wait 12|WRWalkTo -107600 312400|wait 40|WRWalkTo -122600 312400|wait 40' }
)
$procs = @()
$procs += Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7810 -wrworld=netwalktest -wrstore=json -wrdefaultpreset=playtest -wrtune=wildlife.enabled:0 `"-ExecCmds=p.NetShowCorrections 1`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 40
foreach ($w in $walkers) {
	$procs += Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7810?Path=fire?WRName=$($w.Name) -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60,p.NetShowCorrections 1`" `"-wrbotcmds=$($w.Cmds)`" -abslog=`"$out\$($w.Name).log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 5
}
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) { Start-Sleep -Seconds 5 }
foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path (Split-Path $project) 'Saved\Worlds\netwalktest\world.json') -ErrorAction SilentlyContinue

$failed = 0
foreach ($w in $walkers) {
	$log = Join-Path $out "$($w.Name).log"
	$ignored = @(Select-String -Path $log -Pattern 'ignoring server correction' -ErrorAction SilentlyContinue).Count
	$corrections = @(Select-String -Path $log -Pattern '\*\*\* Client: Error for' -ErrorAction SilentlyContinue).Count
	$joined = @(Select-String -Path "$out\server.log" -Pattern "Join succeeded: $($w.Name)" -ErrorAction SilentlyContinue).Count -gt 0
	$ok = $joined -and $ignored -eq 0
	if (-not $ok) { $failed++ }
	"{0,-6} {1,-14} joined {2,-5} corrections thrown away {3,4}   corrections {4,4}" -f $(if ($ok) { 'PASS' } else { 'FAIL' }), $w.Name, $joined, $ignored, $corrections
}
"server corrections sent: {0}" -f @(Select-String -Path "$out\server.log" -Pattern '\*\*\* Server: Error for' -ErrorAction SilentlyContinue).Count
"logs: $out"
if ($failed -gt 0) { exit 1 }
