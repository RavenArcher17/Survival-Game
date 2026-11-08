# Crafting time (0.7) by real clicks, as the host on L_Island at 16:10: a forge, its Crafting tab by E.
#  1. Copper ingot x5 (the + button, then Craft): the details say how long (2 s a recipe, over the playtest preset's 2x
#     crafting speed: 1 s each), one order of five goes on the forge's queue, the strip under the cards shows it under
#     way, and the ingots come into the bag one at a time.
#  2. Ten more, then 50 m off: the rest go into the forge's tray and the maker is told; back at it, Take all.
#  3. Copper sword x2, cancelled while the first is under way: its materials come back.
#  4. Max more ingots (a dozen or so), then the world saved: the order is in the forge's piece in world.json.
# -Shots keeps the pictures; -Log re-checks the client log of an earlier run.
#   crafttimetest.ps1 [-Port 7898] [-Shots] [-Out <folder>] [-Log <client.log>]
param([int]$Port = 7898, [switch]$Shots, [string]$Out = '', [string]$Log = '')
$name = 'Mason'
$X = -132000; $Y = -56400; $G = 4794
function Shot { if ($Shots) { @('wait 0.8', 'shot showui', 'wait 1.2') } else { @() } }
$atForge = @("WRTeleport $($X + 150) $Y", 'wait 1.5', "WRLookAt $($X + 400) $Y $($G + 60)", 'wait 0.5', 'WRKey E', 'wait 1.5')
$cmds = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'WRHour 11', 'wait 2', 'WRGive copper_ore 60', 'WRGive wood 30', 'WRGive stone 60',
	"WRTeleport $($X - 600) $Y", 'wait 8', 'WRClearCreatures',
	'WRGhost forge', "WRLookAt $($X + 400) $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5') + $atForge +
	# 1. Five ingots.
	@('WRClick Copper ingot', 'wait 0.8', 'WRClick +', 'WRClick +', 'WRClick +', 'WRClick +', 'wait 0.5', 'WRUIText', 'WRClick Craft Copper ingot (5)', 'wait 2.2') + (Shot) + @('WRUIText', 'wait 5') +
	# 2. Ten, walked off from, taken from the tray.
	@(1..9 | ForEach-Object { 'WRClick +' }) + @('wait 0.5', 'WRClick Craft Copper ingot (10)', 'wait 1', 'WRKey Escape', 'wait 0.5', "WRTeleport $($X - 5000) $Y", 'wait 14') + $atForge + (Shot) +
	@('WRUIText', 'WRClick Take all', 'wait 1.5') +
	# 3. Two swords, cancelled.
	@('WRClick Copper sword', 'wait 0.8', 'WRClick +', 'wait 0.5', 'WRClick Craft Copper sword (2)', 'wait 0.6', 'WRClick Cancel', 'wait 1.5', 'WRUIText') +
	# 4. Three more ingots (from the Materials list: the card is further down), and the world saved while they're under way.
	@('WRClick Materials', 'wait 0.8', 'WRClick Copper ingot #1', 'wait 0.8', 'WRClick Max', 'wait 0.5', 'WRClick Craft Copper ingot', 'wait 0.8', 'WRCmd saveworld', 'wait 1') + (Shot) +
	@('WRKey Escape', 'wait 1', 'WRSay crafttime-done')
$shotNames = @('making', 'tray_ready', 'materials_queue')

