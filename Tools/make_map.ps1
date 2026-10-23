# Builds a test map headless inside the editor.
#   make_map.ps1               -> L_Wildreach (World Partition 4x4 km grid)
#   make_map.ps1 arena         -> L_CombatArena (combat test arena)
#   make_map.ps1 arena -Force  rebuilds it
param([ValidateSet('world', 'arena')] [string]$Which = 'world', [switch]$Force)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $PSScriptRoot '..\Wildreach.uproject' | Resolve-Path
$file = if ($Which -eq 'arena') { 'make_arena_map.py' } else { 'make_test_map.py' }
$script = Join-Path $PSScriptRoot $file | Resolve-Path
$env:WR_FORCE = if ($Force) { '1' } else { '0' }
if ($Force -and $Which -eq 'arena') {
	# Deleting a map asset from the python commandlet is unreliable; remove the file instead.
	Remove-Item (Join-Path $PSScriptRoot '..\Content\Maps\L_CombatArena.umap') -ErrorAction SilentlyContinue
}
& $ue $project -run=pythonscript "-script=$script" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
$code = $LASTEXITCODE
Remove-Item Env:WR_FORCE
exit $code
