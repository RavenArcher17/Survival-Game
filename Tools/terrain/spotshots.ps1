# Screenshots of chosen spots on L_Island (world look checks: lava, resource nodes, caves, bushes, wildlife), from a
# rendered client driven by bot commands, as uishots.ps1. Each spot is "label=commands", commands separated by ';'
# (bot commands, wr.Cam X Y Z Yaw Pitch for a free camera, WRHour, ...); a scene picture follows each spot.
#   spotshots.ps1 -Port 7871 -Out <folder> -Spots 'lava_day=WRHour 12;wr.Cam -40000 85000 9000 90 -20', 'lava_night=WRHour 0.5'
# -Ui takes "shot showui" pictures (HUD included) instead of HighResShot.
param([int]$Port = 7838, [string]$Out = '', [string[]]$Spots = @(), [switch]$Ui, [int]$Res = 1600)
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-spotshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$name = 'SpotShots'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date
$h = [int]($Res * 9 / 16)
$shot = if ($Ui) { 'wait 0.6|shot showui|wait 1.5' } else { "wait 0.6|HighResShot ${Res}x$h|wait 1.5" }
$cmds = @('WRBotPause 1', 'wait 3', 'WRClearCreatures')
$labels = @()
foreach ($s in $Spots) {
	$label, $rest = $s -split '=', 2
	$cmds += ($rest -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
	$cmds += $shot
	$labels += $label
}
$cmds += 'wait 2'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=spotshots -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name} -game -windowed -ResX=$Res -ResY=$h -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
$deadline = (Get-Date).AddSeconds(180 + 25 * $labels.Count)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	$n = @(Get-ChildItem $shotDir -Filter '*creenshot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started }).Count
	if ($n -ge $labels.Count) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_spotshots.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$files = @(Get-ChildItem $shotDir -Filter '*creenshot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
	$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
	Move-Item $files[$i].FullName $dest -Force
	$dest
}
$errors = @(Select-String -Path "$logs\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Select-Object -First 5)
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
if ($errors) { "client errors:"; $errors | ForEach-Object { '  ' + $_.Line.Trim() } }
"{0} of {1} spots captured (logs: {2})" -f [Math]::Min($files.Count, $labels.Count), $labels.Count, $logs
$lock.ReleaseMutex()
