# Combat feel on the arena (0.5.0 feedback pass): the starter spell, attacking out of a held block, creature wind-ups.
# Vanguard, a new frost survivor (a windowed client, so the wheel key goes through the real UI), starts with Frost shard
# on wheel 1 and casts it twice with a tap of Q, which earns its first path point. Then it holds block at a Cinderfang
# and swings light and heavy without letting go: the guard drops for each swing and is back up when it ends, and still
# blocks. Every creature attack must land only after its wind-up (the server's own log times), and the client shows
# each wind-up (TELEGRAPH lines). -Shots also frames a Cinderfang, Basaltback and Emberhorn winding up from two sides,
# by day and by night (Saved\Screenshots\WindowsEditor\telegraph_*).
param([int]$Port = 7827, [switch]$Shots)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-combat-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Vanguard.json'), (Join-Path $db 'world_combattest.json') -ErrorAction SilentlyContinue

function Hold($seconds) { (1..[math]::Ceiling($seconds) | ForEach-Object { 'wait 1|WRHeal' }) -join '|' }   # wait, staying alive
function Face($seconds) { (1..[math]::Ceiling($seconds * 2) | ForEach-Object { 'WRAimAt cinderfang|wait 0.5' }) -join '|' }  # keep the guard toward it
$cmds = @(
	'WRBotPause 1', 'wait 3', 'WRClearCreatures', 'WRLook 0 0', 'wait 1',
	# The starter spell: a tap of the wheel key casts what wheel 1 has picked.
	'WRKey Q', 'wait 1.5', 'WRKey Q', 'wait 1',
	# Block, then attack while still holding it.
	'WRSpawn cinderfang 1', 'wait 0.3', 'WRAimAt cinderfang', 'WRPress block 10', (Face 2.5), 'WRHeal',
	'WRPress light', (Face 2), 'WRHeal', 'WRPress heavy', (Face 2.5), 'WRHeal', (Face 2.5), 'WRHeal', 'wait 1',
	'WRClearCreatures', 'wait 0.5'
)
if ($Shots) {
	# Each wind-up from its side and from your own camera (or its other side): frames every 0.1 s while it attacks you.
	foreach ($hour in 12, 22) {
		$cmds += @("WRHour $hour", 'wait 2',
			'WRSpawn cinderfang 1', 'wait 0.3', 'WRArtCam cinderfang 90', 'wait 1', "WRShotSeq telegraph_cinder_${hour}_side_ 30 0.1", (Hold 4),
			'WRArtCam none', "WRShotSeq telegraph_cinder_${hour}_player_ 30 0.1", (Face 4), 'WRHeal', 'WRClearCreatures', 'wait 0.5')
	}
	# Big ones, provoked by an arrow: a Basaltback stomps (ring), an Emberhorn charges (line).
	$cmds += @('WRHour 12', 'wait 1', 'WRGive hunting_bow 1', 'WRGive stone_arrow 20', 'WREquip hunting_bow', 'wait 0.5')
	foreach ($beast in 'basaltback', 'emberhorn') {
		$cmds += @("WRSpawn $beast 1", 'wait 0.3', 'WRRanged 1', 'wait 0.5', 'WRAim 1', 'wait 0.5', "WRAimAt $beast", 'wait 0.1', 'WRFire', 'wait 0.3', 'WRAim 0', 'WRRanged 0',
			"WRArtCam $beast 90", "WRShotSeq telegraph_${beast}_side_ 40 0.1", (Hold 4), "WRArtCam $beast -70", "WRShotSeq telegraph_${beast}_other_ 40 0.1", (Hold 4),
			'WRArtCam none', 'WRClearCreatures', 'wait 0.5')
	}
}
$cmds += 'WRSay combattest-done'
$cmds = $cmds -join '|'

# Verbose: the server logs abilities, bites and blocks; the client its telegraphs. Wheel 1 starts empty (a fresh install).
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=combattest `"-LogCmds=LogWildreach Verbose`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$ini = '[Wildreach.UI]:Wheel1=,[Wildreach.UI]:Wheel2=,[Wildreach.UI]:WheelPicked1=0,[Wildreach.UI]:StarterWheeled='
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=frost?WRName=Vanguard -game -windowed -ResX=1280 -ResY=720 -unattended -nosound -wrbot `"-ini:GameUserSettings:$ini`" `"-ExecCmds=t.MaxFPS 60`" `"-LogCmds=LogWildreach Verbose`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru
$deadline = (Get-Date).AddSeconds($(if ($Shots) { 200 } else { 90 }))
while ((Get-Date) -lt $deadline -and @(Select-String -Path "$out\server.log" -Pattern 'CHAT .*combattest-done' -ErrorAction SilentlyContinue).Count -lt 1) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Lines($path) { @(Select-String -Path $path -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line }) }
function Msg($line) { $line -replace '^.*LogWildreach: (Verbose: )?', '' }
function Stamp($line) { if ($line -match '^\[(\d+)\.(\d+)\.(\d+)-(\d+)\.(\d+)\.(\d+):(\d+)\]') { [datetime]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4], [int]$Matches[5], [int]$Matches[6], [int]$Matches[7]) } }
$server = Lines "$out\server.log"
$client = Lines "$out\client.log"
$msgs = @($server | ForEach-Object { Msg $_ })

