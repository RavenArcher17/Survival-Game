# Records a creature's scripted performance frame by frame, the same for every creature id and every run, so two runs
# (old and new clips, two creatures) can be laid side by side. A standalone game (no server) on L_CreatureStage (a flat
# floor beside a 20 degree ramp: Tools\make_map.ps1 stage), daytime, nothing else around. The game runs on a fixed time
# step (-UseFixedTimeStep -FPS=30 -FixedSeed): each frame is exactly 1/30 s of game time however long it takes to draw
# and save, and every frame is saved (WRRecord). The creature's brain is off (WRCreatureDrive and the rest); the camera
# frames it by its size (WRArtCam):
#    0.0 s  idle, side-on                            10.8  bite, 12.4 bite (standing)
#    3.0    walk +X 4 s (0.4 x its speed)             14.0  turns on the spot to face up the ramp (camera held)
#    7.0    run +X 3 s (its full speed)               15.2  walks up the ramp 4 s, camera from behind its right (110)
#   10.0    stops                                     19.8  flinch, 20.8 flinch, 22.0 dies, held to the end (25.2)
# Frames land in -Out as f0000.png ... with timing.csv (world time of every frame) and sheet.png (every -SheetEvery-th
# frame, labelled). Prints the frame count, where they are, and whether every frame step was 1/30 s.
#   creaturevideo.ps1 -Creature woolly_drudge [-Out <dir>] [-Res 1280x720] [-SheetEvery 30] [-MotionBlur]
#                     [-Tune anim.creatureFeet.enabled:0]   (tuning overrides for the run, e.g. feet IK off to compare)
param([Parameter(Mandatory = $true)][string]$Creature, [string]$Out = '', [string]$Res = '1280x720', [int]$SheetEvery = 30,
	[string]$Map = '/Game/Maps/L_CreatureStage', [switch]$MotionBlur, [string]$Tune = '', [int]$TimeoutSeconds = 0)
