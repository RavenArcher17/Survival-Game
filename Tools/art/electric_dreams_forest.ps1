# Copies the Electric Dreams forest kit into Content/ (cooks/wild-answers-1116.md 4.1).
#
# The Ancient forest draws Epic's Electric Dreams hornbeams, ferns, ground cover, dead leaves, moss, elder, mossy
# boulders and twigs (Data/forest_art.json). They are a Fab sample, ~35 GB whole, so like the six Fab packs they are not
# in git: this copies only the packages the forest uses (Tools/art/electric_dreams_forest.json, ~1.9 GB) from an
# Electric Dreams project folder, with their paths unchanged (/Game/Megascans, /Game/MSPresets, /Game/Custom,
# /Game/PhysicalMaterials). Then run the commandlet once (it caps the textures at 2K and writes Data/forest_art.json):
#   UnrealEditor-Cmd.exe Wildreach.uproject -run=pythonscript -script="Tools/art/electric_dreams_forest.py" -unattended -nop4 -nosplash
#
# Get the project: Fab, "Electric Dreams Environment" (free, Epic), Add to My Library, then in the Epic launcher's
# Unreal Engine > Library > Fab Library, Create Project (5.8). Then:
#   powershell -ExecutionPolicy Bypass -File Tools\art\electric_dreams_forest.ps1 -From "<that project folder>"
param(
	[string]$From = "$env:USERPROFILE\OneDrive\Documents\Unreal Projects\ElectricDreamsEnv",
	# Copy over files already there (the commandlet re-saves the textures at 2K: without -Force they're kept).
	[switch]$Force
)
$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Src = Join-Path $From "Content"
if (-not (Test-Path (Join-Path $Src "Megascans"))) { throw "No Electric Dreams content at $Src (pass -From <the project folder>)" }
$List = (Get-Content (Join-Path $PSScriptRoot "electric_dreams_forest.json") -Raw | ConvertFrom-Json).packages
$Copied = 0; $Bytes = 0; $Missing = @()
foreach ($Pkg in $List) {
	$Rel = $Pkg.Substring(6).Replace('/', '\')          # strip "/Game/"
	$Found = $false
	foreach ($Ext in ".uasset", ".ubulk", ".uexp", ".uptnl") {
		$S = Join-Path $Src ($Rel + $Ext)
		if (-not (Test-Path $S)) { continue }
		$D = Join-Path (Join-Path $Repo "Content") ($Rel + $Ext)
		New-Item -ItemType Directory -Force (Split-Path -Parent $D) | Out-Null
		$Si = Get-Item $S
		if ($Force -or -not (Test-Path $D)) { Copy-Item $S $D -Force }
		$Copied++; $Bytes += $Si.Length; $Found = $true
	}
	if (-not $Found) { $Missing += $Pkg }
}
"Copied $Copied files ({0:N0} MB) for $($List.Count) packages." -f ($Bytes / 1MB)
if ($Missing.Count) { "Missing in the source ($($Missing.Count)):"; $Missing | ForEach-Object { "  $_" } }
