# Creature raids (WRRaids) as the host on L_Island: a small base (claim totem, Hearth, workbench, chest) in the volcanic
# lowland, and the world's setting read ("rare" in the playtest preset: a claim's raid every 72 game hours or so).
#  1. A raid from the east (WRCmd raid now, 6 s warning): the warning card and notice, five Cinderfangs (3 points + 2 for
#     the level 1 Hearth) come and bite the base while the player is 45 m off, then turn on the player who walks in;
#     killed, it's held, and the claim's next raid is scheduled (and saved with the claim).
#  2. A raid made due (WRCmd raid due) while the player is 400 m away waits; back home, it's warned and comes; with
#     raid.maxMinutes short here it times out, and the rest walk off and are taken away.
# -Shots keeps the pictures; -Log re-checks the client log of an earlier run.
#   raidtest.ps1 [-Port 7897] [-Shots] [-Out <folder>] [-Log <client.log>]
param([int]$Port = 7897, [switch]$Shots, [string]$Out = '', [string]$Log = '')
$name = 'Mason'
$X = -132000; $Y = -56400; $G = 4794
function Shot { if ($Shots) { @('wait 0.8', 'shot showui', 'wait 1.2') } else { @() } }
$cmds = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'WRHour 11', 'wait 2', 'WRGive wood 300', 'WRGive stone 300', 'WRGive copper_ingot 12',
	"WRTeleport $($X - 600) $Y", 'wait 8', 'WRClearCreatures') +
	# The base: totem in the middle, Hearth and workbench beside it, the chest on its east side.
	@('WRGhost claim_totem', "WRLookAt $X $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
		'WRGhost hearth', "WRLookAt $($X - 400) $($Y - 500) $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
		"WRTeleport $($X + 200) $($Y - 700)", 'wait 2',
		'WRGhost workbench', "WRLookAt $($X + 500) $($Y + 500) $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
		'WRGhost storage_chest', "WRLookAt $($X + 900) $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5',
		# Something in the chest: a broken chest leaves it in a bag.
		"WRTeleport $($X + 600) $($Y - 300)", 'wait 1.5', 'WRCmd storeall', 'wait 1.5',
		'WRCmd raid status', 'wait 1', 'WRCmd raid plan', 'wait 1') +
	# 1. From the east, the player 45 m to the west watching; then in among them, then they're killed.
	@("WRTeleport $($X - 4500) $Y", 'wait 2', 'WRLook 0 -6', 'WRCmd raid now 6 90', 'wait 1') + (Shot) + @('wait 14') +
	# (A look from 25 m north of the chest: further than a Cinderfang notices you.)
	@("WRTeleport $($X + 800) $($Y - 2600)", 'wait 1.5', "WRLookAt $($X + 800) $Y $($G + 100)", 'wait 0.5') + (Shot) + @('wait 10') +
	@("WRTeleport $($X + 300) $($Y - 300)", 'wait 2.5') + @(1..5 | ForEach-Object { 'WRCreatureHP cinderfang 0' }) + @('wait 4') + (Shot) +
	# 2. Due while 400 m away: it waits; back home, it's warned; then 150 m off (out of their reach) while it runs out of time.
	@('WRCmd raid due', "WRTeleport $($X - 40000) $Y", 'wait 8', "WRTeleport $($X - 4500) $Y", 'wait 4', 'WRCmd raid status', "WRTeleport $($X - 15000) $Y",
		'wait 60', 'WRCmd raid status', 'wait 40', 'WRCmd raid status', 'wait 1', 'WRSay raidtest-done')
$shotNames = @('warning', 'raiders_at_the_base', 'held')

