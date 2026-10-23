# Builds the first-playable island map (L_Island) from the approved layout.
#   build_island.ps1          export the heightmap if missing, then build the map if it doesn't exist
#   build_island.ps1 -Force   re-export and rebuild from scratch
param([switch]$Force)
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$height = Join-Path $root 'Art\Terrain\island_height.r16'
if ($Force -or -not (Test-Path $height)) {
	Push-Location $root
	node Tools/terrain/layout_proposal.mjs Docs/terrain 7 --export Art/Terrain/island_height.r16
	Pop-Location
}
if ($Force) {
	# World Partition maps keep their actors in __ExternalActors__/__ExternalObjects__ folders.
	Remove-Item (Join-Path $root 'Content\Maps\L_Island.umap') -ErrorAction SilentlyContinue
	foreach ($d in '__ExternalActors__', '__ExternalObjects__') {
		Remove-Item (Join-Path $root "Content\$d\Maps\L_Island") -Recurse -Force -ErrorAction SilentlyContinue
	}
}
# Runs in the full editor (a window opens and closes by itself): landscape edit layers build the
# visible heightmap textures on the GPU during the editor's tick, which a commandlet doesn't do.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$script = Join-Path $PSScriptRoot 'import_island.py'
$log = Join-Path $env:TEMP 'wildreach-build-island.log'
$p = Start-Process $ue -ArgumentList "`"$(Join-Path $root 'Wildreach.uproject')`" -ExecutePythonScript=`"$script`" -unattended -nosplash -windowed -ResX=1280 -ResY=720 -abslog=`"$log`"" -PassThru
if (-not $p.WaitForExit(1800000)) { Stop-Process -Id $p.Id -Force; Write-Error 'build timed out after 30 minutes' }
Select-String -Path $log -Pattern '\[Wildreach\]|ImportLandscapeR16|LogPython: Error|Traceback' | ForEach-Object { $_.Line }
exit $p.ExitCode
