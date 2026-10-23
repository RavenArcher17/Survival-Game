# Runs the Wildreach automation tests headless and prints a pass/fail summary.
#   test.ps1            all Wildreach.* tests
#   test.ps1 Paths      just Wildreach.Paths.*
param([string]$Filter = '')
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$log = Join-Path $env:TEMP 'wildreach-tests.log'
$name = if ($Filter) { "Wildreach.$Filter" } else { 'Wildreach' }
& $ue $project "-ExecCmds=Automation RunTests $name;Quit" -unattended -nullrhi -nosplash -nosound "-abslog=$log" | Out-Null
$results = Select-String -Path $log -Pattern 'Test Completed\. Result=\{(\w+)\} Name=\{([^}]+)\}'
$failed = 0
foreach ($r in $results) {
    $status = $r.Matches[0].Groups[1].Value; $test = $r.Matches[0].Groups[2].Value
    if ($status -ne 'Success') { $failed++ }
    "{0,-8} {1}" -f $status, $test
}
Select-String -Path $log -Pattern 'Error: .*Expected|LogAutomationController: Error' | ForEach-Object { $_.Line.Trim() }
"{0} tests, {1} failed (log: {2})" -f $results.Count, $failed, $log
if ($results.Count -eq 0 -or $failed -gt 0) { exit 1 }
