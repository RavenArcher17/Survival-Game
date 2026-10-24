# Builds Unreal Engine 5.8.4 from source into C:\Dev\UE5Source (needed only for packaged dedicated
# servers; keep developing on the launcher engine). Takes several hours and ~200 GB. Safe to rerun:
# each step skips what's already done. Log: C:\Dev\UE5Source-build.log
$ErrorActionPreference = 'Continue'
$src = 'C:\Dev\UE5Source'
$log = 'C:\Dev\UE5Source-build.log'
function Step($name) { "[$(Get-Date -Format 'HH:mm:ss')] $name" | Tee-Object -FilePath $log -Append }

Step 'start'
if (-not (Test-Path "$src\Setup.bat")) {
	Step 'cloning 5.8.4-release (shallow)'
	git clone --depth 1 --branch 5.8.4-release https://github.com/EpicGames/UnrealEngine.git $src 2>&1 | Out-File $log -Append
}
if (-not (Test-Path "$src\Setup.bat")) { Step 'FAILED: clone'; exit 1 }

Set-Location $src
if (-not (Test-Path "$src\Engine\Binaries\DotNET\UnrealBuildTool")) {
	Step 'Setup.bat (downloads ~20 GB of dependencies)'
	cmd /c "Setup.bat --force" 2>&1 | Out-File $log -Append
}
Step 'GenerateProjectFiles'
cmd /c "GenerateProjectFiles.bat" 2>&1 | Out-File $log -Append

foreach ($target in 'ShaderCompileWorker', 'UnrealEditor') {
	Step "building $target"
	& "$src\Engine\Build\BatchFiles\Build.bat" $target Win64 Development -WaitMutex 2>&1 | Out-File $log -Append
	if ($LASTEXITCODE -ne 0) { Step "FAILED: $target (exit $LASTEXITCODE)"; exit 1 }
}
Step 'DONE: source engine built'
