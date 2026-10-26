# Packages the game for Windows with the launcher engine: one Wildreach.exe for everyone. Start it, pick a
# name and path, then Host (your PC is the server; friends need UDP 7777 forwarded to you) or Join by IP.
# A packaged dedicated server needs the source engine (Tools\build_engine.ps1); see the README.
#   package.ps1                    -> C:\Dev\WildreachBuild\Windows (Development: dev console stays on for playtests)
#   package.ps1 -Config Shipping
param([ValidateSet('Development', 'Shipping')] [string]$Config = 'Development', [string]$Out = 'C:\Dev\WildreachBuild')
$uat = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$log = Join-Path $Out 'package.log'
New-Item -ItemType Directory -Force $Out | Out-Null
Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue

"[$(Get-Date -Format 'HH:mm:ss')] BuildCookRun ($Config) -> $Out" | Tee-Object -FilePath $log
& $uat BuildCookRun "-project=$project" -noP4 -platform=Win64 "-clientconfig=$Config" -build -cook "-map=/Game/Maps/L_Island" `
	-stage -pak -archive "-archivedirectory=$Out" -utf8output -unattended 2>&1 | Tee-Object -FilePath $log -Append | Select-String -Pattern 'BUILD SUCCESSFUL|BUILD FAILED|ERROR|Error:' | Select-Object -Last 20
if ($LASTEXITCODE -ne 0) { "[$(Get-Date -Format 'HH:mm:ss')] FAILED (exit $LASTEXITCODE), see $log"; exit 1 }

# The game reads its numbers from Data\*.json next to the project folder (tuning, items, creatures, ...).
$game = Join-Path $Out 'Windows\Wildreach'
Copy-Item (Join-Path $root 'Data') $game -Recurse -Force
$version = ((Get-Content (Join-Path $root 'Config\DefaultGame.ini') | Where-Object { $_ -match '^ProjectVersion=' } | Select-Object -First 1) -replace '^ProjectVersion=', '').Trim()
@"
WILDREACH $version

1. Start Wildreach from its shortcut (or WildreachUpdater.exe): it installs a new version by itself
   when there is one (your saves are kept); everyone in a game needs the same version.
   Type a name, pick Male or Female, and Fire (volcanic) or Frost (tundra).
2. One person clicks Host. Everyone else types the host's IP and clicks Join.
   - On the same Wi-Fi: the host's local IP (run ipconfig, "IPv4 Address").
   - Over the internet: the host forwards UDP port 7777 on their router to their PC
     and shares their public IP (search "what is my ip").
3. Controls are listed at the top of the screen. A few to start with:
   LMB / R attack, RMB block, C dodge, E interact (and tame a knocked-out creature),
   B build and craft, I inventory, G swap to a bow or other ranged weapon (RMB aims).
   F10 returns to the menu.

Your character is saved on the host's PC under your name (Wildreach\Saved\ServerDB).
"@ | Set-Content -Encoding ascii (Join-Path $Out 'Windows\README.txt')
"[$(Get-Date -Format 'HH:mm:ss')] DONE: $Out\Windows\Wildreach.exe" | Tee-Object -FilePath $log -Append
