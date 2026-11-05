# Worlds (WRWorlds) through the main menu, by real clicks in one game window, going back to the menu between worlds as a
# player does: Host a game -> New world "WT Alpha" (PvE) -> build a foundation -> Main menu -> New world "WT Beta" (PvP):
# empty, a fresh character, PvP's settings -> Main menu -> rename Beta, Delete -> Keep it, Delete -> Yes -> Play Alpha:
# the foundation and the character are back, with PvE's settings -> Main menu -> the world called Island, copied from
# the old Saved/ServerDB on the first look at the list: the old character is in it, and ServerDB is still there.
# The real Island (if any) and the note that it was copied are set aside while this runs, and put back after.
#   worldstest.ps1 [-Port 7886] [-Shots <folder>]   (screenshots of the window: the list, a new world, rename, delete)
param([int]$Port = 7886, [string]$Shots = '')
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-worlds-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $logs | Out-Null
$log = Join-Path $logs 'game.log'
$worlds = Join-Path $root 'Saved\Worlds'
$old = Join-Path $root 'Saved\ServerDB'
$name = 'Worldbot'
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$shotsBefore = @(Get-ChildItem $shotDir -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })

$island = Join-Path $worlds 'Island'
$marker = Join-Path $worlds 'moved_from_ServerDB.txt'
$kept = Join-Path $logs 'kept'
$seed = Join-Path $old "players\$name.json"

function Remove-TestWorlds { Get-ChildItem $worlds -Directory -Filter 'WT_*' -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force }
function Json([string]$path) { if (Test-Path $path) { Get-Content $path -Raw | ConvertFrom-Json } else { $null } }

$shot = if ($Shots) { @('wait 1', 'shot showui', 'wait 1') } else { @() }
$menu = @('wait 3', 'WRMainMenu', 'wait 3', 'WRClick Host a game', 'wait 2')
$leave = @('WRKey Escape', 'wait 1.5', 'WRClick Main menu')
function At([int]$world, [object[]]$cmds) { $cmds | ForEach-Object { "@${world}:$_" } }
$bot = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off') +
	(At 1 ($menu + $shot + @('WRClick New world', 'wait 2', 'WRType WT Alpha', 'wait 1') + $shot + @('WRClick Create and play'))) +
	(At 2 (@('wait 10', 'WRTeleport -131650 -56400', 'wait 5', 'WRClearCreatures', 'WRGive wood 120', 'wait 1',
		'WRGhost wood_foundation', 'WRLookAt -131200 -56400 4800', 'wait 1.5', 'WRGhostPlace', 'wait 2') + $leave)) +
	(At 3 ($menu + @('WRClick New world', 'wait 2', 'WRType WT Beta', 'wait 1', 'WRClick PvP', 'wait 1') + $shot + @('WRClick Create and play'))) +
	(At 4 (@('wait 10') + $leave)) +
	(At 5 ($menu + @('WRClick WT Beta', 'wait 1') + $shot + @('WRClick Rename', 'wait 1.5', 'WRType WT Gamma', 'wait 0.5') + $shot + @('WRKey Enter', 'wait 1.5',
		'WRClick Delete', 'wait 1') + $shot + @('WRClick Keep it', 'wait 1', 'WRClick Delete', 'wait 1', 'WRClick Yes, delete it', 'wait 1.5', 'WRClick WT Alpha', 'wait 1', 'WRClick Play'))) +
	(At 6 (@('wait 10') + $leave)) +
	# (Esc: one press from a new world's name box back to the list, one more out to the menu. Each click after it only lands if it worked.)
	(At 7 ($menu + @('WRClick New world', 'wait 2', 'WRKey Escape', 'wait 1', 'WRClick Rename', 'wait 1.5', 'WRKey Escape', 'wait 1', 'WRKey Escape', 'wait 1', 'WRClick Host a game', 'wait 2', 'WRClick Island', 'wait 1', 'WRClick Play'))) +
	(At 8 @('wait 10', 'WRSay worlds-done'))

