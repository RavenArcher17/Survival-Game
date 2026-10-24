# Turns the packaged game (Tools\package.ps1 -> C:\Dev\WildreachBuild\Windows) into one file friends can
# double-click: C:\Dev\WildreachBuild\Wildreach-Setup.exe. Uses the C# compiler that ships with Windows.
param([string]$Build = 'C:\Dev\WildreachBuild')
$ErrorActionPreference = 'Stop'
$game = Join-Path $Build 'Windows'
$work = Join-Path $Build 'installer'
$csc = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$fx = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
if (-not (Test-Path (Join-Path $game 'Wildreach.exe'))) { throw "No packaged game in $game - run Tools\package.ps1 first" }

# Microsoft's VC++ runtime installer (ships with UE) so Setup can install it on PCs that lack it.
New-Item -ItemType Directory -Force (Join-Path $game 'Redist'), $work | Out-Null
Copy-Item 'C:\Program Files\Epic Games\UE_5.8\Engine\Extras\Redist\en-us\vc_redist.x64.exe' (Join-Path $game 'Redist') -Force

$zip = Join-Path $work 'game.zip'
Remove-Item $zip -ErrorAction SilentlyContinue
Write-Host "Zipping the game (a few minutes)..."
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($game, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

$out = Join-Path $Build 'Wildreach-Setup.exe'
Write-Host "Compiling $out..."
& $csc /nologo /target:winexe /platform:x64 /optimize "/out:$out" "/resource:$zip,game.zip" `
	"/r:$fx\System.IO.Compression.dll" "/r:$fx\System.IO.Compression.FileSystem.dll" "/r:$fx\System.Windows.Forms.dll" `
	"/r:$fx\System.Drawing.dll" "/r:$fx\Microsoft.CSharp.dll" (Join-Path $PSScriptRoot 'installer\WildreachSetup.cs')
if ($LASTEXITCODE -ne 0) { throw "csc failed ($LASTEXITCODE)" }
Remove-Item $zip
"{0} ({1:N0} MB)" -f $out, ((Get-Item $out).Length / 1MB)