# The starter spell.
Check 'a new survivor starts with Frost shard' (@($msgs | Where-Object { $_ -match '^PATH starter frost\.frost_shard for Vanguard$' }).Count -ge 1)
Check 'at level 1, in a quick slot' (@($msgs | Where-Object { $_ -match '^PATH apply .* main=frost\(0\) .*level=1 abilities=1 ' }).Count -ge 1)
Check 'it goes on wheel 1' (@($client | Where-Object { (Msg $_) -match '^UI starter frost\.frost_shard on wheel 1 wedge 0' }).Count -ge 1)
$shards = @($msgs | Where-Object { $_ -match '^ABILITY \S+ used WRGA_FrostShard' }).Count
Check 'a tap of Q casts it' ($shards -ge 2) "$shards casts"
Check 'and it earns path XP' (@($msgs | Where-Object { $_ -match '^PATH frost earned a point \(1 earned' }).Count -ge 1)

# Attacking out of a held block.
Check 'a light attack comes straight out of a held block' (@($msgs | Where-Object { $_ -match '^BLOCK \S+ swings from the guard \(WRGA_LightAttack\)' }).Count -ge 1)
Check 'so does a heavy one' (@($msgs | Where-Object { $_ -match '^BLOCK \S+ swings from the guard \(WRGA_HeavyAttack\)' }).Count -ge 1)
$back = @($server | Where-Object { (Msg $_) -match '^BLOCK \S+ swing done: block still held, guard back up' })
Check 'the guard is back up when each swing ends' ($back.Count -ge 2) "$($back.Count) of 2"
$blocked = @()
if ($back.Count -gt 0) {
	$after = Stamp $back[0]
	$blocked = @($server | Where-Object { (Msg $_) -match '^COMBAT WRCreature_\d+ -> WRCharacter_\d+ .*result=Blocked' -and (Stamp $_) -gt $after })
}
Check 'and still blocks after a swing' ($blocked.Count -ge 1) "$($blocked.Count) blocked bites after the first swing"

# Creature attacks land only after their wind-up (bite 0.5 s, lunge 0.6 s in tuning).
$windupAt = @{}; $windupKind = @{}; $gaps = @()
foreach ($line in $server) {
	$m = Msg $line
	if ($m -match '^AI (WRCreature_\d+) winds up (\w+)') { $windupAt[$Matches[1]] = Stamp $line; $windupKind[$Matches[1]] = $Matches[2] }
	elseif ($m -match '^COMBAT (WRCreature_\d+) -> WRCharacter_\d+ ' -or $m -match '^AI (WRCreature_\d+) \w+ hits WRCharacter_\d+') {
		$who = $Matches[1]
		if ($windupAt.ContainsKey($who)) { $gaps += [pscustomobject]@{ kind = $windupKind[$who]; s = ((Stamp $line) - $windupAt[$who]).TotalSeconds } }
		else { $gaps += [pscustomobject]@{ kind = 'none'; s = -1 } }
	}
}
$early = @($gaps | Where-Object { $_.s -lt 0.45 })
$bites = @($gaps | Where-Object { $_.kind -eq 'bite' })
Check 'creature attacks land only after a wind-up' ($gaps.Count -ge 2 -and $early.Count -eq 0) ("{0} hits, shortest wind-up {1:N2} s{2}" -f $gaps.Count, $(if ($gaps.Count) { ($gaps | Measure-Object s -Minimum).Minimum } else { 0 }), $(if ($early.Count) { " ($($early[0].kind))" } else { '' }))
Check 'bites wind up for about half a second' ($bites.Count -ge 1 -and @($bites | Where-Object { $_.s -lt 0.48 -or $_.s -gt 0.75 }).Count -eq 0) "$($bites.Count) bites"
$tele = @($client | Where-Object { (Msg $_) -match '^TELEGRAPH WRCreature_\d+ \w+ wind-up' })
Check 'the client shows each wind-up' ($tele.Count -ge 2) "$($tele.Count) telegraphs"
if ($Shots) {
	$moves = @($tele | ForEach-Object { if ((Msg $_) -match ' (\w+) wind-up') { $Matches[1] } } | Sort-Object -Unique)
	Check 'telegraphs for bite, lunge, stomp and charge' (@('bite', 'lunge', 'stomp', 'charge' | Where-Object { $moves -notcontains $_ }).Count -eq 0) ($moves -join ', ')
}

$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
