# Two players at one base (0.7 polish): Mason hosts L_Island (no window) and builds a base in the volcanic lowland;
# Bravo joins in a 1024x768 window (4:3) and plays it by keys and clicks. The two keep in step by chat ("waitchat").
#  1. A shared forge: Bravo orders 8 copper ingots by clicks; Mason orders 2 swords behind it. Bravo's strip shows
#     Mason's order as "Mason's order" with no Cancel (Bravo isn't in his clan yet); after Bravo joins the clan, Cancel is
#     on both. Bravo's ingots come into Bravo's bag; Mason's swords wait their turn and go to the tray (he walked off).
#  2. A raid on the base: Bravo, Mason's clanmate, gets the warning card (with no notice under it), then the raid
#     tracker under the level plate (coming, then "5 of 5 raiders left"), and "Raid over" when Mason kills them.
# -Shots keeps Bravo's pictures.
#   cooptest.ps1 [-Port 7896] [-Shots] [-Out <folder>]
param([int]$Port = 7896, [switch]$Shots, [string]$Out = '')
$X = -132000; $Y = -56400; $G = 4794
function Shot { if ($Shots) { @('wait 0.8', 'shot showui', 'wait 1.2') } else { @() } }
$fx = $X + 500; $fy = $Y + 500   # the forge
$mason = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'WRHour 11', 'wait 2', 'WRGive wood 300', 'WRGive stone 300', 'WRGive copper_ore 20', 'WRGive copper_ingot 12',
	"WRTeleport $($X - 600) $Y", 'wait 8', 'WRClearCreatures',
	'WRGhost claim_totem', "WRLookAt $X $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
	'WRGhost hearth', "WRLookAt $($X - 400) $($Y - 500) $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
	"WRTeleport $($X + 200) $($Y - 700)", 'wait 2',
	'WRGhost forge', "WRLookAt $fx $fy $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
	'WRGhost storage_chest', "WRLookAt $($X + 900) $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
	'WRClan create Wolves', 'WRClan invite Bravo', 'wait 1', 'WRSay host-ready',
	# 1. Behind Bravo's order at the forge.
	# (The forge lands short of the look point, near (X+430, Y+276): Mason stands 2.5 m from it, Bravo's side.)
	'waitchat bravo-ordered', "WRTeleport $($fx - 250) $($fy - 400)", 'wait 2', 'WRCmd craft copper_sword none 2', 'wait 0.5', 'WRSay mason-ordered',
	# 2. The raid, from the east, once Bravo has looked at the queue twice and is watching.
	'waitchat bravo-watching', "WRTeleport $($X - 4500) $($Y - 300)", 'wait 1', 'WRCmd raid now 6 90', 'WRSay raid-started',
	'waitchat bravo-saw-raiders', "WRTeleport $($X + 300) $($Y - 300)", 'wait 2') + @(1..5 | ForEach-Object { 'WRCreatureHP cinderfang 0' }) + @('wait 3', 'WRSay host-killed',
	'waitchat bravo-done', 'wait 1', 'WRSay coop-done')
$atForge = @("WRTeleport $($fx - 250) $fy", 'wait 1.5', "WRLookAt $fx $fy $($G + 60)", 'wait 0.5', 'WRKey E', 'wait 1.5')
$bravo = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 8', 'WRGive copper_ore 20') + $atForge +
	@('WRClick Materials', 'wait 0.8', 'WRClick Copper ingot #1', 'wait 0.8') + @(1..7 | ForEach-Object { 'WRClick +' }) + @('wait 0.5', 'WRClick Craft Copper ingot (8)', 'wait 0.5', 'WRSay bravo-ordered',
	'waitchat mason-ordered', 'wait 1') + (Shot) + @('WRSay ui-before-clan', 'waitchat ui-before-clan', 'WRUIText', 'WRKey Escape', 'wait 0.5',
	'WRClan join Wolves', 'wait 2') + $atForge + (Shot) + @('WRSay ui-in-clan', 'waitchat ui-in-clan', 'WRUIText', 'WRKey Escape', 'wait 0.5',
	# 2. Watching from 45 m west of the base, facing it.
	"WRTeleport $($X - 4500) $($Y + 300)", 'wait 1.5', 'WRLook 0 -6', 'wait 0.5', 'WRSay bravo-watching',
	'waitchat raid-started', 'wait 0.6') + (Shot) + @('WRSay ui-card', 'waitchat ui-card', 'WRUIText', 'wait 11') + (Shot) + @('WRSay ui-tracker', 'waitchat ui-tracker', 'WRUIText', 'WRSay bravo-saw-raiders',
	'waitchat host-killed', 'wait 2') + (Shot) + @('WRSay ui-over', 'waitchat ui-over', 'WRUIText', 'wait 1', 'WRSay bravo-done')
