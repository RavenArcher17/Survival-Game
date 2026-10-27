# Ranged weapons test on the arena: a bot fires the bow (stone then tranq arrows), crossbow, blowgun, a thrown spear
# and a bola at a Cinderfang, with screenshots of each weapon in hand (art camera on the bot). Checks shots, hits,
# torpor, the bola's root and the spear left to pick up. -Body f for the female body.
param([int]$Seconds = 125, [string]$Body = '', [int]$Port = 7795)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-ranged-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\players\Archer.json'), (Join-Path (Split-Path $project) 'Saved\ServerDB\world_rangedtest.json') -ErrorAction SilentlyContinue
$shot = 'HighResShot 1600x900|wait 1'
$aim = 'WRAimAt cinderfang|WRHeal|wait 0.1'   # after the aim zoom settles, and a frame for the camera to swing round
# A fresh Cinderfang (70 hp, 70 torpor) for each weapon, rooted so it can't reach the archer: a bite staggers, and a
# staggered survivor can't shoot.
$target = 'WRClearCreatures|WRSpawn cinderfang 1|WRHold 40|wait 1'   # rooted as it lands (it would lunge at you)
$cmds = @(
	'WRBotPause 1', 'wait 2',
	'WRGive hunting_bow 1', 'WRGive stone_arrow 10', 'WRGive tranq_arrow 10', 'WRGive crossbow 1', 'WRGive crossbow_bolt 5',
	'WRGive blowgun 1', 'WRGive torpor_dart 10', 'WRGive throwing_spear 2', 'WRGive bola 2', 'wait 1',
	'WREquip hunting_bow', 'wait 1', 'WRRanged 1', 'wait 2', 'WRArtCam me 100', 'wait 1', $shot,     # bow out, side
	$target, $aim, 'WRAim 1', 'wait 1', $aim, $shot, $aim,                                             # aiming the bow
	'WRFire', 'wait 0.3', $shot, 'wait 1', $aim, 'WRFire', 'wait 1.3',                                 # two stone arrows
	'WRArtCam cinderfang 60', 'wait 1', $shot, 'WRArtCam me 100',                                      # arrows stuck in it
	$target, 'WRAmmo 1', 'wait 0.5', $aim, 'WRFire', 'wait 1.3', $aim, 'WRFire', 'wait 1', 'WRTameStat', # two tranq arrows: out cold
	'WREquip crossbow', 'wait 1.5', $target, 'WRAim 1', 'wait 1', $aim, $shot, $aim, 'WRFire', 'wait 2.8', $aim, 'WRFire', 'wait 1',
	'WREquip blowgun', 'wait 1.5', $target, 'WRAim 1', 'wait 1', $aim, $shot, $aim, 'WRFire', 'wait 1.3', $aim, 'WRFire', 'wait 1', 'WRTameStat',
	'WRAim 0', 'WREquip throwing_spear', 'wait 1.5', $target, $shot, 'WRAim 1', 'wait 1', $aim, 'WRFire', 'wait 0.3', $shot, 'wait 2',
	'WREquip bola', 'wait 1.5', 'WRAim 1', 'wait 1', $aim, 'WRFire', 'wait 1.5', 'WRTameStat', 'WRAim 0', # same target as the spear
	'WRRanged 0', 'wait 1.5', $shot                                                                    # back to sword and shield
) -join '|'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=rangedtest -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$bodyOpt = if ($Body) { "?Body=$Body" } else { '' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=fire?WRName=Archer$bodyOpt -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds $Seconds
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Count($path, $pattern) { @(Select-String -Path $path -Pattern $pattern).Count }
$log = "$out\server.log"
$shots = Count $log 'RANGED .* fires'
$hits = Count $log 'COMBAT WRCharacter.* -> WRCreature'
Check 'every weapon fires (10 shots)' ($shots -ge 10) "$shots shots"
Check 'the shots land' ($hits -ge 9) "$hits hits of $shots"
Check 'stone arrows hit (bow 14 + arrow 6)' ((Count $log 'COMBAT WRCharacter.* -> WRCreature.* dmg=20\.0') -ge 2)
Check 'crossbow bolts hit hard (30 + 10)' ((Count $log 'COMBAT WRCharacter.* -> WRCreature.* dmg=40\.0') -ge 1)
Check 'tranq arrows and torpor darts knock Cinderfangs out' ((Count $log 'TAME .* knocked out') -ge 2) "$(Count $log 'TAME .* knocked out') knockouts"
Check 'arrows stick in what they hit' ((Count $log 'RANGED (bow|crossbow|blowgun) hit WRCreature') -ge 4)
Check 'a thrown spear is left to pick up' ((Count $log 'throwing_spear landed; left to pick up') -ge 1)
Check 'the bola roots a Cinderfang' ((Count $log 'RANGED bola roots') -ge 1)
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None')
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
Select-String -Path "$out\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
