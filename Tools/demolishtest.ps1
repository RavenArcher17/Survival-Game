# Demolish mode in the build bar, played with the mouse and keyboard as a player does (WRKey, WRClick, WRMouseClick go
# through Slate like real input). On L_Island by the claim test's hut site, beside a foundation and wall that another
# player ("Stranger", from a prepared world save) built:
#   B opens the build bar, a click on the Wood foundation card and a click in the world builds one; X turns demolish on,
#   the chip over the red copy says "Demolish Wood foundation" and what comes back, a click takes it down (half back);
#   aimed at Stranger's wall the chip says whose it is and a click is refused, as is the server when asked straight;
#   X again, Esc and right mouse each leave demolish mode, the Demolish button turns it on, X from outside build mode
#   opens the bar demolishing, and the key is listed in the H help and in Settings > Controls.
# -Shots also saves pictures: the bar demolishing a wall of yours (two angles by day, one by night), Stranger's wall
# refused, and the bar with its Demolish button. -Log re-checks the client log of an earlier run.
#   demolishtest.ps1 [-Port 7875] [-Shots] [-Out <folder>] [-Log <client.log>]
param([int]$Port = 7875, [switch]$Shots, [string]$Out = '', [string]$Log = '')
$name = 'Wrecker'
# Each WRUIState: the menu and what build mode holds that it should show.
$expect = @(
	@('none', 'none'), @('build', 'none'), @('build', 'wood_foundation'), @('build', 'demolish'), @('build', 'demolish'),
	@('build', 'none'), @('build', 'demolish'), @('build', 'none'), @('build', 'none'), @('none', 'none'), @('build', 'demolish'), @('none', 'none')
)
$hut = 'WRTeleport -131650 -56400|wait 5|WRLook 0 -10|wait 1'
$cmds = @(
	'WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 3', $hut, 'WRClearCreatures', 'WRHour 13', 'WRGive wood 120', 'wait 1.5', 'WRUIState',
	# Build a foundation through the bar: the card, then a click in the world.
	'WRKey B', 'wait 1', 'WRUIState',
	'WRClick Wood foundation @ Enough for', 'wait 1', 'WRUIState',
	'WRLookAt -131200 -56400 4800', 'wait 1.5', 'WRMouseClick', 'wait 2',
	# X: demolish it.
	'WRKey X', 'wait 0.5', 'WRLookAt -131200 -56400 4850', 'wait 1.5', 'WRUIState', 'WRUIText', 'WRMouseClick', 'wait 2',
	# Stranger's wall: the chip says whose it is, a click is refused, the server refuses too.
	'WRLookAt -132000 -56600 4984', 'wait 1.5', 'WRUIText', 'WRMouseClick', 'wait 1', 'WRDemolish -132000 -56600 4834', 'wait 1.5', 'WRUIState',
	# Out and in: X, the button, Esc, right mouse, Esc out of build mode, X from outside, B.
	'WRKey X', 'wait 1', 'WRUIState',
	'WRClick Demolish', 'wait 1', 'WRUIState',
	'WRKey Escape', 'wait 1', 'WRUIState',
	'WRClick Demolish', 'wait 1', 'WRMouseClick 0.5 0.5 rmb', 'wait 1', 'WRUIState',
	'WRKey Escape', 'wait 1', 'WRUIState',
	'WRKey X', 'wait 1', 'WRUIState',
	'WRKey B', 'wait 1', 'WRUIState',
	# The key in the H help and in Settings > Controls.
	'WRKey H', 'wait 1', 'WRUIText', 'WRKey H', 'wait 0.5',
	'WRKey Escape', 'wait 1.5', 'WRSettings scroll 2000', 'wait 0.5', 'WRUIText', 'WRClick Resume', 'wait 1'
)
if ($Shots) {
	# A wood wall of yours on a foundation (placed with the ghost, as a player would), then the bar demolishing it.
	$cmds += @(
		'WRGhost wood_foundation', 'WRLookAt -131200 -56400 4800', 'wait 1.5', 'WRGhostPlace', 'wait 1',
		'WRGhost wood_wall', 'WRLookAt -131200 -56560 4860', 'wait 1.5', 'WRGhostPlace', 'wait 1.5',
		'WRKey B', 'wait 1', 'WRKey X', 'WRLookAt -131200 -56600 4960', 'wait 2', 'shot showui', 'wait 2',
		'WRTeleport -130700 -57250', 'wait 4', 'WRLookAt -131200 -56600 4960', 'wait 2', 'shot showui', 'wait 2',
		'WRHour 22', 'wait 3', 'WRLookAt -131200 -56600 4960', 'wait 1', 'shot showui', 'wait 2', 'WRHour 13',
		$hut, 'WRLookAt -132000 -56600 4984', 'wait 2', 'shot showui', 'wait 2',
		'WRKey X', 'wait 1', 'shot showui', 'wait 3'
	)
}
$cmds += @('wait 1', 'WRUIState', 'wait 1')
$shotNames = @('demolish_wall_day_a', 'demolish_wall_day_b', 'demolish_wall_night_b', 'demolish_refused', 'build_bar_button')

