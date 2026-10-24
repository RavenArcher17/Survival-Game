# Eye-level screenshots among the island's trees (first-person, several directions) to check they sit on the ground.
param([int]$Seconds = 150)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-treeshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$shot = 'wait 2|HighResShot 1600x900|wait 1'
$cmds = @('WRBotPause 1', 'WRView fp',
	'WRTeleport -100400 -129600', 'wait 12', 'WRLook 0 -6', $shot, 'WRLook 90 -6', $shot, 'WRLook 200 -6', $shot,   # volcanic spawn hills
	'WRTeleport -60000 -100000', 'wait 12', 'WRLook 30 -6', $shot, 'WRLook 150 -6', $shot,                            # volcanic midlands
	'WRTeleport 48400 -2000', 'wait 12', 'WRLook 60 -4', $shot, 'WRLook 180 -4', $shot,                                # glacier pines
	'WRTeleport 96182 -40000', 'wait 12', 'WRLook 0 -6', $shot, 'WRLook 120 -6', $shot                                 # tundra pines
) -join '|'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7806 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7806?Path=fire?WRName=TreeCam -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds $Seconds
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
