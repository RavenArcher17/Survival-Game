# A base by real keys and clicks, as the host on L_Island at 16:10 (the narrowest common screen, where the build bar's
# tabs are tightest): the build bar pages its cards ("1–3 of 6", the › arrow) and the claim totem and Hearth have their
# own Claim tab (0.6.2 hid them past a 4 px scroll bar); the first claim brings the hint about the Hearth; a Hearth
# broken by hits stays standing, broken, and its screen's Repair mends it; a chest renamed through its Rename box; a
# waypoint set by right-clicking the map shows there and on the HUD compass.
# The tests used to place the Hearth by command (WRGhost / WRPlace), never through the bar, so none of them saw it was
# out of sight. -Shots keeps the pictures; -Log re-checks the client log of an earlier run.
#   basetest.ps1 [-Port 7896] [-Shots] [-Out <folder>] [-Log <client.log>]
param([int]$Port = 7896, [switch]$Shots, [string]$Out = '', [string]$Log = '')
$name = 'Mason'
$X = -132000; $Y = -56400; $G = 4794
function Shot { if ($Shots) { @('wait 0.8', 'shot showui', 'wait 1.5') } else { @() } }
$cmds = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'WRHour 11', 'wait 2', 'WRGive wood 300', 'WRGive stone 300', 'WRGive copper_ingot 12',
	"WRTeleport $($X - 600) $Y", 'wait 8', 'WRClearCreatures', 'WRLook 0 -8') +
	# The bar: Stations' first page and the arrow on, page two, the Claim tab.
	@('WRKey B', 'wait 1.5', 'WRClick Stations', 'wait 1') + (Shot) + @('WRUIText', 'WRClick ›', 'wait 1') + (Shot) + @('WRUIText', 'WRClick Claim', 'wait 1') + (Shot) + @('WRUIText', 'WRKey B', 'wait 1') +
	# The claim totem, then hints and moments on long enough for the Hearth hint's turn (each hint stays 9 s).
	@('WRGhost claim_totem', "WRLookAt $($X + 1400) $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5', 'WRFlow quiet off', 'WRFlow hints on', 'wait 22') + (Shot) + @('WRFlow hints off', 'WRFlow quiet on') +
	# The Hearth, broken by hits, its screen by E, Repair.
	@('WRGhost hearth', "WRLookAt $X $Y $G", 'wait 1', 'WRGhostPlace', 'wait 1.5', "WRTeleport $($X - 260) $Y", 'wait 2', 'WRHitPiece 170', 'wait 1.5', 'WRLook 0 -20', 'WRKey E', 'wait 1.5') + (Shot) +
	@('WRUIText', 'WRClick Repair', 'wait 1.5') + (Shot) + @('WRUIText', 'WRKey Escape', 'wait 1') +
	# A chest, renamed through its box.
	@("WRTeleport $($X - 600) $Y", 'wait 1.5', 'WRGhost storage_chest', "WRLookAt $X $($Y + 700) $G", 'wait 1', 'WRGhostPlace', 'wait 1.5', "WRTeleport $X $($Y + 500)", 'wait 2', 'WRLook 90 -25',
		'WRKey E', 'wait 1.5', 'WRClick Rename', 'wait 1') + (Shot) + @('WRType Ore and metal', 'wait 0.5', 'WRKey Enter', 'wait 1.5') + (Shot) + @('WRUIText', 'WRKey Escape', 'wait 1') +
	# The map: a right-click sets the waypoint; the compass shows it.
	@('WRMenu map 0', 'wait 2', 'WRMouseClick 0.40 0.38 rmb once', 'wait 1.5') + (Shot) + @('WRUIText', 'WRMenu none 0', 'wait 1.5', 'WRLook 300 -5', 'wait 1') + (Shot) + @('WRUIText', 'wait 1', 'WRSay basetest-done')
$shotNames = @('stations_page1', 'stations_page2', 'claim_tab', 'hint', 'hearth_broken', 'hearth_repaired', 'chest_name_box', 'chest_named', 'map_waypoint', 'compass_waypoint')

if (-not $Log) {
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
	$env:UE_SKIP_UBT_SDK_SETUP = '1'
	$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
	$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
	$root = Split-Path $project
	$logs = Join-Path $env:TEMP ("wildreach-base-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
	if (-not $Out) { $Out = Join-Path $logs 'shots' }
	New-Item -ItemType Directory -Force $logs | Out-Null
	$db = Join-Path $root 'Saved\Worlds\basetest'
	Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
	# Hints are remembered per player name: forget this one's so the Hearth hint can come.
	$ini = Join-Path $root 'Saved\Config\WindowsEditor\GameUserSettings.ini'
	if (Test-Path $ini) { $t = [IO.File]::ReadAllText($ini); [IO.File]::WriteAllText($ini, ($t -replace "(?m)^Hints_$name=.*\r?\n", '')) }
	$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
	$started = Get-Date
	$c = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=${name}?Path=fire?Body=m -game -windowed -ResX=1440 -ResY=900 -unattended -nosound -port=$Port -wrworld=basetest -wrtune=wildlife.enabled:0 -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
	$deadline = (Get-Date).AddSeconds(300)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
		Start-Sleep -Seconds 3
		if (Select-String -Path "$logs\client.log" -Pattern "chat ${name}: basetest-done" -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
	}
	if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
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
	$Log = "$logs\client.log"
}

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$lines = @(Get-Content $Log -Encoding UTF8 | Where-Object { $_ -match 'LogWildreach: ' } | ForEach-Object { ($_ -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
$lines | Where-Object { $_ -match '^(CLICK|MOUSECLICK|NOTICE|HEARTH|CHEST|WAYPOINT|BUILD|FLOW hint)' } | ForEach-Object { "  $_" }
$texts = @($lines | Where-Object { $_ -like 'UITEXT*' } | ForEach-Object { ($_ -replace '^UITEXT\s+-?\d+,\s*-?\d+ ', '') })
$notices = @($lines | Where-Object { $_ -like 'NOTICE *' } | ForEach-Object { $_.Substring(7) })
Check 'the Stations tab shows its first page and how many there are' ($texts -contains '1–3 of 6')
Check 'the › arrow turns to the next page' ($texts -contains '4–6 of 6')
Check 'the Claim tab holds the claim totem and the Hearth' (($texts -contains 'Land claim totem') -and ($texts -contains 'Hearth'))
Check 'the first claim brings the hint about the Hearth' (@($lines | Where-Object { $_ -like 'FLOW hint hearth:*Claim tab*' }).Count -eq 1)
Check 'a Hearth hit to nothing breaks and stays standing' ((@($lines | Where-Object { $_ -like 'BUILD * (hearth) broken by *' }).Count -eq 1) -and ($notices -contains 'hearth hp 0'))
Check 'its screen says broken and offers Repair at its cost' (($texts -contains 'Hearth health · broken') -and @($texts | Where-Object { $_ -like 'Mend it so its stations work again: 15 stone*' }).Count -ge 1)
Check 'Repair mends it' ((@($lines | Where-Object { $_ -like 'HEARTH * repaired by Mason (was broken)' }).Count -eq 1) -and ($notices -contains 'The Hearth is mended and burning: its stations work again'))
Check 'Rename opens the name box' (@($lines | Where-Object { $_ -eq 'CHEST name box open' }).Count -eq 1)
Check 'the chest takes the typed name' (($notices -contains 'Chest named "Ore and metal"') -and ($texts -contains 'Ore and metal'))
Check 'a right-click on the map sets the waypoint' (@($lines | Where-Object { $_ -like 'WAYPOINT set at *' }).Count -eq 1 -and @($lines | Where-Object { $_ -eq 'WAYPOINT cleared' }).Count -eq 0)
$missed = @($lines | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
Check 'every click lands on its button' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
$errors = @(Select-String -Path $Log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the client log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (log: {2})" -f $results.Count, $failed, $Log
if ($failed -gt 0) { exit 1 }
