# Tree spells + the Paths menu's learn command, on the arena's training dummies.
# Both are new survivors: each starts with its path's starter spell (Fire bolt, Frost shard) in quick slot 1 and casts it
# twice, which earns its first path point. Then Pyro (fire) tries a branch node before the trunk (refused), learns a trunk
# node via the menu command, then auto-spends into Fire wall, Lava pool, Meteor, Molten edge and Molten cleave; Cryo
# (frost) into Ice lance, Frost nova, Ice wall, Blizzard and Flurry. Each casts its quick slots 3-7 at its own dummy; the
# server confirms every cast.
param([int]$Port = 7823)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-spell-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Pyro.json'), (Join-Path $db 'players\Cryo.json'), (Join-Path $db 'world_spelltest.json') -ErrorAction SilentlyContinue

$pyro = @('WRBotPause 1', 'wait 1', 'WRCast 1', 'wait 1.6', 'WRCast 1', 'wait 1', 'WRGivePoints fire 120', 'WRLearn fire spreading_flames', 'WRLearn fire heat_capacity', 'WRAutoSpend fire 100', 'wait 2',
	'WRTeleport -400 -500', 'wait 2', 'WRLook 0 -20', 'WRCast 3', 'wait 3', 'WRCast 4', 'wait 3', 'WRCast 5', 'wait 3', 'WRLook 0 0', 'WRCast 6', 'wait 2', 'WRCast 7', 'wait 4',
	'WRSay spelltest-done') -join '|'
$cryo = @('WRBotPause 1', 'wait 1', 'WRCast 1', 'wait 1.4', 'WRCast 1', 'wait 1', 'WRGivePoints frost 120', 'WRAutoSpend frost 100', 'wait 2',
	'WRTeleport -400 500', 'wait 2', 'WRLook 0 0', 'WRCast 3', 'wait 2', 'WRCast 4', 'wait 2', 'WRCast 5', 'wait 2', 'WRCast 6', 'wait 3', 'WRCast 7', 'wait 3',
	'WRSay spelltest-done') -join '|'

# Verbose: the server logs every ability it confirms (ABILITY <who> used <class>).
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=spelltest `"-LogCmds=LogWildreach Verbose`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=fire?WRName=Pyro -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$pyro`" -abslog=`"$out\pyro.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=frost?WRName=Cryo -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cryo`" -abslog=`"$out\cryo.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(85)
while ((Get-Date) -lt $deadline -and @(Select-String -Path "$out\server.log" -Pattern 'CHAT .*spelltest-done').Count -lt 2) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' })
$pyroNotices = @(Select-String -Path "$out\pyro.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ', '').Trim() })
# Starter spells: a new survivor owns its path's first spell, it's in quick slot 1, and using it earns path XP.
foreach ($who in @(@('Pyro', 'fire', 'fire_bolt', 'WRGA_FireBolt'), @('Cryo', 'frost', 'frost_shard', 'WRGA_FrostShard'))) {
	Check "$($who[0]) starts with $($who[2])" (@($server | Where-Object { $_ -match "^PATH starter $($who[1])\.$($who[2]) for $($who[0])$" }).Count -ge 1)
	$firstApply = @($server | Where-Object { $_ -match "^PATH apply .* main=$($who[1])\(0\) .*abilities=1 " }).Count
	Check "$($who[0])'s starter is granted at level 1" ($firstApply -ge 1)
	Check "$($who[0]) earns path XP with it" (@($server | Where-Object { $_ -match "^PATH $($who[1]) earned a point \(1 earned" }).Count -ge 1)
}
Check 'a branch node waits for the trunk' ($pyroNotices -contains 'finish the trunk first')
Check 'the Paths menu''s learn command learns a node' ($pyroNotices -contains 'Learned Heat capacity') (($pyroNotices | Where-Object { $_ -notlike 'chat *' } | Select-Object -First 2) -join '; ')
foreach ($node in 'fire.fire_wall', 'fire.lava_pool', 'frost.ice_lance', 'frost.frost_nova') {
	Check "bought $node" (@($server | Where-Object { $_ -match "^PATH bought $([regex]::Escape($node)) " }).Count -ge 1)
}
$used = @($server | ForEach-Object { if ($_ -match 'ABILITY \S+ used (\S+)') { $Matches[1] } })   # "Verbose: ABILITY ..."
foreach ($spell in 'WRGA_FireBolt', 'WRGA_FrostShard') { Check "cast the starter $($spell -replace 'WRGA_', '')" ($used -contains $spell) "$(@($used | Where-Object { $_ -eq $spell }).Count) casts" }
# Slots 3-7 hold every tree spell each path has built.
foreach ($spell in 'WRGA_FireWall', 'WRGA_LavaPool', 'WRGA_Meteor', 'WRGA_MoltenEdge', 'WRGA_MoltenCleave', 'WRGA_IceLance', 'WRGA_FrostNova', 'WRGA_IceWall', 'WRGA_Blizzard', 'WRGA_Flurry') {
	Check "cast $($spell -replace 'WRGA_', '')" ($used -contains $spell) "$(@($used | Where-Object { $_ -eq $spell }).Count) casts"
}
$combat = @($server | Where-Object { $_ -match '^COMBAT WRCharacter_\d+ -> WRTrainingDummy' })
$fire = @($combat | Where-Object { $_ -match 'Element.Fire' }).Count; $frost = @($combat | Where-Object { $_ -match 'Element.Frost' }).Count
Check 'fire spells hit the dummies' ($fire -gt 0) "$fire hits"
Check 'frost spells hit the dummies' ($frost -gt 0) "$frost hits"
$errors = @(Select-String -Path "$out\server.log", "$out\pyro.log", "$out\cryo.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
