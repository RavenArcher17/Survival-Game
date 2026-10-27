# Server rules test on the arena with the PvP standard preset and two players:
# PvP on between strangers, off between clanmates; Souls-like death bag only its owner can recover;
# life skill XP, levels, perks and their stats; life skills saved with the player.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-social-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
$save = Join-Path $db 'players\Alpha.json'
Remove-Item $save, (Join-Path $db 'players\Bravo.json'), (Join-Path $db 'clans.json'), (Join-Path $db 'world_socialtest.json') -ErrorAction SilentlyContinue

$alpha = @(
	'WRBotPause 1', 'WRTeleport 1000 1000', 'wait 4',
	'WRHitPlayer Bravo 10', 'wait 1',
	'WRClan create Wolves', 'WRClan invite Bravo', 'wait 8',
	'WRHitPlayer Bravo 10', 'WRClan info', 'wait 6',
	'WRTeleport 1000 1000', 'wait 1', 'WRInteract', 'wait 8',
	'WRSkillXP mining 5000', 'WRSkillStat miningYield', 'WRPerk mining 25 1', 'WRSkill mining 60', 'WRPerk mining 50 1', 'WRPerk mining 50 0',
	'WRSkillStat doubleOre', 'WRSkill taming 50', 'WRPerk taming 50 0', 'WRSkillStat extraTames', 'WRSkill building 25', 'WRPerk building 25 0', 'WRSkillStat buildCost'
)
$bravo = @(
	'WRBotPause 1', 'WRTeleport 1000 1000', 'wait 9', 'WRClan join Wolves', 'wait 6',
	'WRGive wood 20', 'WRTeleport 1000 1000', 'wait 1', 'WRKillMe', 'wait 14', 'WRTeleport 1000 1000', 'wait 1', 'WRInteract', 'wait 1', 'WRSay socialtest-done'
)
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7813 -wrworld=socialtest -wrpreset=pvp_standard -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7813?Path=fire?WRName=Alpha -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($alpha -join '|')`" -abslog=`"$out\alpha.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7813?Path=frost?WRName=Bravo -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($bravo -join '|')`" -abslog=`"$out\bravo.log`"" -PassThru -WindowStyle Hidden
# Done once Bravo has been for its bag, Alpha has its last stat and the player autosave (every 10 s) holds the building perk.
function Saved { if (Test-Path $save) { try { (Get-Content $save -Raw | ConvertFrom-Json).lifeSkills } catch { $null } } }
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	$skills = Saved
	if ((Select-String -Path "$out\server.log" -Pattern 'CHAT .*socialtest-done' -Quiet) -and
		(Test-Path "$out\alpha.log") -and (Select-String -Path "$out\alpha.log" -Pattern 'NOTICE stat buildCost' -Quiet) -and $skills -and $skills.building -and $skills.building.level -eq 25) { break }
}
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' })
$a = @(Select-String -Path "$out\alpha.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ', '').Trim() })
$b = @(Select-String -Path "$out\bravo.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ', '').Trim() })
function Stat([string]$name) { $l = $a | Where-Object { $_ -match "^stat $name = (-?[\d.]+)" } | Select-Object -First 1; if ($l -and $l -match '= (-?[\d.]+)') { [double]$Matches[1] } else { $null } }
Check 'the server runs the PvP standard preset' (@($server | Where-Object { $_ -eq 'Server preset: pvp_standard' }).Count -ge 1)
$hits = @($a | Where-Object { $_ -like 'hit Bravo:*' })
Check 'PvP is on between strangers' ($hits.Count -ge 1 -and $hits[0] -eq 'hit Bravo: hit') ($hits -join '; ')
Check 'a clan is founded and joined' (($a -contains 'Founded clan Wolves') -and ($b -contains 'Joined clan Wolves') -and ($a -contains 'Clan Wolves (leader Alpha): Alpha, Bravo'))
Check 'PvP is off between clanmates' ($hits.Count -ge 2 -and $hits[1] -eq 'hit Bravo: ignored') ($hits -join '; ')
$bag = $server | Where-Object { $_ -match '^DEATH Bravo \(souls\): (\d+) stacks dropped' } | Select-Object -First 1
Check 'a death drops a bag of belongings (souls)' ($null -ne $bag) $bag
Check 'only its owner can open it' ($a -contains "Bravo's lost belongings: only they can recover them")
Check 'its owner gets everything back' (@($b | Where-Object { $_ -match '^Recovered everything \(\d+ stacks\)' }).Count -eq 1) (($b | Where-Object { $_ -match '^Recovered' }) -join '; ')
Check 'skill XP raises the level' (@($server | Where-Object { $_ -match '^SKILL \S+ mining reached level 36' }).Count -eq 1)
Check 'every level gives its stat (mining yield)' ((Stat 'miningYield') -gt 0) "miningYield $(Stat 'miningYield')"
Check 'perks are chosen at their tier' (($a -contains 'Mining perk: Double strike') -and ($a -contains 'Taming perk: Bonded') -and ($a -contains 'Building perk: Thrifty'))
Check 'a tier''s perk can only be chosen once' ($a -contains 'already chose that perk')
Check 'perks change their stats' ((Stat 'doubleOre') -gt 0 -and (Stat 'extraTames') -ge 1 -and (Stat 'buildCost') -lt 0) "doubleOre $(Stat 'doubleOre'), extraTames $(Stat 'extraTames'), buildCost $(Stat 'buildCost')"
$skills = Saved
Check 'life skills and perks are saved with the player' ($skills -and $skills.mining.level -eq 60 -and $skills.mining.perks[1] -eq 1 -and $skills.taming.perks[1] -eq 0 -and $skills.building.perks[0] -eq 0) $(if ($skills) { $skills | ConvertTo-Json -Compress } else { 'no save' })
$errors = @(Select-String -Path "$out\server.log", "$out\alpha.log", "$out\bravo.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
