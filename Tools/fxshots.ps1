# Effects captures (WRFx): a rendered client on L_Island (against a headless server) plays combat, spells and world
# moments and takes numbered screenshots of each, in daylight and at night, from the player's camera and a second angle
# (WRArtCam frames a creature or your own body from the side). Sequences are several shots a few frames apart.
#   fxshots.ps1 -Pass combat|frost|world [-Out <folder>] [-Only <scene,...>] [-Port 7864]
#   combat (fire path): heavy hits on a creature, a parry, crit / weak numbers, fire bolt, fire wall, lava pool, perf
#   frost  (frost path): frost shard, frost nova, ice lance, a burning and a frozen creature
#   world  (fire path): chopping, mining, picking, placing and demolishing, Hearth, torch, brazier, lava embers, ash, snow
param([string]$Pass = 'combat', [string]$Out = '', [string[]]$Only = @(), [int]$Port = 7864)
# One run at a time on this machine (shares uishots' lock: each starts a server and a rendered client).
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-fxshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$name = 'Sparky'
$path = if ($Pass -eq 'frost') { 'frost' } else { 'fire' }
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date

# In a scene's commands, "SHOT <label>" takes a picture; "BURST <label> <n> <gap>" takes n pictures gap seconds apart.
# Flat open ground by volcanic camp 3: a creature (a passive one for melee, so it can't kill you mid-run) spawned in front and rooted, you beside it (melee: WRGoTo stands you
# 2 m on its +Y side, so yaw 270 faces it) or 7 m off (spells).
$arena = 'WRHeal|WRTeleport -124800 -51600|wait 5|WRClearCreatures|WRLook 0 0|wait 0.5|WRSpawn {0} 1|wait 0.4|WRHold 120|wait 1|WRGoTo {0}|wait 1|WRLook 270 -8|wait 1'
$far = 'WRTeleport -124800 -51600|wait 5|WRClearCreatures|WRLook 0 0|wait 0.5|WRSpawn {0} 1|wait 0.4|WRHold 120|wait 1|WRAimAt {0}|wait 1'
$scenes = [ordered]@{}
if ($Pass -eq 'combat') {
	$scenes['heavy_day'] = 'WRHour 12|wait 2|' + ($arena -f 'ashstrider') + '|WR.FxPress heavy|wait 0.5|BURST heavy_day 6 0.05|wait 1.5|WR.FxPress heavy|wait 0.58|SHOT heavy_day_b|wait 1.5'
	$scenes['heavy_side'] = 'WRCreatureHP ashstrider 1|wait 0.5|WRArtCam ashstrider 100|wait 1.5|WR.FxPress heavy|wait 0.55|BURST heavy_side 4 0.06|wait 1|WR.FxHit crit 61|wait 0.15|SHOT crit_side|wait 1.2|WR.FxHit weak 38|wait 0.15|SHOT weak_side|wait 1.2|WRArtCam off|wait 1'
	$scenes['parry'] = 'WRCreatureHP ashstrider 1|wait 0.5|WR.FxHit parry|wait 0.03|BURST parry 4 0.05|wait 1|WRArtCam ashstrider 100|wait 1.2|WR.FxHit parry|wait 0.03|BURST parry_side 3 0.05|WRArtCam off|wait 1'
	$scenes['block'] = 'WR.FxHit block|wait 0.03|SHOT block|wait 1'
	$scenes['bolt_day'] = ($far -f 'ashstrider') + '|WRCast 1|wait 0.22|BURST bolt_day 6 0.07|wait 1.5|WRArtCam ashstrider 90|wait 1.5|WRCast 1|wait 0.25|BURST bolt_side 5 0.07|WRArtCam off|wait 1.5'
	$scenes['firewall_day'] = 'WRTeleport -124800 -51600|wait 3|WRClearCreatures|WRLook 0 -15|wait 1|WRCast 3|wait 0.6|SHOT spell3_day|wait 1.5|SHOT spell3_day_b|wait 3|WRLook 40 -25|wait 0.5|SHOT spell3_day_c|wait 4'
	$scenes['lavapool_day'] = 'WRLook 0 -20|wait 1|WRCast 4|wait 0.7|SHOT spell4_day|wait 1.5|SHOT spell4_day_b|wait 2|WRLook -40 -25|wait 0.5|SHOT spell4_day_c|wait 7'
	$scenes['heavy_night'] = 'WRHour 23|wait 3|' + ($arena -f 'ashstrider') + '|WR.FxPress heavy|wait 0.55|BURST heavy_night 4 0.05|wait 1.5|WR.FxHit parry|wait 0.03|SHOT parry_night|wait 1'
	$scenes['bolt_night'] = ($far -f 'ashstrider') + '|WRCast 1|wait 0.22|BURST bolt_night 5 0.07|wait 2'
	$scenes['firewall_night'] = 'WRClearCreatures|WRLook 0 -15|wait 1|WRCast 3|wait 0.8|SHOT spell3_night|wait 2.5|WRLook 0 -20|WRCast 4|wait 1|SHOT spell4_night|wait 2'
	# Close looks at effects the camera rarely catches mid-game (chips flying off a struck tree), spawned in front of you with
	# WR.Fx: the same recipes gameplay plays (the run's 'FX started' line shows the gameplay ones arrived).
	$scenes['looks'] = 'WRHour 12|wait 2|WRTeleport -124800 -51600|wait 4|WRClearCreatures|WRLook 0 -20|wait 1|WR.Fx gather.wood 1.3 2.5|wait 0.12|BURST look_wood 2 0.15|wait 1.5|WR.Fx gather.stone 1.3 2.5|wait 0.15|SHOT look_stone|wait 1.5|WR.Fx gather.fiber 1.3 2.5|wait 0.25|SHOT look_fiber|wait 1.5|WR.Fx build.break.wood 1.5 4|wait 0.2|SHOT look_break|wait 2|WR.Fx stomp 4 6|wait 0.2|SHOT look_stomp|wait 2|WR.Fx frostnova 5 7|wait 0.25|SHOT look_nova|wait 2|WR.Fx icelance 20 1.5 0 0|wait 0.2|SHOT look_lance|wait 2'
	# Frame time with stat unit, then with a heavy load of effects around you (WR.FxStress), and the effects' own cost.
	$scenes['perf'] = 'WRHour 12|wait 2|WRTeleport -124800 -51600|wait 4|WRClearCreatures|stat unit|wait 4|WR.FxStats|wait 2|SHOT perf_before|WR.FxStats|WR.FxStress|wait 2|SHOT perf_stress|WR.FxStats|WR.FxStress|wait 2|SHOT perf_stress_b|WR.FxStats|stat unit|wait 1'
} elseif ($Pass -eq 'frost') {
	$scenes['shard_day'] = 'WRHour 12|wait 2|' + ($far -f 'frostantler') + '|WRCast 1|wait 0.22|BURST shard_day 6 0.07|wait 1.5|WRArtCam frostantler 90|wait 1.5|WRCast 1|wait 0.25|BURST shard_side 5 0.07|WRArtCam off|wait 1.5'
	$scenes['spell3_day'] = 'WRLook 0 -6|wait 1|WRCast 3|wait 0.15|BURST spell3_day 5 0.08|wait 2|WRArtCam frostantler 90|wait 1.5|WRCast 3|wait 0.15|BURST spell3_side 4 0.08|WRArtCam off|wait 2'
	$scenes['spell4_day'] = ($arena -f 'ashstrider') + '|WRLook 0 -25|wait 1|WRCast 4|wait 0.3|BURST spell4_day 5 0.08|wait 2|WRArtCam me 120|wait 1.5|WRCast 4|wait 0.3|BURST spell4_side 4 0.08|WRArtCam off|wait 2'
	$scenes['status'] = 'WRStatus burn|wait 1.5|WRArtCam me 60|wait 1.5|SHOT burn_me|wait 0.5|WRStatus frozen|wait 1|SHOT frozen_me|WRArtCam off|wait 1'
	$scenes['shard_night'] = 'WRHour 23|wait 3|' + ($far -f 'frostantler') + '|WRCast 1|wait 0.22|BURST shard_night 5 0.07|wait 2|WRLook 0 -6|WRCast 3|wait 0.15|BURST spell3_night 4 0.08|wait 2'
	$scenes['spell4_night'] = ($arena -f 'ashstrider') + '|WRLook 0 -25|wait 1|WRCast 4|wait 0.3|BURST spell4_night 4 0.08|wait 2'
} else {
	# The survival test's hut site on the volcanic side (falling ash there), a lava field further east, a tundra camp.
	$build = 'WRGive wood 300|WRGive stone 200|WRGive fiber 50|WRGive copper_ingot 10|WRGive core_vulkarn 1'
	$scenes['gather_tree'] = 'WRHour 12|wait 2|WRTeleport -124800 -51600|wait 5|WRClearCreatures|WRBotPause 0|WRGather tree|wait 10|WRGather off|WRBotPause 1|wait 1.5|WR.FxPress light|wait 0.32|BURST tree 4 0.1|wait 1.2|WRArtCam me 70|wait 1.5|WR.FxPress light|wait 0.32|BURST tree_side 3 0.1|WRArtCam off|wait 1'
	$scenes['gather_rock'] = 'WRBotPause 0|WRGather rock|wait 10|WRGather off|WRBotPause 1|wait 1.5|WR.FxPress light|wait 0.32|BURST rock 4 0.1|wait 1.2|WRArtCam me 70|wait 1.5|WR.FxPress light|wait 0.32|BURST rock_side 3 0.1|WRArtCam off|wait 1'
	$scenes['gather_shrub'] = 'WRBotPause 0|WRGather shrub|wait 10|WRGather off|WRBotPause 1|wait 1.5|WR.FxPress light|wait 0.32|BURST shrub 4 0.1|wait 1.2|WRArtCam me 70|wait 1.5|WR.FxPress light|wait 0.32|BURST shrub_side 3 0.1|WRArtCam off|wait 1'
	$scenes['build'] = 'WRTeleport -131300 -56400|wait 5|WRClearCreatures|' + $build + '|WRTeleport -131650 -56400|wait 3|WRLook 180 -22|wait 1|WRPlace wood_foundation -132000 -56400 4734 0|wait 0.25|BURST place 3 0.15|wait 1|WRPlace stone_wall -132200 -56400 4834 90|wait 0.25|SHOT place_wall|wait 2|WRDemolish|wait 0.15|BURST demolish 5 0.12|wait 2|WRArtCam me 150|wait 1.5|WRPlace wood_foundation -132000 -56400 4734 0|wait 0.25|SHOT place_side|wait 2|WRDemolish|wait 0.15|BURST demolish_side 3 0.12|WRArtCam off|wait 2'
	$scenes['hearth_day'] = 'WRPlace claim_totem -131100 -56000 4847 0|wait 1|WRPlace hearth -131300 -56800 4838 0|wait 1|WRPlace torch -130900 -56700 4840 0|WRPlace brazier -131000 -57100 4835 0|wait 2|WRTeleport -131300 -56300|wait 3|WRLook 270 -18|wait 2|SHOT hearth_day|wait 1|WRTeleport -130500 -56900|wait 3|WRLook 180 -12|wait 2|SHOT hearth_day_b'
	$scenes['lava_day'] = 'WRTeleport -30500 82500|wait 6|WRLook 0 -10|wait 3|SHOT lava_day|wait 1|SHOT lava_day_b|WRTeleport -25500 79000|wait 5|WRLook 90 -15|wait 3|SHOT lava_day_c'
	$scenes['snow_day'] = 'WRTeleport 136800 51400|wait 7|WRLook 0 -5|wait 4|SHOT snow_day|wait 1|WRLook 120 15|wait 1|SHOT snow_day_b'
	$scenes['ash_day'] = 'WRTeleport -131300 -56300|wait 6|WRLook 90 5|wait 4|SHOT ash_day|WRLook 90 -10|wait 1|SHOT ash_day_b'
	$scenes['night'] = 'WRHour 23|wait 4|WRTeleport -131300 -56300|wait 4|' + $build + '|WRPlace claim_totem -131100 -56000 4847 0|wait 1|WRPlace hearth -131300 -56800 4838 0|wait 1|WRPlace torch -130900 -56700 4840 0|WRPlace brazier -131000 -57100 4835 0|wait 2|WRLook 270 -18|wait 3|SHOT hearth_night|WRTeleport -130500 -56900|wait 3|WRLook 180 -12|wait 2|SHOT hearth_night_b|WRLook 90 5|wait 2|SHOT ash_night|WRTeleport -30500 82500|wait 6|WRLook 0 -10|wait 3|SHOT lava_night|WRLook 90 -25|wait 1|SHOT lava_night_b|WRTeleport 136800 51400|wait 7|WRLook 0 -5|wait 4|SHOT snow_night|WRLook 120 15|wait 1|SHOT snow_night_b|WRTeleport -124800 -51600|wait 5|WRBotPause 0|WRGather tree|wait 10|WRGather off|WRBotPause 1|wait 1.5|WR.FxPress light|wait 0.32|BURST tree_night 3 0.1'
}
if ($Only.Count -gt 0) { $Only = $Only | ForEach-Object { $_ -split ',' } }
$cmds = @('WRBotPause 1', 'wait 3', 'WRGive stone_hatchet 1', 'WRGive stone_pick 1', "WRGivePoints $path 120", "WRLearn $path $(if ($path -eq 'fire') { 'fire_bolt' } else { 'frost_shard' })", "WRAutoSpend $path 100", 'WRWeapon copper common 0', 'wait 2')
$labels = @()
foreach ($s in $scenes.GetEnumerator()) {
	$take = $Only.Count -eq 0 -or $Only -contains $s.Key
	foreach ($c in ($s.Value -split '\|')) {
		$c = $c.Trim()
		if ($c -match '^SHOT (\S+)$') { if ($take) { $cmds += 'shot showui'; $labels += $Matches[1] } }
		elseif ($c -match '^BURST (\S+) (\d+) ([\d.]+)$') {
			if ($take) { for ($i = 1; $i -le [int]$Matches[2]; $i++) { $cmds += 'shot showui'; $labels += ('{0}_{1}' -f $Matches[1], $i); if ($i -lt [int]$Matches[2]) { $cmds += "wait $($Matches[3])" } } }
		}
		else { $cmds += $c }
	}
}
$cmds += 'WR.FxStats', 'wait 2'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=fxshots -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=${path}?WRName=${name}?Body=m -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
$deadline = (Get-Date).AddSeconds(300 + 30 * $scenes.Count)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	$n = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started }).Count
	if ($n -ge $labels.Count -and (Select-String -Path "$logs\client.log" -Pattern 'FX stats' -Quiet -ErrorAction SilentlyContinue)) { Start-Sleep -Seconds 2; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_fxshots.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$files = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime, Name)
for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
	$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
	Move-Item $files[$i].FullName $dest -Force
	$dest
}
$errors = @(Select-String -Path "$logs\client.log", "$logs\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Select-Object -First 8)
if ($errors) { "errors:"; $errors | ForEach-Object { '  ' + $_.Line.Trim() } }
Select-String -Path "$logs\client.log" -Pattern 'FX (ready|stats|started|stress|unknown)' -ErrorAction SilentlyContinue | ForEach-Object { '  ' + ($_.Line -replace '^.*LogWildreach: ', '') }
"{0} of {1} shots (logs: {2})" -f [Math]::Min($files.Count, $labels.Count), $labels.Count, $logs
$lock.ReleaseMutex()
