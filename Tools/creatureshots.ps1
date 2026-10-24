# Art check: spawn each creature in its own corner of the combat arena and screenshot it with WRArtCam
# (front three-quarter, then side-on profile while it moves).
#   powershell -File Tools\creatureshots.ps1 magmaw basaltback ...
# Shots land in Saved\Screenshots\ in creature order (two per creature); the paths are printed at the end.
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Ids)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-creatureshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null

# 3x3 grid, 4500 apart: farther than any aggro radius, so earlier spawns stay out of later shots.
$spots = foreach ($y in -4500, 0, 4500) { foreach ($x in -4500, 0, 4500) { "$x $y" } }
$cmds = @('WRBotPause 1', 'wait 2')
for ($i = 0; $i -lt $Ids.Count; $i++) {
	$id = $Ids[$i]
	# HighResShot captures on the next rendered frame, so wait before changing the camera again.
	$cmds += "WRTeleport $($spots[$i % 9])", 'wait 1', 'WRHeal', "WRSpawn $id 1", 'wait 1', "WRArtCam $id 60", 'wait 1', 'HighResShot 1600x900', 'wait 1',
		'WRHeal', "WRArtCam $id -90", 'wait 2', 'HighResShot 1600x900', 'wait 1', 'WRArtCam off'
}
$cmds = $cmds -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7791 -wrtune=creature.burrowHide:0 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7791?WRName=ArtBot -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds (30 + 9 * $Ids.Count)
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
Write-Output "logs: $out"
