# Creature AI test on the arena (no world save involved). Food chain: a Pumicehog dropped by the Cinderfang pack gets
# hunted, killed and eaten (hunger forced on with -wrtune), and an Ashstrider runs from them. Day and night: the
# Gloomhowl pack sleeps by day (doesn't notice you at 13 m) and hunts at night. Morale: a badly hurt Rimeclaw runs when
# shot. Attacks: an Emberhorn charges, a Magmaw spits and lunges, a Basaltback stomps. -Shots: a windowed client and
# screenshots (the hunt, the sleeping pack, each telegraph).
param([switch]$Shots, [int]$Port = 7797)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-ai-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\players\Ranger.json'), (Join-Path (Split-Path $project) 'Saved\ServerDB\world_aitest.json') -ErrorAction SilentlyContinue
$before = Get-Date
$tune = '-wrtune=creatureAI.huntIntervalMinutes:0'
$shot = if ($Shots) { 'HighResShot 1600x900' } else { 'wait 0.1' }
function Aim($id) { "WRAimAt $id|wait 0.1" }
function Hold($seconds) { (1..[math]::Ceiling($seconds) | ForEach-Object { 'wait 1|WRHeal' }) -join '|' }   # wait, staying alive

$cmds = @(
	'WRBotPause 1', 'wait 2', 'WRGive hunting_bow 1', 'WRGive stone_arrow 30', 'WRGive iron_armor 1', 'WREquip hunting_bow', 'WREquip iron_armor', 'wait 1',
	# Food chain: prey turns up by the Cinderfang pack (-3500, 0); we watch from 50 m off.
	'WRTeleport 0 4000', 'wait 1', 'WRSpawnAt pumicehog -3300 900', 'wait 3', 'WRArtCam cinderfang 70', 'wait 2', $shot, 'wait 12',
	'WRSpawnAt ashstrider -3000 1300', 'wait 2', $shot, 'wait 6', 'WRArtCam none',
	# Day and night: the Gloomhowl pack (3500, 0) naps by day, hunts at night.
	'WRHour 12', 'wait 1', 'WRTeleport 3500 -1300', 'wait 4', 'WRArtCam gloomhowl 40', 'wait 1', $shot, 'WRArtCam none',
	'WRHour 22', (Hold 3), 'WRClearCreatures', 'WRHour 12', 'wait 1',
	# Morale: a Rimeclaw at 30% health takes an arrow and runs.
	'WRTeleport 0 2500', 'wait 1', 'WRClearCreatures', 'WRRanged 1', 'wait 1', 'WRAim 1', 'wait 1',
	'WRSpawn rimeclaw 1', 'wait 0.5', 'WRCreatureHP rimeclaw 0.3', (Aim 'rimeclaw'), 'WRFire', (Hold 4), 'WRClearCreatures',
	# Attacks: provoke an Emberhorn (charge), meet a Magmaw (spit, lunge), provoke a Basaltback (stomp).
	'WRSpawn emberhorn 1', 'wait 0.5', (Aim 'emberhorn'), 'WRFire', 'WRArtCam emberhorn 100', 'wait 0.6', $shot, (Hold 6), 'WRArtCam none', 'WRClearCreatures',
	'WRSpawn basaltback 1', 'wait 0.5', (Aim 'basaltback'), 'WRFire', 'WRArtCam basaltback 100', (Hold 6), $shot, (Hold 4), 'WRArtCam none', 'WRClearCreatures',
	'WRSpawn magmaw 1', 'WRArtCam magmaw 100', 'wait 0.4', $shot, (Hold 8), 'WRArtCam none', 'WRClearCreatures'
) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=aitest $tune `"-LogCmds=LogWildreach Verbose`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$view = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=fire?WRName=Ranger -game $view -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
Start-Sleep -Seconds 140
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Count($path, $pattern) { @(Select-String -Path $path -Pattern $pattern).Count }
$log = "$out\server.log"
Check 'hungry carnivores hunt' ((Count $log 'AI .* hunts WRCreature') -ge 1)
Check 'hunted prey runs' ((Count $log 'flees from .* \(hunted') -ge 1)
Check 'the pack kills its prey and shares it' ((Count $log 'DEATH WRCreature.* killed by WRCreature') -ge 1 -and (Count $log 'AI .* eats the ') -ge 2) "$(Count $log 'AI .* eats the ') ate"
Check 'night hunters sleep by day' ((Count $log 'AI .* falls asleep') -ge 1)
Check 'and wake to hunt at night' ((Count $log 'AI .* wakes up') -ge 1 -and (Count $log 'CMD .*hour 22.*\(night\)') -eq 1)
Check 'a badly hurt creature runs' ((Count $log 'flees from .* \(badly hurt') -ge 1)
Check 'an Emberhorn charges' ((Count $log 'AI .* uses charge') -ge 1)
Check 'a Basaltback stomps' ((Count $log 'AI .* uses stomp') -ge 1)
Check 'a Magmaw spits and lunges' ((Count $log 'AI .* uses spit') -ge 1 -and (Count $log 'AI .* uses lunge') -ge 1)
Check 'special attacks land' ((Count $log 'AI .* (lunge|charge|stomp) hits ') -ge 3) "$(Count $log 'AI .* (lunge|charge|stomp) hits ') hits"
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None')
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
if ($Shots) { Select-String -Path "$out\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value } }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
