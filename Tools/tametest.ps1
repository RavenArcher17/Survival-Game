# End-to-end taming test on the arena: offer food (Ashstrider), ride it; ARK-style knockout taming (Cinderfang:
# two tranq arrows, a stone arrow that costs effectiveness, a narcotic, kibble in its stash, tamed with a bonus);
# steal and hatch a Kilnmite egg, give orders; a Rimeclaw left to wake up loses its progress; then restart the server
# and check tames were saved (with their bonus). Taming times and the unconscious torpor drain are shortened with
# -wrtune. -Shots runs a windowed client and takes screenshots of the taming panel and the Taming menu.
param([switch]$Shots)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-tame-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Tamer.json'), (Join-Path $db 'world_L_CombatArena.json') -ErrorAction SilentlyContinue
$tune = '-wrtune=taming.easySeconds:4,taming.mediumSeconds:6,taming.hardSeconds:600,taming.tameLimit:5,taming.unconsciousTorporDecay:0.05'
$before = Get-Date
$shot = if ($Shots) { 'HighResShot 1600x900|wait 1' } else { 'wait 0.1' }
$aim = 'WRAimAt cinderfang|wait 0.1'   # a frame for the camera to swing round before the shot

$cmds = @(
	'WRBotPause 1', 'WRGive raw_meat 5', 'WRGive berries 5',
	'WRTeleport 1500 -2200', 'wait 1', 'WRSpawn ashstrider 1', 'wait 1', 'WRGoTo ashstrider', 'WRInteract', 'wait 7', 'WRTameStat',
	'WRTame ride', 'wait 2', 'WRTameStat', 'WRDismount', 'wait 1', 'WRTameStat', 'wait 1', 'WRTame stay', 'wait 1',
	# Knockout taming: tranq arrows put it down, a stone arrow while it's out costs effectiveness, a narcotic keeps it
	# under, kibble in its stash tames it.
	'WRGive hunting_bow 1', 'WRGive tranq_arrow 4', 'WRGive narcotic 2', 'WRGive kibble 3',
	'WRTeleport -1500 -5000', 'wait 1', 'WRSpawn cinderfang 1', 'wait 0.5', 'WRHold 30', 'wait 0.5',
	'WREquip hunting_bow', 'wait 1', 'WRRanged 1', 'wait 1', 'WRAim 1', 'wait 1',
	$aim, 'WRFire', 'wait 1.3', $aim, 'WRFire', 'wait 1.3', 'WRTameStat',
	'WRGive stone_arrow 1', 'WRAmmo 1', 'wait 0.5', $aim, 'WRFire', 'wait 1', 'WRAim 0', 'WRRanged 0', 'WRTameStat',
	'WRGoTo cinderfang', 'wait 0.5', 'WRNarcotic', 'wait 0.5', 'WRInteract', 'wait 1', $shot, 'WRInteract', 'wait 0.5',
	'WRTameGive kibble', 'wait 1', $shot, 'WRTameStat', 'wait 7', 'WRTameStat',
	'WRGoTo nest kilnmite', 'wait 1', 'WRInteract', 'wait 1', 'WREquip egg_kilnmite', 'wait 2', 'WRTameStat', 'WRTeleport 900 -3300', 'wait 5', 'WRTameStat', 'WRTame aggressive', 'WRTame stay', 'wait 0.5',
	# Left to wake up: a Rimeclaw (hard: 600 s of food, 60 s on the playtest preset) part-tamed on raw meat loses its progress when it wakes.
	'WRTeleport -1500 -5000', 'wait 1', 'WRSpawn rimeclaw 1', 'wait 0.5', 'WRGoTo rimeclaw', 'WRTorpor 1000', 'wait 0.5',
	'WRTameGive raw_meat', 'wait 8', 'WRTameStat', 'wait 12.5', 'WRTameStat', 'WRClearCreatures'   # cleared as it wakes, before it bites the tames
) -join '|'

function Start-Server($log) {
	Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7811 $tune -abslog=`"$log`"" -PassThru -WindowStyle Hidden
}
$s = Start-Server "$out\server.log"
Start-Sleep -Seconds 20
$view = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7811?Path=fire?WRName=Tamer -game $view -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
Start-Sleep -Seconds 130   # past the 60 s and 120 s world autosaves
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
Select-String -Path "$out\server.log" -Pattern 'TAME |WORLD saved|RANGED .*(fires|hit)' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"

# Restart: the tames should come back.
$s2 = Start-Server "$out\server2.log"
Start-Sleep -Seconds 20
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }
Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
if ($Shots) {
	Get-ChildItem (Join-Path (Split-Path $project) 'Saved\Screenshots') -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
}
