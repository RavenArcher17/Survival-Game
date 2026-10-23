# Walk test on L_Island: real character movement from spawns to the passes, and into the ridge wall.
param([int]$Seconds = 420)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-walk-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null

$walkers = @(
	@{ Name = 'WalkA_NorthPass'; Cmds = 'WRBotPause 1|WRTeleport -100400 -129600|wait 3|WRWalkTo 60000 -129600' },
	@{ Name = 'WalkB_MiddleSpawn'; Cmds = 'WRBotPause 1|WRTeleport -131200 -51600|wait 3|WRWalkTo -1300 -129600' },
	@{ Name = 'WalkC_RidgeWall'; Cmds = 'WRBotPause 1|WRTeleport -60000 0|wait 3|WRWalkTo 60000 0' }
)
$procs = @()
$procs += Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7800 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
foreach ($w in $walkers) {
	$procs += Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7800?Path=fire?WRName=$($w.Name) -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($w.Cmds)`" -abslog=`"$out\$($w.Name).log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 2
}
Start-Sleep -Seconds $Seconds
foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
foreach ($w in $walkers) {
	"--- $($w.Name)"
	Select-String -Path "$out\$($w.Name).log" -Pattern 'WALK |CMD .*tp' -ErrorAction SilentlyContinue | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
}
Select-String -Path "$out\server.log" -Pattern 'CMD .*tp ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
