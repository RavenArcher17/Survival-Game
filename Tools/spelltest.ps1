# Tree spells + the Paths menu's learn command, on the arena's training dummies.
# Pyro (fire) tries a branch node before the trunk (refused), learns a trunk node via the menu command, then auto-spends
# into Fire wall, Lava pool, Meteor, Molten edge and Molten cleave; Cryo (frost) into Ice lance, Frost nova, Ice wall,
# Blizzard and Flurry. Each casts its quick slots 3-7 at its own dummy; the server confirms every cast.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-spell-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Pyro.json'), (Join-Path $db 'players\Cryo.json'), (Join-Path $db 'world_spelltest.json') -ErrorAction SilentlyContinue

$pyro = @('WRBotPause 1', 'WRGivePoints fire 120', 'WRLearn fire spreading_flames', 'WRLearn fire fire_bolt', 'WRAutoSpend fire 100', 'wait 2',
	'WRTeleport -400 -500', 'wait 2', 'WRLook 0 -20', 'WRCast 3', 'wait 3', 'WRCast 4', 'wait 3', 'WRCast 5', 'wait 3', 'WRLook 0 0', 'WRCast 6', 'wait 2', 'WRCast 7', 'wait 4',
	'WRSay spelltest-done') -join '|'
$cryo = @('WRBotPause 1', 'WRGivePoints frost 120', 'WRAutoSpend frost 100', 'wait 2',
	'WRTeleport -400 500', 'wait 2', 'WRLook 0 0', 'WRCast 3', 'wait 2', 'WRCast 4', 'wait 2', 'WRCast 5', 'wait 2', 'WRCast 6', 'wait 3', 'WRCast 7', 'wait 3',
	'WRSay spelltest-done') -join '|'

# Verbose: the server logs every ability it confirms (ABILITY <who> used <class>).
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7823 -wrworld=spelltest `"-LogCmds=LogWildreach Verbose`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7823?Path=fire?WRName=Pyro -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$pyro`" -abslog=`"$out\pyro.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7823?Path=frost?WRName=Cryo -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cryo`" -abslog=`"$out\cryo.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(75)
while ((Get-Date) -lt $deadline -and @(Select-String -Path "$out\server.log" -Pattern 'CHAT .*spelltest-done').Count -lt 2) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' })
$pyroNotices = @(Select-String -Path "$out\pyro.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ', '').Trim() })
Check 'a branch node waits for the trunk' ($pyroNotices -contains 'finish the trunk first')
Check 'the Paths menu''s learn command learns a node' ($pyroNotices -contains 'Learned Fire bolt') (($pyroNotices | Where-Object { $_ -notlike 'chat *' } | Select-Object -First 2) -join '; ')
foreach ($node in 'fire.fire_wall', 'fire.lava_pool', 'frost.ice_lance', 'frost.frost_nova') {
	Check "bought $node" (@($server | Where-Object { $_ -match "^PATH bought $([regex]::Escape($node)) " }).Count -ge 1)
}
$used = @($server | ForEach-Object { if ($_ -match 'ABILITY \S+ used (\S+)') { $Matches[1] } })   # "Verbose: ABILITY ..."
# Slots 3-7 hold every tree spell each path has built.
foreach ($spell in 'WRGA_FireWall', 'WRGA_LavaPool', 'WRGA_Meteor', 'WRGA_MoltenEdge', 'WRGA_MoltenCleave', 'WRGA_IceLance', 'WRGA_FrostNova', 'WRGA_IceWall', 'WRGA_Blizzard', 'WRGA_Flurry') {
	Check "cast $($spell -replace 'WRGA_', '')" ($used -contains $spell) "$(@($used | Where-Object { $_ -eq $spell }).Count) casts"
}
$combat = @($server | Where-Object { $_ -match '^COMBAT WRCharacter_\d+ -> WRTrainingDummy' })
$fire = @($combat | Where-Object { $_ -match 'Element.Fire' }).Count; $frost = @($combat | Where-Object { $_ -match 'Element.Frost' }).Count
Check 'fire spells hit the dummies' ($fire -gt 0) "$fire hits"
Check 'frost spells hit the dummies' ($frost -gt 0) "$frost hits"
$errors = @(Select-String -Path "$out\server.log", "$out\pyro.log", "$out\cryo.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