if (-not $Log) {
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
	$env:UE_SKIP_UBT_SDK_SETUP = '1'
	$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
	$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
	$root = Split-Path $project
	$logs = Join-Path $env:TEMP ("wildreach-demolish-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
	if (-not $Out) { $Out = Join-Path $logs 'shots' }
	New-Item -ItemType Directory -Force $logs | Out-Null
	$db = Join-Path $root 'Saved\ServerDB'
	New-Item -ItemType Directory -Force (Join-Path $db 'players') | Out-Null
	Remove-Item (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue
	# Stranger's foundation and wall at the hut site (the world save format: AWRBuildPiece::ToJson).
	$piece = { param($id, $x, $y, $z, $hp) "{`"piece`":`"$id`",`"owner`":`"Stranger`",`"x`":$x,`"y`":$y,`"z`":$z,`"yaw`":0,`"hp`":$hp,`"code`":`"`",`"locked`":false,`"planted`":false,`"cropSecondsLeft`":0,`"storage`":[]}" }
	$world = "{`"version`":1,`"pieces`":[$(& $piece 'wood_foundation' -132000 -56400 4734 600),$(& $piece 'wood_wall' -132000 -56600 4834 400)],`"tames`":[]}"
	[System.IO.File]::WriteAllText((Join-Path $db 'world_demolishtest.json'), $world)
	$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
	$started = Get-Date
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=demolishtest -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 30
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=m -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
	$deadline = (Get-Date).AddSeconds(420)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
		Start-Sleep -Seconds 5
		# (Case-sensitive: the command line in the log says "WRUIState" too.)
		if (@(Select-String -Path "$logs\client.log" -Pattern 'UISTATE' -CaseSensitive -ErrorAction SilentlyContinue).Count -ge $expect.Count + 1) { Start-Sleep -Seconds 2; break }
	}
	foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
	Start-Sleep -Seconds 2
	Remove-Item (Join-Path $db 'world_demolishtest.json'), (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue
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

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$lines = @(Get-Content $Log | Where-Object { $_ -match 'LogWildreach: ' } | ForEach-Object { ($_ -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
$lines | Where-Object { $_ -match '^(UISTATE|CLICK|KEY |MOUSECLICK|NOTICE|DEMOLISH)' } | ForEach-Object { "  $_" }
$states = @($lines | Where-Object { $_ -like 'UISTATE*' })
$bad = @()
for ($i = 0; $i -lt $expect.Count; $i++) {
	$st = if ($i -lt $states.Count) { $states[$i] } else { '' }
	if ($st -notmatch "menu=$($expect[$i][0]) .*placing=$($expect[$i][1])$") { $bad += "step $($i + 1): wanted $($expect[$i] -join '/'), got " + $(if ($st -match 'menu=(\w+).*placing=(\S+)') { "$($Matches[1])/$($Matches[2])" } else { 'nothing' }) }
}
Check 'each key and click puts build mode where a player expects' ($bad.Count -eq 0) (($bad | Select-Object -First 3) -join '; ')
$notices = @($lines | Where-Object { $_ -like 'NOTICE *' } | ForEach-Object { $_.Substring(7) })
Check 'a click on the card and one in the world builds a foundation' (@($notices | Where-Object { $_ -eq 'Built Wood foundation' }).Count -ge 1)
# The text on screen while aiming (WRUIText right after each aim): the chip over the red copy.
$texts = @($lines | Where-Object { $_ -like 'UITEXT*' } | ForEach-Object { ($_ -replace '^UITEXT\s+-?\d+,\s*-?\d+ ', '') })
Check 'demolishing, the chip names the piece under the crosshair' ($texts -contains 'Demolish Wood foundation')
Check 'and what comes back (half of 20 wood)' ($texts -contains '10 wood back')
Check 'a click takes it down, half refunded' (@($notices | Where-Object { $_ -eq 'Demolished Wood foundation (half refunded)' }).Count -eq 1)
Check "aimed at Stranger's wall, the chip says whose it is" ($texts -contains 'Not yours: Stranger built it')
Check 'a click there is refused, saying why' (@($lines | Where-Object { $_ -eq 'DEMOLISH refused here: not yours: Stranger built it' }).Count -ge 1)
Check 'the server refuses it too' ($notices -contains 'not yours: Stranger built it')
Check "Stranger's pieces stay up" (@($notices | Where-Object { $_ -like 'Demolished Wood wall*' }).Count -eq 0)
$clicks = @($lines | Where-Object { $_ -like 'MOUSECLICK*' })
Check 'clicks in the world reach the game view' ($clicks.Count -ge 4 -and @($clicks | Where-Object { $_ -notmatch 'SViewport' }).Count -eq 0) (($clicks | Select-Object -First 2) -join ' | ')
$missed = @($lines | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
Check 'every click on the bar lands on its button' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
Check 'the H help and Settings > Controls list the key' (@($texts | Where-Object { $_ -eq 'Building: demolish mode' }).Count -ge 2)
$errors = @(Select-String -Path $Log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the client log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (log: {2})" -f $results.Count, $failed, $Log
if ($failed -gt 0) { exit 1 }
