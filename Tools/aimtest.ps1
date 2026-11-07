# Aiming: where spells go, and what lock-on will take.
#   In the arena: a bolt goes through the crosshair (not parallel to it from the chest); lock-on takes what's in the open,
#   nearest the crosshair, refuses what's behind a wall and lets go when a wall comes between; a target running sideways
#   gets hit when locked on (Hunter fires at Runner, a second player, on a PvP preset).
#   On the island, as the host, inside a real volcanic cave: its buried Kilnmites can't be locked on to.
#   aimtest.ps1            the test
#   aimtest.ps1 -NoAssist  the moving-target part with no lead and no homing, to see what they're worth (not a pass/fail)
param([switch]$NoAssist, [int]$Port = 7851)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-aim-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$env:UE_SKIP_UBT_SDK_SETUP = '1'
foreach ($w in 'aimtest', 'aimcave') { Remove-Item -Recurse -Force (Join-Path $root "Saved\Worlds\$w") -ErrorAction SilentlyContinue }

# ---- the arena
$tune = 'spells.fireBolt.damage:1,spells.fireBolt.burnBuildup:0,spells.meteor.damage:30,spells.meteor.burnBuildup:0,spells.meteor.cooldown:1'
if ($NoAssist) { $tune += ',spells.lockLead:0,spells.lockHoming:0,spells.lockHomingPlayers:0' }
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=aimtest -wrpreset=pvp_standard -wrtune=$tune `"-LogCmds=LogWildreach Verbose`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20

$hunter = @('WRBotPause 1', 'wait 3',
	# Through the crosshair: three dummies, each aimed at with the crosshair alone.
	# (The arena's Cinderfangs are 20 m from here: gone for the length of the test, or they eat the bot. Cleared from beside
	# them, more than 30 m from the dummies, which count as creatures too.)
	'WRTeleport -3500 0', 'wait 1', 'WRClearCreatures', 'wait 0.5', 'WRSay phase-crosshair', 'WRTeleport -1500 0', 'wait 3',
	'WRLookAt 0 0 100', 'wait 0.5', 'WRCast 1', 'wait 1.6', 'WRLookAt 0 500 100', 'wait 0.5', 'WRCast 1', 'wait 1.6', 'WRLookAt 0 -500 100', 'wait 0.5', 'WRCast 1', 'wait 1.6',
	'WRLookAt 0 0 60', 'wait 0.5', 'WRCast 1', 'wait 1.6', 'WRLookAt 0 500 60', 'wait 0.5', 'WRCast 1', 'wait 1.6', 'WRLookAt 0 -500 60', 'wait 0.5', 'WRCast 1', 'wait 1.6',
	# Lock-on and a wall: taken in the open, lost when a wall comes between, refused behind it, taken again once it's gone.
	# (Facing north, at the dummy on that side: the runner's lane is behind, so nothing else is in view.)
	'WRSay phase-wall', 'WRGive wood 100', 'WRTeleport 0 100', 'wait 3', 'WRLookAt 0 500 100', 'wait 0.5', 'WRLockOn', 'wait 1.5',
	'WRPlace palisade_wall 0 300 0 0', 'wait 4', 'WRLookAt 0 500 100', 'wait 0.5', 'WRLockOn', 'wait 1',
	'WRDemolish 0 300 0', 'wait 1.5', 'WRLookAt 0 500 100', 'wait 0.5', 'WRLockOn', 'wait 1', 'WRLockOn', 'wait 0.5',
	# A target running across at 15 m, locked on.
	'WRSay phase-moving', 'WRTeleport -300 -2200', 'wait 3', 'WRLookAt 1200 -2200 100', 'wait 0.5', 'WRLockOn', 'wait 1.5')
