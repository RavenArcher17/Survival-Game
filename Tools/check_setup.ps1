# Checks that this PC has what Wildreach needs to build, run, test and release, and says how to fix what's missing.
# Run it first on a new PC, and whenever a build or the editor fails in a way that looks like setup:
#   powershell -ExecutionPolicy Bypass -File Tools\check_setup.ps1
# Exit code 0: ready. 1: at least one FAIL (each says what to do). NOTE lines are optional extras.
$root = Split-Path $PSScriptRoot -Parent
$script:fails = 0

function Check([string]$what, [bool]$ok, [string]$fix) {
	if ($ok) { Write-Host "PASS  $what" }
	else { Write-Host "FAIL  $what" -ForegroundColor Red; Write-Host "      $fix"; $script:fails++ }
}
function Note([string]$what) { Write-Host "NOTE  $what" -ForegroundColor Yellow }

# Unreal Engine: the scripts in Tools\ look for the launcher install in its default folder.
$ue = 'C:\Program Files\Epic Games\UE_5.8'
$ver = ''
if (Test-Path "$ue\Engine\Build\Build.version") {
	$v = Get-Content "$ue\Engine\Build\Build.version" -Raw | ConvertFrom-Json
	$ver = "$($v.MajorVersion).$($v.MinorVersion).$($v.PatchVersion)"
}
Check "Unreal Engine 5.8 in $ue (found: $(if ($ver) { $ver } else { 'none' }))" ($ver -like '5.8.*') `
	'Install Unreal Engine 5.8 from the Epic Games Launcher (Unreal Engine > Library > +), in its default folder.'
if ($ver -like '5.8.*' -and $ver -ne '5.8.4') { Note "Raven builds with 5.8.4; $ver should work, but update the engine in the launcher if anything odd happens." }

# Visual Studio with the C++ game workload (UE 5.8: VS 2022 17.8 or later, or VS 2026).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = ''
if (Test-Path $vswhere) {
	$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Workload.NativeGame Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName
}
Check "Visual Studio with 'Game development with C++' (found: $(if ($vs) { $vs } else { 'none' }))" ([bool]$vs) `
	"Install Visual Studio 2022 Community (free) and tick the workloads 'Game development with C++', 'Desktop development with C++' and '.NET desktop development'."

# Windows SDK (Unreal wants 10.0.19041 or later).
$sdks = @(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^10\.0\.(\d+)\.' -and [int]$Matches[1] -ge 19041 } | ForEach-Object { $_.Name })
Check "Windows SDK 10.0.19041 or later (found: $(if ($sdks.Count) { $sdks -join ', ' } else { 'none' }))" ($sdks.Count -gt 0) `
	'In the Visual Studio Installer, Modify > Individual components > tick a Windows 11 SDK (10.0.22621 is the one Unreal 5.8 prefers).'

# Git and Git LFS, and the art actually downloaded (not LFS placeholder files).
$git = (& git --version 2>$null)
Check "Git ($git)" ([bool]$git) 'Install Git for Windows from https://git-scm.com (it includes Git LFS).'
$lfs = (& git lfs version 2>$null)
Check "Git LFS ($lfs)" ([bool]$lfs) 'Git LFS comes with Git for Windows; run: git lfs install'
if ($lfs) {
	Push-Location $root
	$list = @(& git lfs ls-files 2>$null)
	Pop-Location
	$missing = @($list | Where-Object { $_ -match '^\S+ - ' })
	Check "the repo's art is downloaded ($($list.Count - $missing.Count) of $($list.Count) LFS files)" ($list.Count -gt 0 -and $missing.Count -eq 0) `
		'In the repo folder run: git lfs install, then git lfs pull'
}

# The Fab packs the game uses. They're not in git (their licence doesn't allow sharing them publicly).
$packs = [ordered]@{
	'GV_FreeShrubsPack'       = '"Free Shrubs Pack (Ultra Realistic Wind)" by Greenleaf Vision'
	'LavaMat'                 = '"Lava Material" by Stone Material (the one that makes a LavaMat folder)'
	'QuadrapedCreatures'      = '"Quadruped Fantasy Creatures" by PROTOFACTOR INC'
	'SimpleRocks'             = '"Simple Rocks" by Orangujanusz Store'
	'WaterMaterials'          = '"Water Materials" by tharlevfx'
	'MWLandscapeAutoMaterial' = '"MW Landscape Auto Material" by MAWI United'
}
foreach ($p in $packs.Keys) {
	$d = Join-Path $root "Content\$p"
	$n = @(Get-ChildItem $d -Recurse -Filter *.uasset -ErrorAction SilentlyContinue).Count
	Check "art pack Content\$p ($n assets)" ($n -gt 0) `
		"Add $($packs[$p]) from Fab to this project so it lands in Content\$p (exact name), or copy that folder from Raven."
}

# Packaging crashes cooking these four Barghest pack files (no skeleton); the game never uses them.
$bad = @('SK_BARGHEST_MANE', 'SK_BARGHEST_TAIL', 'BARGHEST_MANE_Anim', 'BARGHEST_TAIL_Anim') | ForEach-Object {
	Get-ChildItem (Join-Path $root 'Content\QuadrapedCreatures') -Recurse -Filter "$_.uasset" -ErrorAction SilentlyContinue
}
Check 'the Barghest mane and tail files are out of Content (packaging crashes on them)' ($bad.Count -eq 0) `
	("Move these out of the project (anywhere outside Content), with the editor closed: " + (($bad | ForEach-Object { $_.FullName.Substring($root.Length + 1) }) -join ', '))

# A pack added from Fab as a "complete project" can overwrite the project's config.
Push-Location $root
$cfg = @(& git status --porcelain -- Config Wildreach.uproject 2>$null | Where-Object { $_ -notmatch 'DefaultEditor\.ini$' })
$roots = @(Get-ChildItem $root -Filter *.uproject | Where-Object { $_.Name -ne 'Wildreach.uproject' })
Pop-Location
Check 'Config\ and Wildreach.uproject match git' ($cfg.Count -eq 0) `
	("A pack may have overwritten them. Look at 'git diff -- Config' and restore with 'git checkout -- <file>' unless you changed it on purpose: " + ($cfg -join '; '))
Check 'no other .uproject in the repo folder' ($roots.Count -eq 0) ("A pack dropped these; delete them: " + (($roots | ForEach-Object { $_.Name }) -join ', '))

# Optional tools.
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { Note 'GitHub CLI (gh) not found: needed only to publish a release (https://cli.github.com, then gh auth login).' }
if (-not (Get-Command node -ErrorAction SilentlyContinue)) { Note 'Node.js not found: needed only for the island scripts in Tools\terrain (dressing.mjs, spawns.mjs).' }
if ($root -match 'OneDrive') { Note "The repo is inside OneDrive ($root): move it out (for example C:\Dev\Wildreach); syncing fights with Unreal's files." }
$drive = Get-PSDrive ($root.Substring(0, 1)) -ErrorAction SilentlyContinue
if ($drive -and $drive.Free -lt 60GB) { Note ("{0:N0} GB free on {1}: building, the shader cache and packaged builds want about 60 GB more." -f ($drive.Free / 1GB), $drive.Name) }

Write-Host ''
if ($script:fails) { Write-Host "$($script:fails) to fix before building." -ForegroundColor Red; exit 1 }
Write-Host 'Ready. Next: the "Build" step in README.md (section "Working on the game on a new PC").' -ForegroundColor Green
exit 0
