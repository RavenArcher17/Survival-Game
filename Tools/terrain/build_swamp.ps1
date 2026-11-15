# Builds the Swamp into L_Island (cooks/build-biome-swamp.md 2.1-2.2):
#   1. node Tools/terrain/swamp.mjs                 heightmap, Data/swamp.json, preview, dressing rows
#   2. the sea mask, the wildlife walk map and the journal map over every land
#   3. M_SwampAuto (commandlet)                     the landscape material
#   4. import_swamp.py in the full editor, unattended (the landscape's layers render on the GPU in the editor's tick)
# Needs Art/Terrain/storm_height.r16 (Tools/terrain/storm_peaks.mjs) and island_height.r16. Re-running replaces the
# earlier Swamp landscape and camps. One heavy Unreal job at a time.
param([switch]$SkipMaterial, [switch]$SkipImport)
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Push-Location $root
node Tools/terrain/swamp.mjs
node Tools/terrain/sea_mask.mjs
node Tools/terrain/storm_walk.mjs
python -I Tools/terrain/world_map.py
Pop-Location
if ($SkipImport) { exit 0 }
$cmd = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = Join-Path $root 'Wildreach.uproject'
if (-not $SkipMaterial) {
	$env:WR_MATERIALS = 'swamp'
	& $cmd "$proj" -run=pythonscript -script="$(Join-Path $PSScriptRoot 'build_terrain_materials.py')" -unattended -nosplash -nullrhi | Select-String '\[Wildreach\]|Error' | ForEach-Object { $_.Line }
	Remove-Item Env:WR_MATERIALS
}
$log = Join-Path $env:TEMP 'wildreach-build-swamp.log'
$p = Start-Process $ue -ArgumentList "`"$proj`" -ExecutePythonScript=`"$(Join-Path $PSScriptRoot 'import_swamp.py')`" -unattended -nosplash -windowed -ResX=1280 -ResY=720 -abslog=`"$log`"" -PassThru
if (-not $p.WaitForExit(2700000)) { Stop-Process -Id $p.Id -Force; Write-Error 'build timed out after 45 minutes' }
Select-String -Path $log -Pattern '\[Wildreach\]|ImportLandscapeR16|LogPython: Error|Traceback' | ForEach-Object { $_.Line }
exit $p.ExitCode