$ErrorActionPreference = 'Stop'
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
if ($Res -notmatch '^(\d+)x(\d+)$') { throw "-Res must look like 1280x720" }
$w = [int]$Matches[1]; $h = [int]$Matches[2]
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logs = Join-Path $env:TEMP "wildreach-creaturevideo-$Creature-$stamp"
if (-not $Out) { $Out = Join-Path $logs 'frames' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$fps = 30
$name = 'Filmer'
$prefix = "cv$($stamp -replace '-', '')_"
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue

# The performance: each step's commands, then how long it lasts (game seconds). Same for every creature.
$c = $Creature
$steps = @(
	@(@(), 3.0),                                                                  # idle, side-on
	@(@("WRCreatureDrive $c 0 4 0.4"), 4.0),                                      # walk
	@(@("WRCreatureDrive $c 0 3 1.0"), 3.0),                                      # run
	@(@(), 0.8),                                                                  # stop
	@(@("WRCreatureAttack $c"), 1.6),                                             # bite
	@(@("WRCreatureAttack $c"), 1.6),                                             # bite
	@(@('WRArtCamFix 1', "WRCreatureFace $c -90"), 1.2),                          # turn to the ramp (camera held)
	@(@('WRArtCamFix 0', "WRArtCam $c 110", "WRCreatureDrive $c -90 4 0.4"), 4.0),  # up the ramp, three-quarter from behind its right side
	@(@(), 0.6),                                                                  # stop on the slope
	@(@("WRCreatureFlinch $c"), 1.0),                                             # flinch
	@(@("WRCreatureFlinch $c"), 1.2),                                             # flinch
	@(@("WRCreatureHP $c 0"), 3.2)                                                # dies, held
)
$seconds = ($steps | ForEach-Object { $_[1] } | Measure-Object -Sum).Sum
$frames = [int][Math]::Ceiling($seconds * $fps)
# Setup: the filmer out of sight, a clean stage, mid-morning sun from behind the cameras; the creature in, its brain off,
# settled on the floor (and its feet on it) before the first frame.
$cmds = @('WRBotPause 1', 'wr.CreatureFeetLog 1', 'wait 1', 'WRHideMe 1', 'WRTeleport -2500 2500', 'wait 1', 'WRClearCreatures', 'WRHour 10', 'wait 3',
	"WRSpawnAt $c 0 0", "WRCreatureFace $c 0", 'wait 3', "WRArtCam $c 90", 'wait 2', "WRRecord $prefix $frames $w $h")
foreach ($s in $steps) { $cmds += $s[0]; $cmds += "wait $($s[1])" }
$cmds += 'wait 2'

# Crisp frames (no motion blur) unless asked.
$exec = if ($MotionBlur) { '' } else { '"-ExecCmds=r.MotionBlurQuality 0"' }
if ($Tune) { $exec += " -wrtune=$Tune" }
$gameArgs = "`"$project`" ${Map}?WRName=${name} -game -windowed -ResX=$w -ResY=$h -unattended -nosound -nosplash -wrbot -wrworld=creaturevideo " +
	"-UseFixedTimeStep -FPS=$fps -FixedSeed $exec `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\game.log`""
$game = Start-Process $ue -ArgumentList $gameArgs -PassThru
"recording $Creature ($frames frames, ${w}x$h) - game pid $($game.Id), log $logs\game.log"
$pattern = "$prefix*.png"
$deadline = (Get-Date).AddSeconds($(if ($TimeoutSeconds -gt 0) { $TimeoutSeconds } else { 240 + $frames * 1.5 }))
$done = $false
while ((Get-Date) -lt $deadline -and -not $game.HasExited) {
	Start-Sleep -Seconds 3
	$have = @(Get-ChildItem $shotDir -Filter $pattern -ErrorAction SilentlyContinue).Count
	if ($have -ge $frames -and (Test-Path (Join-Path $shotDir "${prefix}timing.csv"))) { Start-Sleep -Seconds 3; $done = $true; break }
}
if (-not $game.HasExited) { Stop-Process -Id $game.Id -Force }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_creaturevideo.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue

# Frames into -Out as f0000.png ..., the timing beside them.
Remove-Item (Join-Path $Out 'f[0-9][0-9][0-9][0-9].png') -ErrorAction SilentlyContinue
$moved = 0
for ($i = 0; $i -lt $frames; $i++) {
	$src = Join-Path $shotDir ('{0}{1:D4}.png' -f $prefix, $i)
	if (Test-Path $src) { Move-Item $src (Join-Path $Out ('f{0:D4}.png' -f $i)) -Force; $moved++ }
}
$csv = Join-Path $shotDir "${prefix}timing.csv"
if (Test-Path $csv) { Move-Item $csv (Join-Path $Out 'timing.csv') -Force }
Remove-Item (Join-Path $shotDir $pattern) -ErrorAction SilentlyContinue

# Every frame one fixed step of game time?
$timing = "no timing written"
if (Test-Path (Join-Path $Out 'timing.csv')) {
	$rows = @(Import-Csv (Join-Path $Out 'timing.csv'))
	$deltas = @($rows | Select-Object -Skip 1 | ForEach-Object { [double]$_.world_delta })
	$bad = @($deltas | Where-Object { [Math]::Abs($_ - 1.0 / $fps) -gt 1e-4 })
	$min = ($deltas | Measure-Object -Minimum).Minimum; $max = ($deltas | Measure-Object -Maximum).Maximum
	$real = [double]$rows[-1].real_seconds
	$timing = "frame step {0:F5}..{1:F5} s over {2} frames ({3} off 1/{4} s); {5:F1} s of game time took {6:F0} s to record" -f $min, $max, $rows.Count, $bad.Count, $fps,
		([double]$rows[-1].world_seconds - [double]$rows[0].world_seconds), $real
}

# Contact sheet: every SheetEvery-th frame, labelled with its number and time.
if ($moved -gt 0 -and $SheetEvery -gt 0) {
	Add-Type -AssemblyName System.Drawing
	$picks = @(for ($i = 0; $i -lt $frames; $i += $SheetEvery) { $i }) | Where-Object { Test-Path (Join-Path $Out ('f{0:D4}.png' -f $_)) }
	$tw = 384; $th = [int]($tw * $h / $w); $cols = 6; $rowsN = [Math]::Ceiling($picks.Count / $cols)
	$sheet = New-Object System.Drawing.Bitmap ($tw * $cols), ($th * $rowsN)
	$g = [System.Drawing.Graphics]::FromImage($sheet)
	$g.InterpolationMode = 'HighQualityBicubic'
	$g.Clear([System.Drawing.Color]::Black)
	$font = New-Object System.Drawing.Font 'Consolas', 11
	for ($k = 0; $k -lt $picks.Count; $k++) {
		$img = [System.Drawing.Image]::FromFile((Join-Path $Out ('f{0:D4}.png' -f $picks[$k])))
		$x = ($k % $cols) * $tw; $y = [Math]::Floor($k / $cols) * $th
		$g.DrawImage($img, $x, $y, $tw, $th)
		$g.DrawString(('{0} f{1:D4} {2:F1}s' -f $Creature, $picks[$k], ($picks[$k] / $fps)), $font, [System.Drawing.Brushes]::Yellow, $x + 4, $y + 4)
		$img.Dispose()
	}
	$sheet.Save((Join-Path $Out 'sheet.png'), [System.Drawing.Imaging.ImageFormat]::Png)
	$g.Dispose(); $sheet.Dispose()
}

$errors = @(Select-String -Path "$logs\game.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue |
	Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' } | Select-Object -First 8)
if ($errors) { "game errors:"; $errors | ForEach-Object { '  ' + $_.Line.Trim() } }
$said = @(Select-String -Path "$logs\game.log" -Pattern 'CMD .*creature(attack|hp) .*|RECORD (start|done).*' -ErrorAction SilentlyContinue | ForEach-Object { '  ' + ($_.Line -replace '^.*?(CMD|RECORD)', '$1') })
$said
"{0} of {1} frames in {2} (f0000.png ...; sheet.png every {3}th; timing.csv)" -f $moved, $frames, $Out, $SheetEvery
$timing
"log: $logs\game.log"
if ($moved -lt $frames) { exit 1 }
