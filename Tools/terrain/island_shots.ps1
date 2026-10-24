# Rendered screenshots around L_Island from several places and angles -> Saved\Screenshots.
param([int]$Seconds = 160)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-islandshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null

# Generous waits: World Partition streams the landscape around each new position (and builds
# textures the first time on uncooked content).
$shot = 'wait 14|HighResShot 1600x900'
$cmds = @(
	'WRBotPause 1',
	'WRTeleport -100400 -129600', 'WRLook 0 -8', $shot,          # NW volcanic spawn, looking east at the ridge
	'WRTeleport -20000 -129600', 'WRLook 0 -5', $shot,           # approaching the north pass
	'WRTeleport -1300 -129600', 'WRLook 90 -10', $shot,          # in the north pass, looking south along the ridge
	'WRTeleport 30000 -100000', 'WRLook 45 -12', $shot,          # tundra side past the pass
	'WRTeleport -78600 -20000', 'WRLook 90 -15', $shot,          # volcano flank, looking at the crater
	'WRTeleport -140000 60000', 'WRLook 20 -6', $shot            # west coast, lowlands,
	'WRTeleport -91800 -90131', 'WRLook 0 -4', $shot,          # outside a volcanic cave entrance (midlands)
	'WRWalkTo -87900 -90131', 'wait 14', 'WRLook 180 -6', $shot,  # inside the cave chamber, looking out past the ore
	'WRTeleport 96182 -40000', 'WRLook 90 -10', $shot,           # tundra pines near a cave
	'WRTeleport -40400 79000', 'WRLook 90 -18', $shot,          # the lava river (volcanic south), from its north bank
	'WRTeleport 48400 -2000', 'WRLook 90 -18', $shot            # slick ice on the glacier plateau
) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7805 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7805?Path=fire?WRName=IslandCam -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds $Seconds
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
