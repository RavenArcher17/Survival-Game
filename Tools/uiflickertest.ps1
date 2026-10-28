# The journal held still, frame by frame. A rendered client opens the tabs (the Map by its key, the rest by clicking
# them, the Map again with the mouse resting on the picture), lets each settle, then logs every piece of on-screen text
# and where it sits on each of 90 frames in a row (WRUIText). A frame where several pieces of text jump from where they
# sit on the others is a flash: 0.4.1's Map tab rebuilt its details panel twice a second inside Slate's paint, so for
# one frame each time it drew before anything in it was measured (Raven: "the bottom right is flashing another panel
# on top of it but only for an instant"). -Shots also saves a picture of each of 45 frames on the map. -Log re-reads
# the client log of an earlier run instead of playing.
#   uiflickertest.ps1 [-Port 7875] [-Shots] [-Log <client.log>]
param([int]$Port = 7875, [switch]$Shots, [string]$Log = '')
# Each section: the commands that bring it up, and the menu it should then show.
$sections = [ordered]@{
	'map (M key)'         = @('WRKey M|wait 2.5', 'map')
	'inventory (clicked)' = @('WRClick Inventory|wait 2', 'inventory')
	'map (clicked)'       = @('WRClick Map|wait 2', 'map')
	'map (mouse on it)'   = @('WRClick N|wait 1.5', 'map')
	'crafting (J key)'    = @('WRKey J|wait 2', 'crafting')
	'paths (clicked)'     = @('WRClick Paths|wait 2', 'paths')
	'skills (K key)'      = @('WRKey K|wait 2', 'skills')
}
$names = @($sections.Keys)

if (-not $Log) {
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
	$env:UE_SKIP_UBT_SDK_SETUP = '1'
	$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
	$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
	$root = Split-Path $project
	$logs = Join-Path $env:TEMP ("wildreach-flicker-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
	New-Item -ItemType Directory -Force $logs | Out-Null
	$name = 'Steady'
	Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
	$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
	$started = Get-Date
	# Each section: its commands, a WRUIState naming it in the log, then one WRUIText per frame.
	$frames = (1..90 | ForEach-Object { 'WRUIText|wait 0' }) -join '|'
	$cmds = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 3', 'WRTeleport -51623 54155', 'wait 6', 'WRLook 270 -8', 'wait 1')
	foreach ($s in $sections.GetEnumerator()) { $cmds += $s.Value[0]; $cmds += 'WRUIState'; $cmds += $frames }
	if ($Shots) { $cmds += 'WRClick Map|wait 2'; $cmds += ((1..45 | ForEach-Object { 'shot showui|wait 0' }) -join '|'); $cmds += 'wait 5' }
	$cmds += 'WRUIState|wait 1'
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=flicker -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 30
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=f -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
	$deadline = (Get-Date).AddSeconds(480)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
		Start-Sleep -Seconds 5
		# (Case-sensitive: the command line in the log says "WRUIState" too.)
		if (@(Select-String -Path "$logs\client.log" -Pattern 'UISTATE' -CaseSensitive -ErrorAction SilentlyContinue).Count -ge $names.Count + 1) { Start-Sleep -Seconds 2; break }
	}
	foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
	Start-Sleep -Seconds 2
	Remove-Item (Join-Path $root 'Saved\ServerDB\world_flicker.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
	$lock.ReleaseMutex()
	if ($Shots) {
		$out = Join-Path $logs 'shots'
		New-Item -ItemType Directory -Force $out | Out-Null
		$files = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
		for ($i = 0; $i -lt $files.Count; $i++) { Move-Item $files[$i].FullName (Join-Path $out ('map_frame{0:D2}.png' -f ($i + 1))) -Force }
		"$($files.Count) frame pictures in $out"
	}
	$Log = "$logs\client.log"
}

# Each section's text, frame by frame (the [frame] counter on each log line), as "text#n" -> "x,y".
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$data = @{}
$shown = @{}
$sec = -1
$rx = [regex]'^\[[^\]]+\]\[\s*(\d+)\]LogWildreach: (UISTATE|UITEXT)\s*(-?\d+)?,?\s*(-?\d+)? ?(.*)$'
foreach ($line in [System.IO.File]::ReadLines($Log)) {
	$m = $rx.Match($line)
	if (-not $m.Success) { continue }
	if ($m.Groups[2].Value -eq 'UISTATE') { $sec++; $data[$sec] = [ordered]@{}; $shown[$sec] = if ($line -match 'menu=(\w+)') { $Matches[1] } else { '?' }; continue }
	if ($sec -lt 0 -or $sec -ge $names.Count) { continue }
	$fk = 'f' + $m.Groups[1].Value
	if (-not $data[$sec].Contains($fk)) { $data[$sec][$fk] = [ordered]@{} }
	$fr = $data[$sec][$fk]
	$k = $m.Groups[5].Value
	$n = 0
	while ($fr.Contains("$k#$n")) { $n++ }
	$fr["$k#$n"] = "$($m.Groups[3].Value),$($m.Groups[4].Value)"
}
for ($i = 0; $i -lt $names.Count; $i++) {
	$f = if ($data.ContainsKey($i)) { $data[$i] } else { [ordered]@{} }
	# Where each piece of text usually sits in this section: its most common place.
	$votes = @{}
	foreach ($fr in $f.Values) { foreach ($e in $fr.GetEnumerator()) { if (-not $votes.ContainsKey($e.Key)) { $votes[$e.Key] = @{} }; $votes[$e.Key][$e.Value] = [int]$votes[$e.Key][$e.Value] + 1 } }
	$place = @{}
	foreach ($k in $votes.Keys) { $place[$k] = ($votes[$k].GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Key }
	$jumps = @()
	foreach ($fe in $f.GetEnumerator()) {
		$moved = @()
		foreach ($e in $fe.Value.GetEnumerator()) {
			$a = $e.Value -split ','; $b = $place[$e.Key] -split ','
			if ([Math]::Abs([int]$a[0] - [int]$b[0]) + [Math]::Abs([int]$a[1] - [int]$b[1]) -gt 3) { $moved += "'$($e.Key -replace '#\d+$', '')' $($place[$e.Key]) -> $($e.Value)" }
		}
		# One label moving (a clock, a distance) is the game; several at once is the layout.
		if ($moved.Count -ge 3) { $jumps += "$($fe.Key): $($moved.Count) moved, e.g. $(($moved | Select-Object -First 4) -join '; ')" }
	}
	$want = $sections[$names[$i]][1]
	if ($shown[$i] -ne $want) { Check "$($names[$i]): opens" $false "showed $($shown[$i]), not $want (a click that didn't land?)"; continue }
	Check "$($names[$i]): holds still frame to frame" ($f.Count -ge 60 -and $jumps.Count -eq 0) ("{0} frames, {1} jumped{2}" -f $f.Count, $jumps.Count, $(if ($jumps) { ': ' + $jumps[0] } else { '' }))
}
$errors = @(Select-String -Path $Log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check "no errors in the client log" ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (log: {2})" -f $results.Count, $failed, $Log
if ($failed -gt 0) { exit 1 }
