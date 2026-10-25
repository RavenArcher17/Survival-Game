# Art check for the player body: the art camera (WRArtCam me) frames the test bot's survivor while it idles (front and
# side), walks, and fights the arena's training dummies. Shots land in Saved\Screenshots (paths printed at the end).
param([int]$Seconds = 70)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-playershots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$shot = 'HighResShot 1600x900|wait 1'
$cmds = @(
	'WRBotPause 1', 'wait 3',
	'WRArtCam me 20', 'wait 2', $shot,                     # idle, front
	'WRArtCam me 100', 'wait 1', $shot,                    # idle, side
	'WRWalkTo 3000 0', 'wait 2', $shot,                    # moving, side
	'WRBotPause 0', 'WRArtCam me 60', 'wait 6', $shot,      # fighting the dummies
	'wait 1', $shot, 'wait 1', $shot
) -join '|'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7792 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7792?Path=fire?WRName=ArtBot -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds $Seconds
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
Write-Output "logs: $out"
