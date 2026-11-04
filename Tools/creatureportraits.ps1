# Art review: each creature alone on L_CreatureStage (grey 1 m checker floor), held still with its AI off, shot
# straight from the front and from its right side, then one picture per creature with both views stacked (phone-sized).
# Standalone game, so the camera always finds the creature it was asked for and the survivor stays hidden.
#   powershell -File Tools\creatureportraits.ps1 [-Out <dir>] [-Hour 10] [-Frame 2.2] [id1 id2 ...]
#   (no ids: every creature with art; -Frame: the camera's distance in model sizes for every one of them, wider for a
#   giant: the frame's fixed margin is a smaller share of a big model)
# -> <Out>\<nn>_<id>.png, the paths printed at the end. One run at a time machine-wide (shares uishots' lock).
[CmdletBinding(PositionalBinding = $false)]
param([string]$Out = '', [double]$Hour = 10, [double]$Frame = 0, [Parameter(ValueFromRemainingArguments = $true)][string[]]$Ids)
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logs = Join-Path $env:TEMP "wildreach-portraits-$stamp"
if (-not $Out) { $Out = Join-Path $logs 'portraits' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$name = 'Portraits'

$data = Get-Content (Join-Path $root 'Data\creatures.json') -Raw | ConvertFrom-Json
if (-not $Ids -or $Ids.Count -eq 0) {
	$Ids = @($data.creatures | Where-Object { $_.mesh } | ForEach-Object { $_.id })
}
# The upright Mixamo bosses hunch, and the default frame cuts off their heads.
$Wide = @{ vulkarn = 2.2; grimjaw = 2.2; obsidian_warden = 2.2 }
$bosses = @($data.creatures | Where-Object { $_.archetype -eq 'boss' } | ForEach-Object { $_.id })
$prefix = "pt$($stamp -replace '-', '')_"
$cmds = @('WRBotPause 1', 'wait 1', 'WRHideMe 1', "WRHour $Hour", 'wait 3')
# Where each one stands (away from the ramp, which runs along y < 0). Creatures share one spot, each removed before the
# next (the clear reaches 30 m round the hidden survivor). A boss raises arena walls that stay, so each boss gets its own
# spot, every one further toward +X: the front camera looks toward +X, so earlier walls stay behind it.
$bossX = -12000
foreach ($id in $Ids) {
	$boss = $bosses -contains $id
	if ($boss) { $x = $bossX; $bossX += 6000 } else { $x = -18000 }
	$y = 8000
	# A boss takes a few seconds to rise into its arena before the camera can find it. HighResShot captures on the next
	# rendered frame: wait before moving the camera again.
	$settle = if ($boss) { 'wait 6' } else { 'wait 2.5' }
	# Any model that needs a wider frame: id = sizes (the art camera's distance in model sizes).
	$frame = if ($Frame -gt 0) { $Frame } elseif ($Wide.ContainsKey($id)) { $Wide[$id] } else { 1.5 }
	# A boss wakes when anyone alive comes within its aggro radius (up to 2500), hidden or not, and fights through the
	# shots: park the survivor well outside it, and bring it back for the clear (which reaches 30 m round it). Others park
	# past the HUD's 25 m nameplate range: a small creature's nameplate sits inside the frame (Glowmoth's did).
	$park = if ($boss) { "$x $($y + 4500)" } else { "$($x - 2700) $y" }
	$cmds += 'WRHideMe 1', "WRTeleport $park", 'wait 0.8', "WRSpawnAt $id $x $y", 'wait 0.5', "WRCreatureFace $id 0", $settle, 'WRHideMe 1', "WRTuneSet artCam.sizes $frame",
		"WRArtCam $id 0", 'wait 1.2', "HighResShot 1600x900 filename=$prefix${id}_front", 'wait 1',
		"WRArtCam $id 90", 'wait 1.5', "HighResShot 1600x900 filename=$prefix${id}_side", 'wait 1',
		"WRTeleport $($x - 1200) $y", 'wait 0.3', 'WRClearCreatures', 'wait 0.5'
}
$cmds += 'wait 2'
# Tighter framing than the art camera's default; burrowers stay up out of the ground.
$tune = 'artCam.sizes:1.5,artCam.extra:60,creature.burrowHide:0'
$gameArgs = "`"$project`" /Game/Maps/L_CreatureStage?WRName=$name -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -nosplash -wrbot " +
	"-wrworld=portraits -wrtune=$tune `"-ExecCmds=r.MotionBlurQuality 0`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\game.log`""
$game = Start-Process $ue -ArgumentList $gameArgs -PassThru
"shooting $($Ids.Count) creatures - game pid $($game.Id), log $logs\game.log"
$last = Join-Path $shotDir "$prefix$($Ids[-1])_side.png"
$deadline = (Get-Date).AddSeconds(120 + 16 * $Ids.Count)
while ((Get-Date) -lt $deadline -and -not $game.HasExited -and -not (Test-Path $last)) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 3
if (-not $game.HasExited) { Stop-Process -Id $game.Id -Force }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_portraits.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue

# One picture per creature: its name, the front view, then the side view.
Add-Type -AssemblyName System.Drawing
$W = 1200; $H = 675; $bar = 64
$font = New-Object System.Drawing.Font 'Segoe UI', 26, ([System.Drawing.FontStyle]::Bold)
$small = New-Object System.Drawing.Font 'Segoe UI', 20
$made = @()
for ($i = 0; $i -lt $Ids.Count; $i++) {
	$id = $Ids[$i]
	$views = @('front', 'side') | ForEach-Object { Join-Path $shotDir "$prefix${id}_$_.png" }
	if (-not ((Test-Path $views[0]) -and (Test-Path $views[1]))) { "missing shots for $id"; continue }
	$pic = New-Object System.Drawing.Bitmap $W, ($bar + 2 * $H)
	$g = [System.Drawing.Graphics]::FromImage($pic)
	$g.InterpolationMode = 'HighQualityBicubic'
	$g.Clear([System.Drawing.Color]::FromArgb(18, 18, 18))
	$g.DrawString(('{0}. {1}' -f ($i + 1), $id), $font, [System.Drawing.Brushes]::White, 14, 10)
	for ($k = 0; $k -lt 2; $k++) {
		$img = [System.Drawing.Image]::FromFile($views[$k])
		$cw = $img.Width; $ch = [int]($cw * $H / $W)
		$src = New-Object System.Drawing.Rectangle ([int](($img.Width - $cw) / 2)), ([int](($img.Height - $ch) * 0.55)), $cw, $ch
		$dst = New-Object System.Drawing.Rectangle 0, ($bar + $k * $H), $W, $H
		$g.DrawImage($img, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
		$g.FillRectangle((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(150, 0, 0, 0))), 0, ($bar + $k * $H), 110, 40)
		$g.DrawString(@('Front', 'Side')[$k], $small, [System.Drawing.Brushes]::Yellow, 10, ($bar + $k * $H + 4))
		$img.Dispose()
	}
	$file = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $id)
	$pic.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
	$g.Dispose(); $pic.Dispose()
	$made += $file
}
Remove-Item (Join-Path $shotDir "$prefix*.png") -ErrorAction SilentlyContinue
$made
"$($made.Count) of $($Ids.Count) creatures in $Out"
