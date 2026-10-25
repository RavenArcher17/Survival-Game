# Ranged weapons test on the arena: a bot fires the bow (stone then tranq arrows), crossbow, blowgun, a thrown spear
# and a bola at a Cinderfang, with screenshots of each weapon in hand (art camera on the bot). Checks shots, hits,
# torpor, the bola's root and the spear left to pick up. -Body f for the female body.
param([int]$Seconds = 125, [string]$Body = '')
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$shotDir = Join-Path (Split-Path $project) 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-ranged-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\players\Archer.json') -ErrorAction SilentlyContinue
$shot = 'HighResShot 1600x900|wait 1'
$aim = 'WRAimAt cinderfang|WRHeal|wait 0.1'   # after the aim zoom settles, and a frame for the camera to swing round
# A fresh Cinderfang (70 hp, 70 torpor) for each weapon, rooted so it can't reach the archer: a bite staggers, and a
# staggered survivor can't shoot.
$target = 'WRClearCreatures|WRSpawn cinderfang 1|wait 0.5|WRHold 40|wait 0.5'
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
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7795 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$bodyOpt = if ($Body) { "?Body=$Body" } else { '' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7795?Path=fire?WRName=Archer$bodyOpt -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
Start-Sleep -Seconds $Seconds
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\server.log" -Pattern 'RANGED |TAME |-> WRCreature|CMD .*(held|removed)' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
"shots: $((Select-String -Path "$out\server.log" -Pattern 'RANGED .* fires' | Measure-Object).Count)  hits: $((Select-String -Path "$out\server.log" -Pattern 'COMBAT WRCharacter.* -> WRCreature' | Measure-Object).Count)"
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)  client errors: $((Select-String -Path "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }
Write-Output "logs: $out"