$shotNames = @('queue_not_in_clan', 'queue_in_clan', 'raid_card', 'raid_tracker', 'raid_over')

$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-coop-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs | Out-Null
$db = Join-Path $root 'Saved\Worlds\cooptest'
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date
$hostLog = Join-Path $logs 'host.log'; $friendLog = Join-Path $logs 'friend.log'
# (Crafting at half speed here: both orders stay on the forge while Bravo looks at them.)
$tune = 'wildlife.enabled:0,craft.timeScale:2,raid.warnSeconds:6'
$h = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=Mason?Path=fire?Body=m -game -nullrhi -unattended -nosound -port=$Port -wrworld=cooptest -wrtune=$tune -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($mason -join '|')`" -abslog=`"$hostLog`"" -PassThru -WindowStyle Hidden
$f = $null
$deadline = (Get-Date).AddSeconds(420)
while ((Get-Date) -lt $deadline -and -not $h.HasExited) {
	Start-Sleep -Seconds 3
	if (-not $f -and (Select-String -Path $hostLog -Pattern 'chat Mason: host-ready' -Quiet -ErrorAction SilentlyContinue)) {
		$f = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=Bravo?Body=f -game -windowed -ResX=1024 -ResY=768 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($bravo -join '|')`" -abslog=`"$friendLog`"" -PassThru
	}
	if (Select-String -Path $hostLog -Pattern 'chat Mason: coop-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
}
foreach ($p in @($f, $h)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$lock.ReleaseMutex()
if ($Shots) {
	New-Item -ItemType Directory -Force $Out | Out-Null
	$files = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
	for ($i = 0; $i -lt $files.Count; $i++) {
		$label = if ($i -lt $shotNames.Count) { $shotNames[$i] } else { "extra$i" }
		$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $label)
		Move-Item $files[$i].FullName $dest -Force
		$dest
	}
}

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Lines([string]$file) { if (Test-Path $file) { @(Get-Content $file -Encoding UTF8 | Where-Object { $_ -match 'LogWildreach: ' } | ForEach-Object { ($_ -replace '^.*LogWildreach: (Warning: )?', '').Trim() }) } else { @() } }
$hl = Lines $hostLog
$fl = Lines $friendLog
$hl | Where-Object { $_ -match '^(CRAFT (Bravo|Mason) queued|RAID (warning|came|over)|NOTICE Your)' } | ForEach-Object { "  host:   $($_.Substring(0, [Math]::Min(170, $_.Length)))" }
$fl | Where-Object { $_ -match '^(RAIDTRACK|FLOW raid|NOTICE Raid|NOTICE chat Bravo: ui-)' } | ForEach-Object { "  friend: $($_.Substring(0, [Math]::Min(170, $_.Length)))" }
function Index($lines, [string]$pattern, [int]$after = -1) { for ($i = $after + 1; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { return $i } }; return -1 }
# What Bravo's screen had after each "ui-" marker (WRUIText).
function Screen([string]$marker) {
	$from = Index $fl "^NOTICE chat Bravo: $marker$"
	if ($from -lt 0) { return @() }
	$to = Index $fl '^NOTICE chat Bravo: ' $from
	if ($to -lt 0) { $to = $fl.Count }
	@($fl[$from..($to - 1)] | Where-Object { $_ -like 'UITEXT*' } | ForEach-Object { $_ -replace '^UITEXT\s+-?\d+,\s*-?\d+ ', '' })
}
$before = Screen 'ui-before-clan'; $inClan = Screen 'ui-in-clan'; $card = Screen 'ui-card'; $tracker = Screen 'ui-tracker'; $over = Screen 'ui-over'
function Count($texts, [string]$exact) { @($texts | Where-Object { $_ -eq $exact }).Count }

$qb = Index $hl '^CRAFT Bravo queued 8 x copper_ingot at forge'
$qm = Index $hl '^CRAFT Mason queued 2 x copper_sword at forge .*; 2 orders\)' $qb
Check 'Bravo orders 8 ingots by clicks at Mason''s forge, and Mason''s 2 swords go behind it' ($qb -ge 0 -and $qm -gt $qb)
Check 'not in his clan, Bravo sees Mason''s order marked as his, with no Cancel on it' ((Count $before "Mason's order") -eq 1 -and (Count $before 'Cancel') -eq 1) "$((Count $before "Mason's order")) marked, $((Count $before 'Cancel')) Cancel"
Check 'in the clan, Cancel is on both orders' ((Count $inClan "Mason's order") -eq 1 -and (Count $inClan 'Cancel') -eq 2) "$((Count $inClan 'Cancel')) Cancel"
$bravoBag = @($hl | Where-Object { $_ -match "^CRAFT Bravo's copper_ingot at forge: Crafted 1 Copper ingot, into their bag" }).Count
$lastIngot = -1; for ($i = 0; $i -lt $hl.Count; $i++) { if ($hl[$i] -match "^CRAFT Bravo's copper_ingot at forge: Crafted") { $lastIngot = $i } }
$firstSword = Index $hl "^CRAFT Mason's copper_sword at forge: "
Check 'Bravo''s ingots come into Bravo''s bag while he''s there' ($bravoBag -ge 4) "$bravoBag into his bag"
Check 'Mason''s swords wait for Bravo''s order, go to the tray and Mason is told' ($firstSword -gt $lastIngot -and @($hl | Where-Object { $_ -match "^CRAFT Mason's copper_sword at forge: .* into the tray" }).Count -eq 2 -and (Index $hl '^NOTICE Your 2 Copper sword are done') -ge 0)
Check 'Bravo, Mason''s clanmate, gets the raid card' ((Index $fl '^FLOW raid card: Cinderfangs / 5 Cinderfangs are coming for your base from the east') -ge 0)
Check 'with no notice under the card' (@($card | Where-Object { $_ -like 'Raid: *' }).Count -eq 0 -and @($card | Where-Object { $_ -like '5 Cinderfangs are coming*' }).Count -ge 1) (($card | Where-Object { $_ -match 'Cinderfangs' }) -join ' | ')
$t0 = Index $fl '^RAIDTRACK stage 0, 0 of 5 left, \d+ s, from the east'
$t1 = Index $fl '^RAIDTRACK stage 1, 5 of 5 left' $t0
Check 'the raid tracker: coming, then on' ($t0 -ge 0 -and $t1 -gt $t0 -and (Count $card 'Raid on your base') -eq 1 -and (Count $card '5 coming from the east') -eq 1)
Check 'it counts them while they''re here' ((Count $tracker 'Raid on your base') -eq 1 -and @($tracker | Where-Object { $_ -match '^[1-5] of 5 raiders left$' }).Count -eq 1) (($tracker | Where-Object { $_ -match 'raiders left' }) -join ' | ')
Check 'killed by Mason: Bravo is told it''s over, and the tracker goes' ((Index $fl '^NOTICE Raid over: you held them off') -ge 0 -and (Count $over 'Raid on your base') -eq 0)
$missed = @($fl | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
Check 'every click lands at 4:3' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
$errors = @(Select-String -Path $hostLog, $friendLog -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in either log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
