# Boss test on the arena with two players. Hero fights every boss in turn (dev damage commands
# stand in for a long fight); Helper joins the Vulkarn fight and deals ~3%, so Vulkarn scales to
# two fighters and Helper gets no loot (10% rule). Checks phases, attacks landing, floods/anvils,
# wing grounding, the Warden's shell, Ignessa's wisps, the Pale Hunter's blizzard, loot and boss parts.
param([int]$Seconds = 240)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-boss-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Hero.json'), (Join-Path $db 'players\Helper.json'), (Join-Path $db 'world_bosstest.json') -ErrorAction SilentlyContinue
$tune = '-wrtune=bosses.damageScale:0.05,bosses.partChance:1'

$fight = { param($id, $extra) @("WRSpawn $id 1", 'wait 7', 'WRHeal', 'WRBossDmgPct 30 none', 'wait 3', 'WRBossDmgPct 25 none', 'wait 6') + $extra + @('WRHeal', 'WRBossDmg 9999999 none', 'wait 8') }
$hero = @('WRBotPause 1', 'WRTeleport 0 4200', 'wait 2', 'WRSpawn vulkarn 1', 'wait 14', 'WRHeal', 'WRBossDmgPct 30 frost', 'wait 3', 'WRBossDmgPct 25 none', 'wait 8',
	'WRHeal', 'WRArtifacts 1', 'WRBossDmgPct 30 none', 'wait 6', 'WRHeal', 'WRBossDmg 9999999 none', 'wait 8', 'WRArtifacts 0')
$hero += & $fight 'skaldrith' @('WRBossDmg 10 fire', 'WRBossDmg 10 fire', 'WRBossDmg 10 fire', 'wait 3')
$hero += & $fight 'obsidian_warden' @('WRBossDmgPct 30 frost', 'WRBossDmgPct 30 frost', 'wait 2')
$hero += & $fight 'ignessa' @('wait 6')
$hero += & $fight 'grimjaw' @('wait 4', 'WRBossDmgPct 0 none')
$hero += & $fight 'pale_hunter' @('wait 6', 'WRBossDmg 10 fire')
$helper = @('WRBotPause 1', 'WRTeleport 300 4200', 'wait 12', 'WRBossDmgPct 3 none')

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7812 -wrworld=bosstest $tune -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7812?Path=fire?WRName=Hero -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($hero -join '|')`" -abslog=`"$out\hero.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7812?Path=frost?WRName=Helper -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($helper -join '|')`" -abslog=`"$out\helper.log`"" -PassThru -WindowStyle Hidden
# The last boss down (and its loot notice) ends the run early.
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline -and -not ((Test-Path "$out\hero.log") -and (Select-String -Path "$out\hero.log" -Pattern 'NOTICE The Pale Hunter defeated' -Quiet))) { Start-Sleep -Seconds 4 }
Start-Sleep -Seconds 2
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$boss = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: BOSS ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: BOSS ', '' })
function Has([string]$pattern) { @($boss | Where-Object { $_ -match $pattern }).Count -ge 1 }
$helperNotices = @(Select-String -Path "$out\helper.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ', '').Trim() })
$bosses = [ordered]@{
	'Vulkarn, the Forge Giant' = 'core_vulkarn'; 'Skaldrith, the Frost Wyrm' = 'core_skaldrith'; 'The Obsidian Warden' = 'trophy_obsidian_heart'
	'Ignessa, Queen of Wisps' = 'trophy_wisp_crown'; 'Grimjaw' = 'trophy_troll_marrow'; 'The Pale Hunter' = 'trophy_stalker_fang'
}
foreach ($name in $bosses.Keys) {
	$e = [regex]::Escape($name)
	$attacks = @($boss | ForEach-Object { if ($_ -match "^$e (\S+) lands: [1-9]") { $Matches[1] } } | Sort-Object -Unique)
	$loot = $boss | Where-Object { $_ -match '^loot: Hero dealt' } | Where-Object { $_ -match $bosses[$name] } | Select-Object -First 1
	Check "$name`: fights with at least two attacks that land, is defeated, drops its loot" ($attacks.Count -ge 2 -and (Has "^$e defeated") -and $loot) "attacks: $($attacks -join ', ')"
}
Check 'Vulkarn scales to two fighters' (Has '^Vulkarn, the Forge Giant scaled to 2 participants')
Check 'Vulkarn goes through phases 2 and 3' ((Has '^Vulkarn, the Forge Giant enters phase 2') -and (Has '^Vulkarn, the Forge Giant enters phase 3'))
Check 'Vulkarn floods the floor and its anvils shatter' ((Has 'floods the floor') -and (Has '\(anvil\) shattered'))
Check 'a helper under 10% gets no loot, and is told so' ((Has '^loot: Helper dealt .* - not enough') -and @($helperNotices | Where-Object { $_ -match 'need 10% for loot' }).Count -ge 1)
Check 'boss parts drop (partChance 1 in this test)' ((Has 'part_giants_ember') -and (Has 'part_wyrm_scale'))
Check 'Skaldrith is grounded by wing hits and its ice pillars shatter' ((Has '^Skaldrith, the Frost Wyrm grounded by wing hits') -and (Has '\(pillar\) shattered'))
Check 'the Obsidian Warden''s shell breaks' (Has '^The Obsidian Warden shell broken')
Check 'Ignessa sends wisps, then lands when they''re gone' ((Has 'sends \d+ wisps') -and (Has 'out of wisps and lands'))
Check 'the Pale Hunter vanishes into the blizzard and shows again' ((Has 'Pale Hunter vanishes into the blizzard') -and (Has 'Pale Hunter is visible'))
$errors = @(Select-String -Path "$out\server.log", "$out\hero.log", "$out\helper.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
