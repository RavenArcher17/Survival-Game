# Animation captures: short frame sequences (default 8 frames over a second) of the survivor and creatures, each shot
# from two camera angles (side and three-quarter, WRArtCam), on the combat arena. The action is played again for each
# angle. Frames land in -Out as <sequence>_<angle>_<nn>.png, plus one contact sheet per sequence and angle
# (sheet_<sequence>_<angle>.png) to look through. One run at a time machine-wide (shares uishots' lock).
#   animshots.ps1 [-Out <folder>] [-Port 7861] [-Only block_walk,strafe_left] [-Body f] [-Night]
param([string]$Out = '', [int]$Port = 7861, [string[]]$Only = @(), [string]$Body = '', [int]$Frames = 8, [double]$Every = 0.125, [switch]$Night)
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-animshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$name = 'Animator'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue

# Each sequence: what to set up, the action (started just before the frames), how long to wait into it, the angles
# (degrees off the subject's nose: 90 = its right side, 35 = three-quarter front), who to frame, and fix = hold the
# camera still once placed (turns read better against a still camera).
$hold = 'WRHeal'
$seqs = [ordered]@{
	'block_walk'   = @{ act = 'WRPress block 2.8|WRAnimDrive 0 2.8 1 0'; lead = 0.9 }
	'strafe_left'  = @{ act = 'WRPress block 2.8|WRAnimDrive -90 2.8 1 0'; lead = 0.9 }
	'strafe_right' = @{ act = 'WRPress block 2.8|WRAnimDrive 90 2.8 1 0'; lead = 0.9 }
	'walk_back'    = @{ act = 'WRPress block 2.8|WRAnimDrive 180 2.8 1 0'; lead = 0.9 }
	'light_run'    = @{ act = 'WRAnimDrive 0 2.6 1 0|wait 0.8|WRPress light 0.05'; lead = 0.0 }
	'heavy_run'    = @{ act = 'WRAnimDrive 0 2.6 1 0|wait 0.8|WRPress heavy 0.05'; lead = 0.0 }
	'cast_move'    = @{ act = 'WRAnimDrive 0 2.6 1 0|wait 0.8|WRCast 1'; lead = 0.0 }
	'turn_place'   = @{ act = 'WRPress block 3.5|wait 0.4|WRAnimDrive 0 0.5 0 200'; lead = 0.2; fix = 1 }
	'turn_slow'    = @{ act = 'WRPress block 4|wait 0.3|WRAnimDrive 0 3.5 0 45'; lead = 1.0; fix = 1; frames = 16 }
	'bow_aimwalk'  = @{ setup = 'WRRanged 1|wait 1.5|WRAim 1|wait 0.8'; act = 'WRAnimDrive 0 2.6 0.6 0'; lead = 0.9; after = 'WRAim 0|WRRanged 0|wait 1' }
	'bow_strafe'   = @{ setup = 'WRRanged 1|wait 1.5|WRAim 1|wait 0.8'; act = 'WRAnimDrive -90 2.6 0.6 0'; lead = 0.9; after = 'WRAim 0|WRRanged 0|wait 1' }
	'bow_back'     = @{ setup = 'WRRanged 1|wait 1.5|WRAim 1|wait 0.8'; act = 'WRAnimDrive 180 2.6 0.6 0'; lead = 0.9; after = 'WRAim 0|WRRanged 0|wait 1' }
	'run'          = @{ act = 'WRAnimDrive 0 2.6 1 0'; lead = 0.9 }
	# Creatures: a Basaltback wandering (walk, turns), a Cinderfang running in and biting, a Magmaw (spit, lunge).
	'basaltback'   = @{ setup = 'WRClearCreatures|WRSpawnAt basaltback 1200 3200|wait 4'; who = 'basaltback'; lead = 0.3; frames = 24; fix = 1 }
	'cinderfang'   = @{ setup = 'WRClearCreatures|WRSpawnAt cinderfang -1300 2500|wait 0.3'; who = 'cinderfang'; lead = 0.6; frames = 24; act = 'WRAnimDrive 90 3 0.5 0' }
	'magmaw'       = @{ setup = 'WRClearCreatures|WRSpawnAt magmaw 0 1100|wait 0.3'; who = 'magmaw'; lead = 0.5; frames = 24 }
}
if ($Only.Count -gt 0) { $Only = $Only | ForEach-Object { $_ -split ',' } }
$angles = @(90, 35)
$cmds = @('WRBotPause 1', 'wr.AnimFootSlide 1', 'wait 3', 'WRGive hunting_bow 1', 'WRGive stone_arrow 20', 'WREquip hunting_bow', 'WRGive iron_armor 1', 'WREquip iron_armor', 'WRGivePoints fire 10', 'WRAutoSpend fire 10',
	'WRTeleport 0 2500', 'wait 2', 'WRClearCreatures', 'WRLook 0 -10', 'wait 1')
