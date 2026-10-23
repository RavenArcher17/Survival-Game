# Builds a test map headless inside the editor.
#   make_map.ps1            -> L_Wildreach (World Partition 4x4 km grid)
#   make_map.ps1 arena      -> L_CombatArena (combat test arena)
#   make_map.ps1 arena -Force  rebuilds it
param([ValidateSet('world', 'arena')] [string]$Which = 'world', [switch]$Force)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $PSScriptRoot '..\Wildreach.uproject' | Resolve-Path
$file = if ($Which -eq 'arena') { 'make_arena_map.py' } else { 'make_test_map.py' }
$script = Join-Path $PSScriptRoot $file | Resolve-Path
$scriptArg = if ($Force) { "$script -force" } else { "$script" }
& $ue $project -run=pythonscript "-script=$scriptArg" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
exit $LASTEXITCODE