if (-not $Log) {
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
	$env:UE_SKIP_UBT_SDK_SETUP = '1'
	$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
	$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
	$root = Split-Path $project
	$logs = Join-Path $env:TEMP ("wildreach-raid-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
	if (-not $Out) { $Out = Join-Path $logs 'shots' }
	New-Item -ItemType Directory -Force $logs | Out-Null
	$db = Join-Path $root 'Saved\Worlds\raidtest'
	Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
	$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
	$started = Get-Date
	$tune = 'wildlife.enabled:0,raid.maxMinutes:0.75,raid.warnSeconds:6,raid.leaveSeconds:30'
	$c = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=${name}?Path=fire?Body=m -game -windowed -ResX=1440 -ResY=900 -unattended -nosound -port=$Port -wrworld=raidtest -wrtune=$tune -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
	$deadline = (Get-Date).AddSeconds(420)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
		Start-Sleep -Seconds 3
		if (Select-String -Path "$logs\client.log" -Pattern "chat ${name}: raidtest-done" -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
	}
	if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
	Start-Sleep -Seconds 2
	$world = Join-Path $db 'world.json'
	$saved = if (Test-Path $world) { Get-Content $world -Raw | ConvertFrom-Json } else { $null }
	Copy-Item $world (Join-Path $logs 'world.json') -ErrorAction SilentlyContinue
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
	$Log = "$logs\client.log"
} else {
	$w = Join-Path (Split-Path $Log) 'world.json'
	$saved = if (Test-Path $w) { Get-Content $w -Raw | ConvertFrom-Json } else { $null }
}

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$lines = @(Get-Content $Log -Encoding UTF8 | Where-Object { $_ -match 'LogWildreach: ' } | ForEach-Object { ($_ -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
$lines | Where-Object { $_ -match '^(RAID (setting|first|plan|warning|came|over|next|due|made|status|none|called|\d+ raiders)|NOTICE Raid|FLOW raid)' } | ForEach-Object { "  $($_.Substring(0, [Math]::Min(230, $_.Length)))" }
function Index([string]$pattern, [int]$after = -1) { for ($i = $after + 1; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { return $i } }; return -1 }
function Between([string]$pattern, [int]$from, [int]$to) { if ($from -lt 0) { return 0 }; if ($to -lt 0) { $to = $lines.Count }; @($lines[$from..($to - 1)] | Where-Object { $_ -match $pattern }).Count }

Check 'the world''s raid setting is read (the playtest preset: rare)' ((Index '^RAID setting rare: a claim''s raid every 72 game hours') -ge 0)
$first = $lines | Where-Object { $_ -match '^RAID first for Mason''s claim in ([\d.]+) game hours' } | Select-Object -First 1
$firstHours = if ($first -match 'in ([\d.]+) game') { [double]$Matches[1] } else { -1 }
Check 'a new claim''s first raid is one interval away (72 h, give or take a quarter)' ($firstHours -ge 54 -and $firstHours -le 90) "$firstHours h"
$plan = $lines | Where-Object { $_ -match '^RAID plan ' } | Select-Object -First 1
Check 'the raid grows with the Hearth level (3 Cinderfangs, 5, 7, 9)' ($plan -match 'volcanic_lowland: level 0 \(3 points\) 3 Cinderfangs; level 1 \(5 points\) 5 Cinderfangs; level 2 \(7 points\) 7 Cinderfangs; level 3 \(9 points\) 9 Cinderfangs') $plan
# Raid 1.
$warn1 = Index '^RAID warning for Mason''s claim \(region volcanic_lowland, Hearth level 1, 5 points\): 5 Cinderfangs from the east'
$came1 = Index '^RAID came at Mason''s claim: 5 raiders' $warn1
$over1 = Index '^RAID over at Mason''s claim \(held: 5 killed\)' $came1
Check 'the warning: the card and the notice, what, how many, from where' ($warn1 -ge 0 -and (Between '^NOTICE Raid: 5 Cinderfangs are coming for your base from the east, in 6 s' $warn1 $came1) -ge 1 -and (Between '^FLOW raid card: Cinderfangs / 5 Cinderfangs are coming' $warn1 $came1) -ge 1)
Check 'they come after the warning: the claim''s Hearth (level 1) makes it 5' ($came1 -ge 0 -and (Between '^NOTICE Raid: they''re here, from the east' $came1 ($came1 + 40)) -ge 1)
$bites = Between '^RAID WRCreature\S* bit ' $came1 $over1
$broken = Between '^BUILD \S+ \(\S+\) destroyed by WRCreature' $came1 $over1
Check 'they bite the base and break a piece (the chest on the side they came from first)' ($bites -ge 4 -and $broken -ge 1 -and (Index '^RAID WRCreature\S* bit \S+ \(storage_chest\)' $came1) -ge 0) "$bites bites, $broken broken"
Check 'the broken chest leaves what it held in a bag' ((Between '^BUILD \S+ \(storage_chest\) spilled [1-9]\d* stacks' $came1 $over1) -eq 1)
Check 'they turn on the player who comes near' ((Between '^COMBAT WRCreature\S* -> WRCharacter' $came1 $over1) -ge 1)
Check 'killed, the raid is held and the claim''s next one is scheduled' ($over1 -ge 0 -and (Between '^NOTICE Raid over: you held them off' $over1 ($over1 + 30)) -ge 1 -and (Between '^RAID next for Mason''s claim in ([\d.]+) game hours' $over1 ($over1 + 30)) -ge 1)
# Raid 2.
$due = Index '^RAID made due on Mason''s claim' $over1
$waits = Index '^RAID due on Mason''s claim: waiting for someone of it to be home' $due
$warn2 = Index '^RAID warning for Mason''s claim' $due
Check 'a due raid waits while nobody of the claim is home' ($waits -ge 0 -and ($warn2 -lt 0 -or $waits -lt $warn2))
$came2 = Index '^RAID came at Mason''s claim: 5 raiders' $warn2
Check 'and comes once they''re back' ($warn2 -gt $waits -and $came2 -gt $warn2)
$over2 = Index '^RAID over at Mason''s claim \(time''s up\)' $came2
$gone = Between '^RAID \d+ raiders gone from Mason''s claim' $over2 -1
Check 'after its time the rest leave and are taken away' ($over2 -ge 0 -and $gone -ge 1 -and (Between '^NOTICE Raid over: the last \d+ are leaving' $over2 ($over2 + 30)) -ge 1)
$nextSaved = if ($saved) { @($saved.pieces | Where-Object { $_.piece -eq 'claim_totem' -and $_.nextRaid -gt $saved.clockHours }).Count } else { 0 }
Check 'the claim''s next raid is saved with it, in the future' ($nextSaved -eq 1)
$missed = @($lines | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
$errors = @(Select-String -Path $Log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the client log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (log: {2})" -f $results.Count, $failed, $Log
if ($failed -gt 0) { exit 1 }
