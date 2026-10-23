# Visual check: headless server + one rendered client bot that takes screenshots from both views.
# Screenshots land in Saved\Screenshots\ (newest run printed at the end).
param([int]$Seconds = 45)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-shots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null

$cmds = @(
	'WRGivePoints fire 40', 'WRAutoSpend fire 30', 'WRSetSub frost', 'WRGivePoints frost 30', 'WRAutoSpend frost 25',
	'wait 8', 'HighResShot 1600x900',                 # third person, fighting the dummies
	'wait 3', 'WRView fp', 'wait 2', 'HighResShot 1600x900', # first person
	'wait 1', 'WRView tp', 'WRBotPause 1', 'WRGoToAltar frost', 'wait 2', 'HighResShot 1600x900', # altar prompt
	'WRBotPause 0', 'WRSpawn cinderfang 3', 'wait 6', 'HighResShot 1600x900'   # pack fight
) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7790 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7790?Path=fire?WRName=ShotBot -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds $Seconds
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
