# Server rules test on the arena with the PvP standard preset and two players:
# PvP on between strangers, off between clanmates; Souls-like death bag only its owner can recover;
# life skill XP, levels, perks and their stats; life skills saved with the player.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-social-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Alpha.json'), (Join-Path $db 'players\Bravo.json'), (Join-Path $db 'clans.json') -ErrorAction SilentlyContinue

$alpha = @(
	'WRBotPause 1', 'WRTeleport 1000 1000', 'wait 4',
	'WRHitPlayer Bravo 10', 'wait 1',
	'WRClan create Wolves', 'WRClan invite Bravo', 'wait 6',
	'WRHitPlayer Bravo 10', 'WRClan info', 'wait 6',
	'WRTeleport 1000 1000', 'wait 1', 'WRInteract', 'wait 8',
	'WRSkillXP mining 5000', 'WRSkillStat miningYield', 'WRPerk mining 25 1', 'WRSkill mining 60', 'WRPerk mining 50 1', 'WRPerk mining 50 0',
	'WRSkillStat doubleOre', 'WRSkill taming 50', 'WRPerk taming 50 0', 'WRSkillStat extraTames', 'WRSkill building 25', 'WRPerk building 25 0', 'WRSkillStat buildCost'
)
$bravo = @(
	'WRBotPause 1', 'WRTeleport 1000 1000', 'wait 6', 'WRClan join Wolves', 'wait 6',
	'WRGive wood 20', 'WRTeleport 1000 1000', 'wait 1', 'WRKillMe', 'wait 14', 'WRTeleport 1000 1000', 'wait 1', 'WRInteract'
)
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7813 -wrpreset=pvp_standard -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7813?Path=fire?WRName=Alpha -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($alpha -join '|')`" -abslog=`"$out\alpha.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7813?Path=frost?WRName=Bravo -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($bravo -join '|')`" -abslog=`"$out\bravo.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 70
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\server.log" -Pattern 'Server preset|CLAN |DEATH |SKILL ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"--- notices ---"
Select-String -Path "$out\alpha.log", "$out\bravo.log" -Pattern 'NOTICE ' | ForEach-Object { "$($_.Filename): " + ($_.Line -replace '^.*NOTICE ','') }
$save = Join-Path $db 'players\Alpha.json'
if (Test-Path $save) { "saved skills: " + ((Get-Content $save -Raw | ConvertFrom-Json).lifeSkills | ConvertTo-Json -Compress) }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