try {
	Remove-TestWorlds
	New-Item -ItemType Directory -Force $worlds, $kept, (Split-Path $seed) | Out-Null
	if (Test-Path $island) { Move-Item $island (Join-Path $kept 'Island') }
	if (Test-Path $marker) { Move-Item $marker (Join-Path $kept 'marker.txt') }
	# An old character to find in Island: on Frost, where the bot joins on Fire.
	[IO.File]::WriteAllText($seed, "{`"version`":1,`"name`":`"$name`",`"paths`":{`"main`":`"frost`",`"sub`":`"`",`"paths`":[],`"quickSlots`":[]}}")

	$ini = "[Wildreach.Menu]:Name=$name,[Wildreach.Menu]:Body=m,[Wildreach.Menu]:Path=fire"
	$c = Start-Process $ue -ArgumentList ("`"$project`" /Game/Maps/L_Island -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot -port=$Port " +
		"`"-ini:Engine:[URL]:Port=$Port`" `"-ini:GameUserSettings:$ini`" `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($bot -join '|')`" -abslog=`"$log`"") -PassThru
	$deadline = (Get-Date).AddSeconds(720)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
		Start-Sleep -Seconds 5
		if (Select-String -Path $log -Pattern 'CHAT .*worlds-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
	}
	if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
	Start-Sleep -Seconds 2

	$results = @()
	function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
	$lines = @(Select-String -Path $log -Pattern 'LogWildreach: ' -ErrorAction SilentlyContinue | ForEach-Object { ($_.Line -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
	function Found([string]$pattern) { @($lines | Where-Object { $_ -match $pattern }).Count -gt 0 }
	# The log from the Nth time a game opened this world up to the next game (0 = the first).
	function Stretch([string]$world, [int]$nth = 0) {
		$starts = @(0..($lines.Count - 1) | Where-Object { $lines[$_] -like "WORLD '$world'*" })
		if ($starts.Count -le $nth) { return @() }
		$from = $starts[$nth]
		$next = @(($from + 1)..($lines.Count - 1) | Where-Object { $lines[$_] -match "^WORLD ('|none)" } | Select-Object -First 1)
		$to = if ($next.Count) { $next[0] } else { $lines.Count }
		@($lines[$from..($to - 1)])
	}

	# The old save.
	$info = Json (Join-Path $island 'info.json')
	Check 'the first look at the list copies the old save into a world called Island' ((Found 'WORLDS copied the old save') -and $info -and $info.name -eq 'Island')
	Check 'with the old characters in it' (Test-Path (Join-Path $island "players\$name.json"))
	Check 'and the old save stays as the backup' ((Test-Path $seed) -and (Test-Path $marker))
	$islandStretch = Stretch 'Island'
	Check 'hosting Island loads the old character' (@($islandStretch | Where-Object { $_ -match "LOAD ${name}: main=frost" }).Count -ge 1) ($islandStretch | Select-Object -First 1)

	# The menu's own island.
	Check "the menu's island is no world" ((@($lines | Where-Object { $_ -like 'WORLD none*' }).Count -ge 4) -and -not (Test-Path (Join-Path $worlds 'L_Island')))

	# A new world.
	$alphaSettings = Json (Join-Path $worlds 'WT_Alpha\settings.json')
	Check 'New world makes a folder with the name and preset picked' ((Found "WORLDS made 'WT Alpha' \(pve\)") -and $alphaSettings -and $alphaSettings.preset -eq 'pve')
	Check 'Create and play hosts it' ((Found 'MENU host: L_Island\?listen.*\?World=WT_Alpha') -and (Found "WORLD 'WT_Alpha' .*preset pve"))
	$alphaWorld = Json (Join-Path $worlds 'WT_Alpha\world.json')
	$pieces = if ($alphaWorld) { @($alphaWorld.pieces | Where-Object { $_.piece -eq 'wood_foundation' }).Count } else { 0 }
	Check 'what is built there is saved in its folder' ($pieces -eq 1) "$pieces foundations"
	Check "and so is the character" (Test-Path (Join-Path $worlds "WT_Alpha\players\$name.json"))

	# A second world is separate.
	$beta = Stretch 'WT_Beta'
	Check 'a second world takes its own preset' ((Found "WORLDS made 'WT Beta' \(pvp_standard\)") -and (Found "WORLD 'WT_Beta' .*preset pvp_standard"))
	Check 'and starts empty' ($beta.Count -gt 0 -and @($beta | Where-Object { $_ -like 'WORLD loaded*' }).Count -eq 0 -and @($beta | Where-Object { $_ -like 'WORLD saved 0 pieces*' }).Count -ge 1) (($beta | Where-Object { $_ -like 'WORLD *' } | Select-Object -Last 2) -join ' / ')
	Check 'with a fresh character' ($beta.Count -gt 0 -and @($beta | Where-Object { $_ -like "LOAD $name*" }).Count -eq 0)

	# The list's buttons.
	Check 'Rename changes its name' (Found "WORLDS renamed 'WT Beta' to 'WT Gamma'")
	$keepAt = [array]::FindIndex([string[]]$lines, [Predicate[string]]{ param($l) $l -like 'CLICK "Keep it"*' })
	$deletedAt = [array]::FindIndex([string[]]$lines, [Predicate[string]]{ param($l) $l -like 'WORLDS deleted*WT_Beta*' })
	Check 'Delete asks first: Keep it keeps it' ($keepAt -ge 0 -and $deletedAt -gt $keepAt) "keep at $keepAt, deleted at $deletedAt"
	Check 'and Yes deletes its folder' ($deletedAt -ge 0 -and -not (Test-Path (Join-Path $worlds 'WT_Beta')))

	# Back to the first world in the same game.
	$again = Stretch 'WT_Alpha' 1
	Check 'going back, its buildings return' (@($again | Where-Object { $_ -like 'WORLD loaded 1 pieces*' }).Count -ge 1) (($again | Where-Object { $_ -like 'WORLD *' } | Select-Object -First 2) -join ' / ')
	Check 'and its character' (@($again | Where-Object { $_ -match "LOAD ${name}: main=fire" }).Count -ge 1)
	Check 'and its preset' (@($again | Select-Object -First 1 | Where-Object { $_ -match 'preset pve' }).Count -eq 1)
	$alphaInfo = Json (Join-Path $worlds 'WT_Alpha\info.json')
	Check 'the list knows when it was played and for how long' ($alphaInfo -and $alphaInfo.playSeconds -ge 5 -and $alphaInfo.version) $(if ($alphaInfo) { "$($alphaInfo.playSeconds) s, version $($alphaInfo.version)" })

	$missed = @($lines | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
	Check 'every click lands on its button' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
	# (As menutest: the engine's Dataflow plugin logs a struct warning as an error on some starts.)
	$errors = @(Select-String -Path $log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
	Check 'no errors in the log' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
}
finally {
	if ($c -and -not $c.HasExited) { Stop-Process -Id $c.Id -Force }
	Remove-TestWorlds
	Remove-Item $seed -ErrorAction SilentlyContinue
	Remove-Item -Recurse -Force $island -ErrorAction SilentlyContinue
	Remove-Item $marker -ErrorAction SilentlyContinue
	if (Test-Path (Join-Path $kept 'Island')) { Move-Item (Join-Path $kept 'Island') $island }
	if (Test-Path (Join-Path $kept 'marker.txt')) { Move-Item (Join-Path $kept 'marker.txt') $marker }
}

if ($Shots) {
	New-Item -ItemType Directory -Force $Shots | Out-Null
	Get-ChildItem $shotDir -Filter *.png | Where-Object { $shotsBefore -notcontains $_.FullName } | Sort-Object LastWriteTime | ForEach-Object { Move-Item $_.FullName (Join-Path $Shots $_.Name) -Force; Join-Path $Shots $_.Name }
}
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