if ($Night) { $cmds += 'WRHour 23', 'wait 2' } else { $cmds += 'WRHour 12', 'wait 2' }
$labels = @()
$frameNames = @()
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
Remove-Item (Join-Path $shotDir 'as*_*.png') -ErrorAction SilentlyContinue
foreach ($s in $seqs.GetEnumerator()) {
	if ($Only.Count -gt 0 -and -not ($Only -contains $s.Key)) { continue }
	$v = $s.Value
	$who = if ($v.who) { $v.who } else { 'me' }
	$n = if ($v.frames) { $v.frames } else { $Frames }
	foreach ($a in $angles) {
		$cmds += 'WRTeleport 0 2500', 'wait 1.2', $hold, 'WRLook 0 -10'
		if ($v.setup) { $cmds += $v.setup }
		$cmds += "WRArtCam $who $a", 'wait 0.8'
		if ($v.fix) { $cmds += 'WRArtCamFix 1' }
		if ($v.act) { $cmds += $v.act }
		if ($v.lead -gt 0) { $cmds += "wait $($v.lead)" }
		# One command takes the whole sequence (WRShotSeq), short names renamed into -Out afterwards: the command list
		# goes on the client's command line, which has a length limit.
		$prefix = 'as{0:D2}_' -f $labels.Count
		for ($i = 0; $i -lt $n; $i++) { $frameNames += ,@(('{0}{1:D2}' -f $prefix, $i), ('{0}_{1}_{2:D2}.png' -f $s.Key, $a, ($i + 1))) }
		$cmds += "WRShotSeq $prefix $n $Every", $hold, 'wait 0.3'  # the bot holds its next command until the shots are done
		$labels += '{0}_{1}' -f $s.Key, $a
		$cmds += 'WRArtCamFix 0', 'WRArtCam me 60', 'wait 1.5'
		if ($v.after) { $cmds += $v.after }
	}
}
$cmds += 'WRClearCreatures', 'wait 2'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=animshots `"-LogCmds=LogWildreach Verbose`" -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$bodyArg = if ($Body) { "?Body=$Body" } else { '' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}$bodyArg -game -windowed -ResX=1280 -ResY=720 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-LogCmds=LogWildreach Verbose`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
$expected = $frameNames.Count
$deadline = (Get-Date).AddSeconds(150 + 10 * $labels.Count + $expected * 0.3)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (@(Get-ChildItem $shotDir -Filter 'as*_*.png' -ErrorAction SilentlyContinue).Count -ge $expected) { Start-Sleep -Seconds 4; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_animshots.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
foreach ($f in $frameNames) {
	$src = Join-Path $shotDir ($f[0] + '.png')
	if (Test-Path $src) { Move-Item $src (Join-Path $Out $f[1]) -Force }
}

# Contact sheets: each sequence's frames 8 to a sheet (4 across), small enough to look through one by one.
Add-Type -AssemblyName System.Drawing
foreach ($label in $labels) {
	$shots = @(Get-ChildItem $Out -Filter "${label}_*.png" | Where-Object { $_.BaseName -match "^$([regex]::Escape($label))_\d+$" } | Sort-Object Name)
	for ($start = 0; $start -lt $shots.Count; $start += 8) {
		$part = @($shots[$start..([Math]::Min($start + 7, $shots.Count - 1))])
		$w = 480; $h = 270; $cols = 4; $rows = [Math]::Ceiling($part.Count / $cols)
		$sheet = New-Object System.Drawing.Bitmap ($w * $cols), ($h * $rows)
		$g = [System.Drawing.Graphics]::FromImage($sheet)
		$g.InterpolationMode = 'HighQualityBicubic'
		$font = New-Object System.Drawing.Font 'Consolas', 11
		for ($i = 0; $i -lt $part.Count; $i++) {
			$img = [System.Drawing.Image]::FromFile($part[$i].FullName)
			$x = ($i % $cols) * $w; $y = [Math]::Floor($i / $cols) * $h
			$g.DrawImage($img, $x, $y, $w, $h)
			$g.DrawString(('{0} #{1}' -f $label, ($start + $i + 1)), $font, [System.Drawing.Brushes]::Yellow, $x + 4, $y + 4)
			$img.Dispose()
		}
		$suffix = if ($shots.Count -gt 8) { '_{0}' -f ($start / 8 + 1) } else { '' }
		$sheet.Save((Join-Path $Out "sheet_$label$suffix.png"), [System.Drawing.Imaging.ImageFormat]::Png)
		$g.Dispose(); $sheet.Dispose()
	}
}
$errors = @(Select-String -Path "$logs\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None|LogAnimation: Warning' -ErrorAction SilentlyContinue | Select-Object -First 8)
if ($errors) { "client errors / animation warnings:"; $errors | ForEach-Object { '  ' + $_.Line.Trim() } }
# How far planted feet slid over the ground while moving (wr.AnimFootSlide): near 0 = the feet keep up with the ground.
Select-String -Path "$logs\client.log" -Pattern 'FOOTSLIDE (.*)' -ErrorAction SilentlyContinue | ForEach-Object { $_.Matches[0].Groups[1].Value } | Set-Content "$Out\footslide.txt"
$got = @(Get-ChildItem $Out -Filter '*.png' | Where-Object { $_.Name -notlike 'sheet_*' }).Count
"{0} of {1} frames captured in {2} (logs: {3})" -f $got, $expected, $Out, $logs
$lock.ReleaseMutex()
