# Builds a test map headless inside the editor.
#   make_map.ps1               -> L_Wildreach (World Partition 4x4 km grid)
#   make_map.ps1 arena         -> L_CombatArena (combat test arena)
#   make_map.ps1 stage         -> L_CreatureStage (flat floor beside a 20 degree ramp: Tools\creaturevideo.ps1)
#   make_map.ps1 arena -Force  rebuilds it
param([ValidateSet('world', 'arena', 'stage')] [string]$Which = 'world', [switch]$Force)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $PSScriptRoot '..\Wildreach.uproject' | Resolve-Path
$file = @{ world = 'make_test_map.py'; arena = 'make_arena_map.py'; stage = 'make_stage_map.py' }[$Which]
$script = Join-Path $PSScriptRoot $file | Resolve-Path
$env:WR_FORCE = if ($Force) { '1' } else { '0' }
$maps = @{ arena = 'L_CombatArena'; stage = 'L_CreatureStage' }
if ($Force -and $maps.ContainsKey($Which)) {
	# Deleting a map asset from the python commandlet is unreliable; remove the file instead.
	Remove-Item (Join-Path $PSScriptRoot "..\Content\Maps\$($maps[$Which]).umap") -ErrorAction SilentlyContinue
}
& $ue $project -run=pythonscript "-script=$script" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
$code = $LASTEXITCODE
Remove-Item Env:WR_FORCE
exit $code
