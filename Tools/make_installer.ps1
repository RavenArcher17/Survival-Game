# Turns the packaged game (Tools\package.ps1 -> C:\Dev\WildreachBuild\Windows) into one file friends can
# double-click: C:\Dev\WildreachBuild\Wildreach-Setup.exe. Uses the C# compiler that ships with Windows.
# The game folder gets version.txt (the game's version, ProjectVersion in Config\DefaultGame.ini) and
# WildreachUpdater.exe, which the shortcuts start: it installs newer releases from GitHub by itself. Publish a
# release tagged v<version> (e.g. v0.2.0) with Wildreach-Setup.exe attached and every installed copy picks it up.
#   make_installer.ps1 [-Build C:\Dev\WildreachBuild] [-Version 0.2.0] [-Out <setup.exe>]
param([string]$Build = 'C:\Dev\WildreachBuild', [string]$Version = '', [string]$Out = '')
$ErrorActionPreference = 'Stop'
$game = Join-Path $Build 'Windows'
$work = Join-Path $Build 'installer'
$csc = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$fx = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
if (-not (Test-Path (Join-Path $game 'Wildreach.exe'))) { throw "No packaged game in $game - run Tools\package.ps1 first" }
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
& $csc /nologo /target:winexe /optimize "/out:$(Join-Path $game 'WildreachUpdater.exe')" `
	"/r:$fx\System.Windows.Forms.dll" "/r:$fx\System.Drawing.dll" (Join-Path $PSScriptRoot 'installer\WildreachUpdater.cs')
if ($LASTEXITCODE -ne 0) { throw "csc (updater) failed ($LASTEXITCODE)" }

$zip = Join-Path $work 'game.zip'
Remove-Item $zip -ErrorAction SilentlyContinue
Write-Host "Zipping the game, version $Version (a few minutes)..."
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($game, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

if (-not $Out) { $Out = Join-Path $Build 'Wildreach-Setup.exe' }
Write-Host "Compiling $Out..."
& $csc /nologo /target:winexe /platform:x64 /optimize "/out:$Out" "/resource:$zip,game.zip" `
	"/r:$fx\System.IO.Compression.dll" "/r:$fx\System.IO.Compression.FileSystem.dll" "/r:$fx\System.Windows.Forms.dll" `
	"/r:$fx\System.Drawing.dll" "/r:$fx\Microsoft.CSharp.dll" (Join-Path $PSScriptRoot 'installer\WildreachSetup.cs')
if ($LASTEXITCODE -ne 0) { throw "csc failed ($LASTEXITCODE)" }
Remove-Item $zip
"{0} ({1:N0} MB), version {2}" -f $Out, ((Get-Item $Out).Length / 1MB), $Version
