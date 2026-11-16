# Turns the packaged game (Tools\package.ps1 -> C:\Dev\WildreachBuild\Windows) into one file friends can
# double-click: C:\Dev\WildreachBuild\Wildreach-Setup.exe. Uses the C# compiler that ships with Windows.
# The game folder gets version.txt (the game's version, ProjectVersion in Config\DefaultGame.ini) and
# WildreachUpdater.exe, which the shortcuts start: it installs newer releases from GitHub by itself. Publish a
# release tagged v<version> (e.g. v0.2.0) with Wildreach-Setup.exe attached and every installed copy picks it up,
# fetching only the parts of that Setup it doesn't already have (installer\WildreachPack.cs).
# What to attach to the release is in <Build>\release: the Setup itself when it's under GitHub's 2 GiB file limit
# (-PartMB), else the Setup cut into Wildreach-Setup.001, .002... with their list Wildreach-Setup.parts and, as
# Wildreach-Setup.exe, the small program that downloads them and runs the whole Setup (installer\WildreachBootstrap.cs;
# older updaters run it, newer ones read the parts themselves). The whole Setup stays at -Out for testing on this PC.
#   make_installer.ps1 [-Build C:\Dev\WildreachBuild] [-Version 0.2.0] [-Out <setup.exe>] [-Engine <UE_5.8 folder>]
#                      [-Release <folder>] [-PartMB 1900]
param([string]$Build = 'C:\Dev\WildreachBuild', [string]$Version = '', [string]$Out = '', [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
	[string]$Release = '', [int]$PartMB = 1900)
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
Copy-Item (Join-Path $Engine 'Engine\Extras\Redist\en-us\vc_redist.x64.exe') (Join-Path $game 'Redist') -Force

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

# ---- what goes on the release
if (-not $Release) { $Release = Join-Path $Build 'release' }
if ($PartMB -lt 1 -or $PartMB -ge 2048) { throw "-PartMB must be under 2048 (GitHub takes nothing over 2 GiB a file)" }
if (Test-Path $Release) { Remove-Item -Recurse -Force $Release }
New-Item -ItemType Directory -Force $Release | Out-Null
$size = (Get-Item $Out).Length
$partSize = [long]$PartMB * 1MB
if ($size -le $partSize) {
	Copy-Item $Out (Join-Path $Release 'Wildreach-Setup.exe')
	"release: Wildreach-Setup.exe in one file -> $Release"
	return
}
function Get-Sha([string]$path) { (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() }
$lines = @('WRPARTS1', "version $Version", "size $size", "sha256 $(Get-Sha $Out)")
$whole = [System.IO.File]::OpenRead($Out)
try {
	$buffer = New-Object byte[] (16MB)
	for ($n = 1; $whole.Position -lt $size; $n++) {
		$name = 'Wildreach-Setup.{0:000}' -f $n
		$path = Join-Path $Release $name
		$output = [System.IO.File]::Create($path)
		try {
			$left = [Math]::Min($partSize, $size - $whole.Position)
			while ($left -gt 0) {
				$read = $whole.Read($buffer, 0, [int][Math]::Min($buffer.Length, $left))
				if ($read -le 0) { throw 'the Setup ended early' }
				$output.Write($buffer, 0, $read); $left -= $read
			}
		} finally { $output.Dispose() }
		$lines += "part $name $((Get-Item $path).Length) $(Get-Sha $path)"
	}
} finally { $whole.Dispose() }
[System.IO.File]::WriteAllText((Join-Path $Release 'Wildreach-Setup.parts'), (($lines -join "`n") + "`n"))
# The download program, with the release it belongs to built in.
New-Item -ItemType Directory -Force $work | Out-Null
$bootInfo = Join-Path $work 'BootInfo.cs'
[System.IO.File]::WriteAllText($bootInfo, "static class BootInfo { public const string Tag = `"v$Version`"; }")
& $csc /nologo /target:winexe /platform:x64 /optimize "/out:$(Join-Path $Release 'Wildreach-Setup.exe')" "/r:$fx\System.Windows.Forms.dll" "/r:$fx\System.Drawing.dll" `
	(Join-Path $PSScriptRoot 'installer\WildreachBootstrap.cs') $pack $bootInfo
if ($LASTEXITCODE -ne 0) { throw "csc (download program) failed ($LASTEXITCODE)" }
Remove-Item $bootInfo
"release: Wildreach-Setup.exe (the download program) + {0} parts + Wildreach-Setup.parts -> {1}" -f ($lines.Count - 4), $Release