if (-not $Log) {
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
	$env:UE_SKIP_UBT_SDK_SETUP = '1'
	$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
	$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
	$root = Split-Path $project
	$logs = Join-Path $env:TEMP ("wildreach-crafttime-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
	if (-not $Out) { $Out = Join-Path $logs 'shots' }
	New-Item -ItemType Directory -Force $logs | Out-Null
	$db = Join-Path $root 'Saved\Worlds\crafttimetest'
	Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
	$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
	$started = Get-Date
	$c = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=${name}?Path=fire?Body=m -game -windowed -ResX=1440 -ResY=900 -unattended -nosound -port=$Port -wrworld=crafttimetest -wrtune=wildlife.enabled:0 -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
	$deadline = (Get-Date).AddSeconds(300)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
		Start-Sleep -Seconds 3
		if (Select-String -Path "$logs\client.log" -Pattern "chat ${name}: crafttime-done" -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
	}
	if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
	Start-Sleep -Seconds 2
	Copy-Item (Join-Path $db 'world.json') (Join-Path $logs 'world.json') -ErrorAction SilentlyContinue
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
}
$worldFile = Join-Path (Split-Path $Log) 'world.json'
$saved = if (Test-Path $worldFile) { Get-Content $worldFile -Raw | ConvertFrom-Json } else { $null }

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$raw = @(Get-Content $Log -Encoding UTF8 | Where-Object { $_ -match 'LogWildreach: ' })
$lines = @($raw | ForEach-Object { ($_ -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
$lines | Where-Object { $_ -match '^(CRAFT|NOTICE (Queued|Crafted|Your|Took|Cancelled|need|The))' } | ForEach-Object { "  $($_.Substring(0, [Math]::Min(200, $_.Length)))" }
$texts = @($lines | Where-Object { $_ -like 'UITEXT*' } | ForEach-Object { ($_ -replace '^UITEXT\s+-?\d+,\s*-?\d+ ', '') })
function Index([string]$pattern, [int]$after = -1) { for ($i = $after + 1; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { return $i } }; return -1 }
# When a line was logged: [2026.11.07-23.52.50:262] -> seconds.
function TimeOf([int]$i) { if ($raw[$i] -match '^\[\d+\.\d+\.\d+-(\d+)\.(\d+)\.(\d+):(\d+)\]') { [int]$Matches[1] * 3600 + [int]$Matches[2] * 60 + [int]$Matches[3] + [int]$Matches[4] / 1000.0 } else { -1 } }

Check 'the details say how long: 1 s each at the playtest preset''s 2x' ($texts -contains 'Takes 1 s each: 5 s for 5. It keeps going if you walk off.')
$q1 = Index '^CRAFT Mason queued 5 x copper_ingot at forge \(1\.0 s each; 1 orders\)'
Check 'Craft (5) is one order of five on the forge''s queue' ($q1 -ge 0)
Check 'the strip under the cards shows it under way' (($texts -contains 'Making') -and @($texts | Where-Object { $_ -match '^[2-5] of 5$' }).Count -ge 1)
$bag = @(); for ($i = $q1 + 1; $i -ge 1 -and $i -lt $lines.Count -and $bag.Count -lt 5; $i++) { if ($lines[$i] -match '^CRAFT Mason''s copper_ingot at forge: Crafted 1 Copper ingot, into their bag') { $bag += $i } }
$span = if ($bag.Count -eq 5) { (TimeOf $bag[4]) - (TimeOf $q1) } else { -1 }
Check 'the five come into the bag one at a time, about a second apart' ($bag.Count -eq 5 -and $span -ge 4.0 -and $span -le 7.5) "$($bag.Count) in $([math]::Round($span, 1)) s"
$q2 = Index '^CRAFT Mason queued 10 x copper_ingot at forge' $q1
$tray = @($lines | Select-Object -Skip ([Math]::Max(0, $q2)) | Where-Object { $_ -match '^CRAFT Mason''s copper_ingot at forge: .* into the tray' }).Count
Check 'walked off, the rest go into the tray, and the maker is told' ($q2 -ge 0 -and $tray -ge 6 -and (Index '^NOTICE Your 10 Copper ingot are done: take them from the forge' $q2) -ge 0) "$tray into the tray"
Check 'Take all brings them into the bag' (($texts -contains 'Take all') -and (Index "^CRAFT Mason took $tray from the forge's tray \(0 left\)" $q2) -ge 0)
$q3 = Index '^CRAFT Mason queued 2 x copper_sword at forge'
$cancel = Index '^CRAFT Mason cancelled Mason''s order of 2 copper_sword at forge' $q3
Check 'Cancel calls the swords off before one is made, and their materials come back' ($q3 -ge 0 -and $cancel -gt $q3 -and (Index '^NOTICE Cancelled 2 Copper sword: its materials are back' $cancel) -ge 0 -and (Index 'copper_sword at forge: Crafted' $q3) -lt 0)
$forge = if ($saved) { @($saved.pieces | Where-Object { $_.piece -eq 'forge' }) | Select-Object -First 1 } else { $null }
$job = if ($forge -and $forge.queue) { $forge.queue[0] } else { $null }
Check 'an order under way is saved with its forge' ($job -and $job.recipe -eq 'copper_ingot' -and $job.left -ge 1 -and $job.owner -eq 'Mason' -and $job.each -eq 1) $(if ($job) { $job | ConvertTo-Json -Compress } else { 'no queue saved' })
$missed = @($lines | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
Check 'every click lands on its button' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
$errors = @(Select-String -Path $Log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the client log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (log: {2})" -f $results.Count, $failed, $Log
if ($failed -gt 0) { exit 1 }
