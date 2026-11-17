# Packages the game for Windows with the launcher engine: one Wildreach.exe for everyone. Start it, pick a
# name and path, then Host (your PC is the server; friends need UDP 7777 forwarded to you) or Join by IP.
# A packaged dedicated server needs the source engine (Tools\build_engine.ps1); see the README.
#   package.ps1                      -> C:\Dev\WildreachBuild\Windows (Shipping: what players get, no console, no log)
#   package.ps1 -Config Development  the testers' build: the dev console and the log (Saved\Logs) stay on
#   package.ps1 -Store freedom35     the Freedom35 download: no WildreachUpdater.exe (Freedom35's own updater, F35Update,
#                                    is added by the site's `f35 installer`), a README that says updates come through it
#   package.ps1 -SkipCook            stage again from the last cook (e.g. a Development copy of the same content)
#   package.ps1 -Engine <folder>     the UE_5.8 folder, when the engine isn't in the launcher's default place
#   package.ps1 -SkipEditorBuild     the editor is already built: cook with it as it is. (Another project's editor or -game run
#                                    on this engine has a Live Coding session, and UnrealBuildTool won't build the editor then.)
# Debug symbols (.pdb) never ship: they are moved to <Out>\Symbols\<version>-<config> for reading crash dumps.
param([ValidateSet('Development', 'Shipping')] [string]$Config = 'Shipping', [string]$Out = 'C:\Dev\WildreachBuild', [switch]$SkipEditorBuild,
	[switch]$SkipCook, [ValidateSet('github', 'freedom35')] [string]$Store = 'github', [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')
$uat = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$log = Join-Path $Out 'package.log'
New-Item -ItemType Directory -Force $Out | Out-Null
Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue

"[$(Get-Date -Format 'HH:mm:ss')] BuildCookRun ($Config) -> $Out" | Tee-Object -FilePath $log
$extra = @(if ($SkipEditorBuild) { '-nocompileeditor' }) + @(if ($SkipCook) { '-skipcook' } else { '-cook' })
& $uat BuildCookRun "-project=$project" -noP4 -platform=Win64 "-clientconfig=$Config" -build "-map=/Game/Maps/L_Island" @extra `
	-stage -pak -archive "-archivedirectory=$Out" -utf8output -unattended 2>&1 | Tee-Object -FilePath $log -Append | Select-String -Pattern 'BUILD SUCCESSFUL|BUILD FAILED|ERROR|Error:' | Select-Object -Last 20
if ($LASTEXITCODE -ne 0) { "[$(Get-Date -Format 'HH:mm:ss')] FAILED (exit $LASTEXITCODE), see $log"; exit 1 }

# The game reads its numbers from Data\*.json next to the project folder (tuning, items, creatures, ...).
$game = Join-Path $Out 'Windows\Wildreach'
Copy-Item (Join-Path $root 'Data') $game -Recurse -Force
# Third-party credits ship with the game (some sounds are CC-BY, which asks for attribution where the work is used).
Copy-Item (Join-Path $root 'Docs\CREDITS.md') (Join-Path $Out 'Windows\CREDITS.md') -Force
$version = ((Get-Content (Join-Path $root 'Config\DefaultGame.ini') | Where-Object { $_ -match '^ProjectVersion=' } | Select-Object -First 1) -replace '^ProjectVersion=', '').Trim()
# Debug symbols never ship (0.8.2: the 0.8.1 package carried a 499 MB Wildreach.pdb to every player). They are kept
# beside the build, per version and config, to read a crash dump's call stack.
$symbols = Join-Path $Out "Symbols\$version-$Config"
New-Item -ItemType Directory -Force $symbols | Out-Null
Get-ChildItem (Join-Path $Out 'Windows') -Recurse -Filter *.pdb | ForEach-Object {
	"[$(Get-Date -Format 'HH:mm:ss')] symbols kept, not shipped: $($_.Name) ($([math]::Round($_.Length / 1MB)) MB) -> $symbols" | Tee-Object -FilePath $log -Append
	Move-Item -Force $_.FullName (Join-Path $symbols $_.Name)
}
[System.IO.File]::WriteAllText((Join-Path $Out 'Windows\version.txt'), $version)
if ($Store -eq 'github') {
	# A copy from the GitHub release carries its updater (0.8.1: a build run from here, not installed, had none, so the
	# main menu's Update now had nothing to start). make_installer.ps1 builds the same two again for the Setup.
	$csc = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
	$fx = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
	& $csc /nologo /target:winexe /optimize /platform:x64 "/out:$(Join-Path $Out 'Windows\WildreachUpdater.exe')" `
		"/r:$fx\System.Windows.Forms.dll" "/r:$fx\System.Drawing.dll" (Join-Path $PSScriptRoot 'installer\WildreachUpdater.cs') (Join-Path $PSScriptRoot 'installer\WildreachPack.cs')
	if ($LASTEXITCODE -ne 0) { "[$(Get-Date -Format 'HH:mm:ss')] FAILED: csc (updater) exit $LASTEXITCODE"; exit 1 }
} else {
	# A Freedom35 download: Freedom35's own updater (F35Update.exe, added by the site's `f35 installer`) keeps it current.
	Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $Out 'Windows\WildreachUpdater.exe')
}
# build.json beside the game: what this package is (F8 reports carry it, WRReport.cpp) and what's in it, counted from the data.
$commit = (& git -C $root rev-parse HEAD 2>$null)
$dirty = [bool](& git -C $root status --porcelain --untracked-files=no 2>$null)
# What a player downloads: every file, the total and the biggest (Freedom35's file store takes up to 64 MB a file; the
# cooked content is cut into 60 MB IoStore partitions, Config\DefaultGame.ini MaxIoStorePartitionSizeMB).
$files = @(Get-ChildItem (Join-Path $Out 'Windows') -Recurse -File)
$sizes = [ordered]@{
	files = $files.Count; bytes = [int64]($files | Measure-Object Length -Sum).Sum
	biggest = @($files | Sort-Object Length -Descending | Select-Object -First 5 | ForEach-Object {
		[ordered]@{ path = $_.FullName.Substring((Join-Path $Out 'Windows').Length + 1); bytes = $_.Length } })
}
function Count-Rows($file, $key) { try { return @((Get-Content -Raw -Encoding utf8 (Join-Path $root "Data\$file") | ConvertFrom-Json).$key).Count } catch { return 0 } }
$lands = @('fire (L_Island)', 'frost (L_Island)', 'lightning (storm_peaks)', 'swamp') + @(Get-ChildItem (Join-Path $root 'Data\lands') -Filter *.json | ForEach-Object { $_.BaseName })
$info = [ordered]@{
	game = 'Wildreach'; version = $version; config = $Config; commit = "$commit"; uncommittedChanges = $dirty
	builtAt = (Get-Date).ToString('yyyy-MM-ddTHH:mm:sszzz'); map = '/Game/Maps/L_Island'; store = $Store
	sizes = $sizes
	features = [ordered]@{
		lands = $lands
		paths = @(Get-ChildItem (Join-Path $root 'Data\paths') -Filter *.json | ForEach-Object { $_.BaseName })
		creatures = (Count-Rows 'creatures.json' 'creatures'); items = (Count-Rows 'items.json' 'items')
		recipes = (Count-Rows 'recipes.json' 'recipes'); buildPieces = (Count-Rows 'pieces.json' 'pieces')
	}
}
$info | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $Out 'Windows\build.json')
$consoleHelp = @'
CONSOLE (testers)
The ` key (left of 1, under Esc) opens the console; type a command and press Enter, Tab completes.
These test commands work only for the studio's testers (signed in with their Freedom35 account);
the game refuses them for everyone else. A few handy ones:
   WRGive <item> <count>        e.g. WRGive wood 100, WRGive copper_ingot 10 (ids: Wildreach\Data\items.json)
   WRTeleport <x> <y>           centimetres, e.g. WRTeleport -131200 -51600 (Volcanic camp 3)
   WRHour <0-24>                set the time of day, e.g. WRHour 22 for night
   WRSpawn <creature> <count>   spawn creatures near you, e.g. WRSpawn ashstrider 1
                                (ids: Wildreach\Data\creatures.json)
   WRHeal                       full health, stamina, food and water
   WRGivePoints <path> <n>      points for a path tree (fire or frost), e.g. WRGivePoints fire 5
   WRClearCreatures             remove the wild creatures within 30 m of you
'@
$updateLine = if ($Store -eq 'freedom35') {
	"1. Start Wildreach from its shortcut: it brings itself up to the newest version through Freedom35`r`n   before it opens (your saves are kept); everyone in a game needs the same version."
} else {
	"1. Start Wildreach from its shortcut (or WildreachUpdater.exe): it installs a new version by itself`r`n   when there is one (your saves are kept); everyone in a game needs the same version."
}
$savesAt = if ($Config -eq 'Shipping') { '%LOCALAPPDATA%\Wildreach\Saved\Worlds' } else { 'Wildreach\Saved\Worlds, in the game folder' }
$testers = if ($Config -eq 'Shipping') {
	"TESTERS`r`nThe studio's testers (signed in with their Freedom35 account) type god-mode in the chat line: the God mode`r`nwindow then has every tester tool. The game refuses them for everyone else."
} else { $consoleHelp }
@"
WILDREACH $version

$updateLine
   Type a name, pick your look and one of the twelve paths (each wakes you in its own land).
2. One person clicks Host. Everyone else types the host's IP and clicks Join.
   - On the same Wi-Fi: the host's local IP (run ipconfig, "IPv4 Address").
   - Over the internet: the host forwards UDP port 7777 on their router to their PC
     and shares their public IP (search "what is my ip").
3. Every key can be changed in Esc > Controls, and H shows them in game. To start with:
   LMB / R attack, RMB block, C dodge, E interact (hold E on your tame for its orders),
   1-8 your items, hold Q for your abilities, I J P K M the journal (inventory, crafting,
   paths, skills, map), B build, G a bow or other ranged weapon (RMB aims), Esc the menu,
   F10 back to the main menu.
   In build mode a piece faces the way you look. R turns it (hold to spin), the wheel turns it
   finely, and F changes the snap: Grid, Fine (smaller steps on a foundation), Off (anywhere).
   X (or the Demolish button on the bar) takes down pieces you built: aim at one, it turns red
   and shows what you get back (half its cost), click. X or Esc stops.
   Tab locks on to what's nearest the crosshair: your spells lead it and curve toward it.

$testers

Each world is a folder on the host's PC ($savesAt), with a character for you in each one, under your name.
Sound, music and other outside files are credited in CREDITS.md.
"@ | Set-Content -Encoding ascii (Join-Path $Out 'Windows\README.txt')
"[$(Get-Date -Format 'HH:mm:ss')] DONE: $Out\Windows\Wildreach.exe" | Tee-Object -FilePath $log -Append
