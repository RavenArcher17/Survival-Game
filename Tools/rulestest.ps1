# A world's settings really change the game (the screen for them is in worldstest): the island is played twice with a
# headless server and a scripted player, once on PvE as it comes and once with a settings.json that changes the new
# rules, and the two are compared: the clock, how fast hunger and thirst drop, how many wildlife packs
# there are, a creature's and a boss's health and damage. A third run has more creature numbers than usual.
#   rulestest.ps1 [-Port 7887]
param([int]$Port = 7887)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-rules-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $logs | Out-Null
$world = 'rulestest'
$db = Join-Path $root "Saved\Worlds\$world"
$name = 'Ruler'

$bot = @(
	'WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 3',
	'WRVitals', 'wait 20', 'WRVitals',
	'WRSpawn cinderfang 1', 'wait 1', 'WRSpawn ignessa 1', 'wait 1', 'WRSay rules-done'
)

# One run: a world with this settings.json, and what the logs say about it.
function Play([string]$tag, [string]$settings) {
	Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
	New-Item -ItemType Directory -Force $db | Out-Null
	[IO.File]::WriteAllText((Join-Path $db 'settings.json'), $settings)
	$slog = "$logs\$tag-server.log"; $clog = "$logs\$tag-client.log"
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=$world -abslog=`"$slog`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 25
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=$name -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($bot -join '|')`" -abslog=`"$clog`"" -PassThru -WindowStyle Hidden
	$deadline = (Get-Date).AddSeconds(150)
	while ((Get-Date) -lt $deadline) {
		Start-Sleep -Seconds 3
		if (Select-String -Path $slog -Pattern 'CHAT .*rules-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 1; break }
	}
	foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
	Start-Sleep -Seconds 2
	$lines = @(Select-String -Path $slog -Pattern 'LogWildreach: ' -ErrorAction SilentlyContinue | ForEach-Object { ($_.Line -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
	$v = @($lines | Where-Object { $_ -match '^VITALS' } | ForEach-Object { if ($_ -match 'hunger ([\d.]+) thirst ([\d.]+), clock ([\d.]+) h, at ([\d.]+) s') { [pscustomobject]@{ Hunger = [double]$Matches[1]; Thirst = [double]$Matches[2]; Clock = [double]$Matches[3]; At = [double]$Matches[4] } } })
	$r = [ordered]@{ Packs = 0; Animals = 0; Share = 0; HungerPerS = 0.0; ThirstPerS = 0.0; ClockPerS = 0.0; CreatureHp = 0.0; CreatureDamage = 0.0; BossHp = 0.0; Client = '' }
	foreach ($l in $lines) {
		if ($l -match "^WILDLIFE (\d+) packs, (\d+) animals, new; the island's share is (\d+)") { $r.Packs = [int]$Matches[1]; $r.Animals = [int]$Matches[2]; $r.Share = [int]$Matches[3] }
		if ($l -match '^SPAWN cinderfang: ([\d.]+) hp, ([\d.]+) damage') { $r.CreatureHp = [double]$Matches[1]; $r.CreatureDamage = [double]$Matches[2] }
		if ($l -match '^BOSS Ignessa, Queen of Wisps ready \(mini, ([\d.]+) hp solo\)') { $r.BossHp = [double]$Matches[1] }
	}
	if ($v.Count -ge 2) {
		$t = [Math]::Max(1.0, $v[1].At - $v[0].At)
		$r.HungerPerS = ($v[0].Hunger - $v[1].Hunger) / $t
		$r.ThirstPerS = ($v[0].Thirst - $v[1].Thirst) / $t
		$r.ClockPerS = ($v[1].Clock - $v[0].Clock) / $t
	}
	$fromHost = @(Select-String -Path $clog -Pattern 'Server settings from host: ' -ErrorAction SilentlyContinue | Select-Object -Last 1)
	if ($fromHost.Count) { $r.Client = $fromHost[0].Line }
	"--- $tag : " + (($r.Keys | Where-Object { $_ -ne 'Client' } | ForEach-Object { "$_ $($r[$_])" }) -join ', ')
	$script:runs[$tag] = $r
}

$script:runs = @{}
Play 'plain' '{"preset":"pve"}'
Play 'changed' '{"preset":"custom","base":"pve","settings":{"dayMinutes":20,"hungerMultiplier":2,"creatureNumbers":0.5,"creatureDifficulty":2}}'
Play 'more' '{"preset":"custom","base":"pve","settings":{"creatureNumbers":2}}'
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Near([double]$a, [double]$b, [double]$slack = 0.1) { $b -ne 0 -and [Math]::Abs($a / $b - 1) -le $slack }
$p = $script:runs['plain']; $c = $script:runs['changed']; $m = $script:runs['more']
Check 'a third of the day length makes the clock three times as fast' (Near $c.ClockPerS (3 * $p.ClockPerS)) ("{0:N5} vs {1:N5} h/s" -f $c.ClockPerS, $p.ClockPerS)
Check 'twice the hunger and thirst drop twice as fast' ((Near $c.HungerPerS (2 * $p.HungerPerS)) -and (Near $c.ThirstPerS (2 * $p.ThirstPerS))) ("hunger {0:N4} vs {1:N4} /s" -f $c.HungerPerS, $p.HungerPerS)
Check 'half the creature numbers: half the packs and half the share of animals' ((Near $c.Packs ($p.Packs / 2) 0.02) -and (Near $c.Share ($p.Share / 2) 0.02)) "$($c.Packs) packs of $($p.Packs), share $($c.Share) of $($p.Share)"
Check 'twice the creature numbers: twice the packs and twice the share' ((Near $m.Packs (2 * $p.Packs) 0.02) -and (Near $m.Share (2 * $p.Share) 0.02)) "$($m.Packs) packs, share $($m.Share), from $($p.Packs), $($p.Share)"
Check 'twice the difficulty: a creature has twice the health and damage' ((Near $c.CreatureHp (2 * $p.CreatureHp) 0.01) -and (Near $c.CreatureDamage (2 * $p.CreatureDamage) 0.01)) "$($c.CreatureHp) hp, $($c.CreatureDamage) damage, from $($p.CreatureHp), $($p.CreatureDamage)"
Check 'and a boss has twice the health' (Near $c.BossHp (2 * $p.BossHp) 0.01) "$($c.BossHp) from $($p.BossHp)"
Check 'the player is told the world''s settings' ($c.Client -match 'custom \(' -and $c.Client -match 'server\.dayMinutes=20' -and $c.Client -match 'server\.creatureDifficulty=2')
$allLogs = @(Get-ChildItem $logs -Filter '*.log' | ForEach-Object { $_.FullName })
$errors = @(Select-String -Path $allLogs -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