foreach ($i in 1..10) { $hunter += 'WRCast 1', 'wait 1.6' }
# A Meteor (quick slot 5, as in spelltest) takes 1.2 s to land: at a runner's pace it has to land ahead of it to hit.
$hunter += 'WRSay phase-meteor', 'WRGivePoints fire 120', 'WRAutoSpend fire 100', 'wait 2'
foreach ($i in 1..4) { $hunter += 'WRCast 5', 'wait 3.5' }
$hunter += 'wait 1', 'WRSay aimtest-done'
# Never quite reaches either end (5.3 s a leg at a run), so it's always running; healed so a Meteor's fire doesn't finish it.
# First the arena's Gloomhowls: gone for the length of the test, as the Cinderfangs are.
$runner = @('WRBotPause 1', 'wait 3', 'WRTeleport 3500 0', 'wait 1', 'WRClearCreatures', 'wait 0.5', 'WRTeleport 1200 -3400', 'wait 3')
foreach ($i in 1..16) { $runner += 'WRWalkTo 1200 -1000', 'wait 5', 'WRHeal', 'WRWalkTo 1200 -3400', 'wait 5', 'WRHeal' }
# (The hunter's client needs the tuning too: it runs the Meteor's cooldown itself.)
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=fire?WRName=Hunter -game -nullrhi -unattended -nosound -wrtune=$tune -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($hunter -join '|')`" -abslog=`"$out\hunter.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=frost?WRName=Runner -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($runner -join '|')`" -abslog=`"$out\runner.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(150)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path "$out\server.log" -Pattern 'CHAT .*aimtest-done' -Quiet)) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- a real cave on the island, as the HOST: a buried burrower is hidden with no collision, so it's never sent to a client
# that joins, but a host's game is the server and has them all. (Its colony stays buried: Kilnmites are neutral.)
$caveLog = "$out\cave.log"
if (-not $NoAssist) {
	# A volcanic cave (the west half of the island): from outside its mouth, walk into the chamber, and look at the colony.
	# Cave local space: the chamber is centred on its origin, the entrance opens toward local +X, the colony is at -300.
	$cave = @((Get-Content -Raw (Join-Path $root 'Data\island_dressing.json') | ConvertFrom-Json).caves) | Where-Object { $_.x -lt 0 } | Select-Object -First 1
	$a = $cave.yaw * [math]::PI / 180; $k = if ($cave.scale) { [double]$cave.scale } else { 1.0 }
	function CaveAt([double]$along) { '{0:F0} {1:F0}' -f ($cave.x + [math]::Cos($a) * $along * $k), ($cave.y + [math]::Sin($a) * $along * $k) }
	$cmds = @('WRBotPause 1', 'wait 3', "WRTeleport $(CaveAt 2200)", 'wait 6', 'WRHeal', "WRWalkTo $(CaveAt 300)", 'wait 16', 'WRHeal', "WRLookAt $(CaveAt -300) $($cave.z + 60)", 'wait 1', 'WRLockOn', 'wait 1', 'WRSay aimcave-done')
	$c = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=Caver?Path=fire -game -nullrhi -unattended -nosound -port=$($Port + 1) -wrworld=aimcave -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$caveLog`"" -PassThru -WindowStyle Hidden
	$deadline = (Get-Date).AddSeconds(130)
	# (The log may not exist yet: Select-String on a missing file would end this loop at once.)
	while ((Get-Date) -lt $deadline -and -not ((Test-Path $caveLog) -and (Select-String -Path $caveLog -Pattern 'chat Caver: aimcave-done|CHAT .*aimcave-done' -Quiet))) { Start-Sleep -Seconds 3 }
	Start-Sleep -Seconds 1
	if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
}
Start-Sleep -Seconds 2

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Clean($line) { ($line -replace '^.*LogWildreach: (Verbose: )?', '').Trim() }
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { Clean $_.Line })
$hunterLog = @(Select-String -Path "$out\hunter.log" -Pattern 'LogWildreach: ' | ForEach-Object { Clean $_.Line })
function Phase([string[]]$lines, [string]$from, [string]$to) {
	$a = -1; $b = $lines.Count
	for ($i = 0; $i -lt $lines.Count; $i++) { if ($a -lt 0 -and $lines[$i] -match "CHAT .*$from") { $a = $i } elseif ($a -ge 0 -and $lines[$i] -match "CHAT .*$to") { $b = $i; break } }
	if ($a -lt 0) { @() } else { @($lines[$a..($b - 1)]) }
}
function Casts($lines) { @($lines | Where-Object { $_ -match 'ABILITY \S+ used WRGA_FireBolt' }).Count }
$crosshair = Phase $server 'phase-crosshair' 'phase-wall'
$dummyHits = @($crosshair | Where-Object { $_ -match '^COMBAT WRCharacter_\d+ -> WRTrainingDummy' }).Count
if (-not $NoAssist) { Check 'a bolt goes through the crosshair at 15 m' ($dummyHits -ge 5 -and (Casts $crosshair) -ge 5) "$dummyHits hits from $(Casts $crosshair) casts, each dummy aimed at with the crosshair alone" }
$moving = Phase $server 'phase-moving' 'phase-meteor'
$meteorPhase = Phase $server 'phase-meteor' 'aimtest-done'
$meteorCasts = @($meteorPhase | Where-Object { $_ -match 'ABILITY \S+ used WRGA_Meteor' }).Count
$meteorHits = @($meteorPhase | Where-Object { $_ -match '^COMBAT WRCharacter_\d+ -> WRCharacter_\d+ dmg=(\d+)' -and [int]$Matches[1] -ge 20 }).Count
$hits = @($moving | Where-Object { $_ -match '^COMBAT WRCharacter_\d+ -> WRCharacter_\d+' -and $_ -match 'Element.Fire' }).Count
$casts = Casts $moving
$locks = Phase $hunterLog 'phase-wall' 'phase-buried'
$lockLines = @($locks | Where-Object { $_ -match '^LOCK ' })
if ($NoAssist) {
	"WITHOUT lead or homing: $hits hits from $casts casts at a target running across, Meteor $meteorHits hits from $meteorCasts casts"
} else {
	# (It turns back at each end of its lane, and a bolt takes over half a second: not every one. Without the lead: 1 in 10.)
	Check 'a locked-on bolt hits a target running across at 15 m' ($casts -ge 6 -and $hits -ge [math]::Ceiling($casts * 0.6)) "$hits hits from $casts casts"
	# (It can turn back during the 1.2 s a Meteor takes, so not all of them; without the lead it's gone by then, as -NoAssist shows.)
	Check 'a locked-on Meteor lands on it too' ($meteorCasts -ge 3 -and $meteorHits -ge 2) "$meteorHits hits from $meteorCasts casts"
	Check 'lock-on takes what''s in the open' ($lockLines[0] -match '^LOCK on WRTrainingDummy') "$($lockLines[0])"
	Check 'a wall coming between lets go' (@($lockLines | Where-Object { $_ -eq 'LOCK lost: out of sight' }).Count -ge 1) (($lockLines | Select-Object -Skip 1 -First 1))
	Check 'behind a wall it won''t lock on' (@($lockLines | Where-Object { $_ -match '^LOCK nothing to lock on to \(0 buried, 1 out of sight\)' }).Count -ge 1) (($lockLines | Select-Object -Skip 2 -First 1))
	Check 'the wall gone, it locks on again' (@($lockLines | Where-Object { $_ -match '^LOCK on WRTrainingDummy' }).Count -ge 2) (($lockLines | Select-Object -Skip 3 -First 1))
	$c = @(Select-String -Path $caveLog -Pattern 'LOCK ' -ErrorAction SilentlyContinue | ForEach-Object { Clean $_.Line } | Where-Object { $_ -match '^LOCK ' }) | Select-Object -First 1
	Check 'as host, in a real cave: its buried colony can''t be locked on to' ($c -match '^LOCK nothing to lock on to \([1-9]\d* buried') "$c"
	$errors = @(Select-String -Path "$out\server.log", "$out\hunter.log", "$out\runner.log", $caveLog -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
	Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
}
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
