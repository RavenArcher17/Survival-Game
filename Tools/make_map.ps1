# Runs Tools/make_test_map.py headless inside the editor.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $PSScriptRoot '..\Wildreach.uproject' | Resolve-Path
$script = Join-Path $PSScriptRoot 'make_test_map.py' | Resolve-Path
& $ue $project -run=pythonscript "-script=$script" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
exit $LASTEXITCODE
