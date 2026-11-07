# Turns the packaged game (Tools\package.ps1 -> C:\Dev\WildreachBuild\Windows) into one file friends can
# double-click: C:\Dev\WildreachBuild\Wildreach-Setup.exe. Uses the C# compiler that ships with Windows.
# The game folder gets version.txt (the game's version, ProjectVersion in Config\DefaultGame.ini) and
# WildreachUpdater.exe, which the shortcuts start: it installs newer releases from GitHub by itself. Publish a
# release tagged v<version> (e.g. v0.2.0) with Wildreach-Setup.exe attached and every installed copy picks it up,
# fetching only the parts of that Setup it doesn't already have (installer\WildreachPack.cs).
#   make_installer.ps1 [-Build C:\Dev\WildreachBuild] [-Version 0.2.0] [-Out <setup.exe>]
param([string]$Build = 'C:\Dev\WildreachBuild', [string]$Version = '', [string]$Out = '')
$ErrorActionPreference = 'Stop'
$game = Join-Path $Build 'Windows'
$work = Join-Path $Build 'installer'
$csc = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$fx = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
if (-not (Test-Path (Join-Path $game 'Wildreach.exe'))) { throw "No packaged game in $game - run Tools\package.ps1 first" }
# Everything in the folder goes in the installer: a build that's been run has saves, logs and settings in Wildreach\Saved,
# which would land on every player's PC. Make the installer before running the build, or run a copy of it.
if (Test-Path (Join-Path $game 'Wildreach\Saved')) { throw "$game\Wildreach\Saved exists (this build has been run): package again, or remove it first" }
if (-not $Version) {
	$line = Get-Content (Join-Path $PSScriptRoot '..\Config\DefaultGame.ini') | Where-Object { $_ -match '^ProjectVersion=' } | Select-Object -First 1
	if (-not $line) { throw 'No ProjectVersion in Config\DefaultGame.ini' }
	$Version = ($line -replace '^ProjectVersion=', '').Trim()
}

# Microsoft's VC++ runtime installer (ships with UE) so Setup can install it on PCs that lack it.
New-Item -ItemType Directory -Force (Join-Path $game 'Redist'), $work | Out-Null
Copy-Item 'C:\Program Files\Epic Games\UE_5.8\Engine\Extras\Redist\en-us\vc_redist.x64.exe' (Join-Path $game 'Redist') -Force

# The updater and the version it compares against (no BOM: it's read as plain text).
[System.IO.File]::WriteAllText((Join-Path $game 'version.txt'), $Version)
Remove-Item (Join-Path $game 'updater.log') -ErrorAction SilentlyContinue
$pack = Join-Path $PSScriptRoot 'installer\WildreachPack.cs'
& $csc /nologo /target:winexe /optimize /platform:x64 "/out:$(Join-Path $game 'WildreachUpdater.exe')" `
	"/r:$fx\System.Windows.Forms.dll" "/r:$fx\System.Drawing.dll" (Join-Path $PSScriptRoot 'installer\WildreachUpdater.cs') $pack
if ($LASTEXITCODE -ne 0) { throw "csc (updater) failed ($LASTEXITCODE)" }

# The installer program, and the tool that puts the game on the end of it.
$stub = Join-Path $work 'stub.exe'
$tool = Join-Path $work 'WildreachPackTool.exe'
& $csc /nologo /target:winexe /platform:x64 /optimize "/out:$stub" "/r:$fx\System.Windows.Forms.dll" "/r:$fx\System.Drawing.dll" `
	"/r:$fx\Microsoft.CSharp.dll" (Join-Path $PSScriptRoot 'installer\WildreachSetup.cs') $pack
if ($LASTEXITCODE -ne 0) { throw "csc (installer) failed ($LASTEXITCODE)" }
& $csc /nologo /target:exe /platform:x64 /optimize "/out:$tool" (Join-Path $PSScriptRoot 'installer\WildreachPackTool.cs') $pack
if ($LASTEXITCODE -ne 0) { throw "csc (pack tool) failed ($LASTEXITCODE)" }

if (-not $Out) { $Out = Join-Path $Build 'Wildreach-Setup.exe' }
Write-Host "Packing the game, version $Version (a few minutes)..."
& $tool $game $stub $Out $Version
if ($LASTEXITCODE -ne 0) { throw "the pack tool failed ($LASTEXITCODE)" }
Remove-Item $stub, $tool
"{0} ({1:N0} MB), version {2}" -f $Out, ((Get-Item $Out).Length / 1MB), $Version
