# Builds Storm peaks into L_Island (cooks/build-biome-lightning.md 2.1-2.3):
#   1. node Tools/terrain/storm_peaks.mjs           heightmap, Data/storm_peaks.json, preview
#   2. M_StormAuto (commandlet)                      the landscape material
#   3. import_storm.py in the full editor, unattended (the landscape's layers render on the GPU in the editor's tick)
# Re-running replaces the earlier Storm peaks landscape and camps. One heavy Unreal job at a time.
param([switch]$SkipMaterial)
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Push-Location $root
node Tools/terrain/storm_peaks.mjs
node Tools/terrain/sea_mask.mjs
node Tools/terrain/storm_walk.mjs
python -I Tools/terrain/world_map.py
Pop-Location
$cmd = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = Join-Path $root 'Wildreach.uproject'
if (-not $SkipMaterial) {
	$env:WR_MATERIALS = 'storm'
	& $cmd "$proj" -run=pythonscript -script="$(Join-Path $PSScriptRoot 'build_terrain_materials.py')" -unattended -nosplash -nullrhi | Select-String '\[Wildreach\]|Error' | ForEach-Object { $_.Line }
	Remove-Item Env:WR_MATERIALS
}
$log = Join-Path $env:TEMP 'wildreach-build-storm.log'
$p = Start-Process $ue -ArgumentList "`"$proj`" -ExecutePythonScript=`"$(Join-Path $PSScriptRoot 'import_storm.py')`" -unattended -nosplash -windowed -ResX=1280 -ResY=720 -abslog=`"$log`"" -PassThru
if (-not $p.WaitForExit(2700000)) { Stop-Process -Id $p.Id -Force; Write-Error 'build timed out after 45 minutes' }
Select-String -Path $log -Pattern '\[Wildreach\]|ImportLandscapeR16|LogPython: Error|Traceback' | ForEach-Object { $_.Line }
exit $p.ExitCode
