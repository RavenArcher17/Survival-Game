# Screenshots of the world's look (polish pass): the sea from the beach and a cliff, both biomes at noon and dusk,
# a base of every build piece, the stations, the Hearth and the claim totem up close, the camera against a wall,
# each by day and at night, plus "stat unit" frame times in three fixed spots. A rendered client on L_Island driven
# by bot commands (as uishots.ps1); one picture per shot into -Out, numbered and named.
#   worldshots.ps1 [-Out <folder>] [-Only coast,base] [-Port 7865] [-Baseline]
# Sections: base (builds the showcase base; later sections use it), perf, coast, biomes, stations, camera, foliage,
# popin (distant decoration at 1x and 3x draw distance), and checks (A/B: mist off, sea waves / highlights off; only
# when asked for with -Only checks).
# -Baseline leaves out what needs this pass's own commands (the free camera wr.Cam, wr.FrameStats), so the same
# script also shoots the unchanged game for before / after.
param([string]$Out = '', [string[]]$Only = @(), [int]$Port = 7865, [switch]$Baseline)
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-worldshots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$name = 'WorldShots'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date
if ($Only.Count -gt 0) { $Only = $Only | ForEach-Object { $_ -split ',' } }

# ---------------------------------------------------------------- helpers
$labels = New-Object System.Collections.ArrayList
$cmds = New-Object System.Collections.ArrayList
$section = ''
$emit = $true
# -Only runs just those sections (plus the base build when a section stands around the base).
$needBase = $Only.Count -eq 0 -or @($Only | Where-Object { $_ -in @('base', 'stations', 'camera', 'perf') }).Count -gt 0
function Wanted { $Only.Count -eq 0 -or $Only -contains $script:section }
function Begin([string]$name) { $script:section = $name; $script:emit = (Wanted) -or ($name -eq 'base' -and $needBase) }
function Add([string[]]$c) { if (-not $script:emit) { return }; foreach ($x in $c) { if ($x) { [void]$cmds.Add($x) } } }
# A scene picture (no UI); a UI picture (stat unit shows only in these).
function Shot([string]$label) { if (Wanted) { Add 'wait 0.6', 'HighResShot 1600x900', 'wait 1.5'; [void]$labels.Add("$($script:section)_$label") } }
function UIShot([string]$label) { if (Wanted) { Add 'wait 0.6', 'shot showui', 'wait 1.5'; [void]$labels.Add("$($script:section)_$label") } }
# Teleport (dropped onto whatever is below: outdoors only) and look; far jumps wait for the world to stream in.
function Look([string]$at, [string]$look, [double]$wait = 5) { Add "WRTeleport $at", "wait $wait", "WRLook $look", 'wait 1' }
# Free camera at a point (this pass's wr.Cam): X Y Z yaw pitch; "wr.Cam off" returns to the survivor.
function Cam([string]$where) { if (-not $Baseline) { Add "wr.Cam $where", 'wait 1.5' } }
function Hour([double]$h) { Add "WRHour $h", 'wait 3' }
function P([string]$piece, [double]$x, [double]$y, [double]$z, [double]$yaw) { Add "WRPlace $piece $x $y $z $yaw" }
# Through the build ghost, as a player places it (first person): stand, look, pick, let it snap, place.
function Ghost([string]$piece, [string]$at, [string]$look) { Add "WRTeleport $at", 'wait 2', "WRLook $look", "WRGhost $piece", 'wait 1', 'WRGhostPlace', 'wait 0.5' }

# ---------------------------------------------------------------- the showcase base
# The house test's flat spot south of the middle volcanic spawn (ground ~4797; foundations sit 60 cm in), just outside
# the spawn's safe zone. A two-storey house of cells A (X, Y), B east, C north, D north-east: wood west half, stone east
# half, a stair hole in D; hut H south of B (stone roof and upper wall) and hut I south of A (wood roof and upper wall);
# E east of D holds the stone stairs. A yard east of the house has the four stations, the Hearth and the claim totem;
# the palisade, gate, guard tower, crop plot and pen stand around it. Every piece type appears once or more.
$X = -132000; $Y = -56400; $Zf = 4734; $T = $Zf + 100; $S1 = $T + 300; $R = $T + 600
# The small things first: a bag stuffed with wood and stone takes nothing more (the base costs ~900 wood, ~700 stone).
Add 'WRBotPause 1', 'wait 3', 'WRClearCreatures'
Begin 'base'
Add 'WRGive copper_ingot 12', 'WRGive copper_ore 8', 'WRGive hide 10', 'WRGive fiber 40', 'WRGive core_vulkarn 1',
	'WRGive wood 1200', 'WRGive stone 900', 'WRView fp'
Add "WRTeleport $($X - 500) $Y", 'wait 5'
P wood_foundation $X $Y $Zf 0
Ghost wood_foundation "$($X - 500) $($Y + 400)" '0 -20'      # C
Ghost stone_foundation "$($X + 900) $Y" '180 -20'            # B
Ghost stone_foundation "$($X + 900) $($Y + 400)" '180 -20'   # D
Ghost stone_foundation "$($X + 1300) $($Y + 400)" '180 -20'  # E
Ghost stone_foundation "$($X + 400) $($Y - 900)" '90 -20'    # H
Ghost wood_foundation "$X $($Y - 900)" '90 -20'              # I
Add "WRTeleport $($X + 100) $($Y + 200)", 'wait 2'
# ground floor
P wood_wall $X ($Y - 200) $T 0; P wood_window ($X - 200) $Y $T 90; P wood_pillar ($X - 200) ($Y + 200) $T 0
P stone_window ($X + 400) ($Y - 200) $T 0; P reinforced_door ($X + 600) $Y $T 90
P wood_wall $X ($Y + 600) $T 0; P wood_wall ($X - 200) ($Y + 400) $T 90
P stone_wall ($X + 400) ($Y + 600) $T 0; P stone_wall ($X + 600) ($Y + 400) $T 90
P wood_doorway ($X + 200) $Y $T 90; P wood_half_wall ($X + 200) ($Y + 400) $T 90
# first floor: floors over A, B, C (D stays open for the stairs rising south onto B), walls stacked on the ones below
P wood_floor $X $Y $S1 0; P stone_floor ($X + 400) $Y $S1 0; P wood_floor $X ($Y + 400) $S1 0
P wood_stairs ($X + 400) ($Y + 400) $T 270
P wood_wall $X ($Y - 200) $S1 0; P wood_window ($X - 200) $Y $S1 90; P wood_wall ($X - 200) ($Y + 400) $S1 90; P wood_wall $X ($Y + 600) $S1 0
P stone_wall ($X + 400) ($Y - 200) $S1 0; P stone_window ($X + 600) $Y $S1 90; P stone_half_wall ($X + 400) ($Y + 600) $S1 0; P stone_wall ($X + 600) ($Y + 400) $S1 90
# a gable roof: wood over the west half, stone over the east, gables closing both sides
P wood_roof $X $Y $R 90; P wood_roof $X ($Y + 400) $R 270; P stone_roof ($X + 400) $Y $R 90; P stone_roof ($X + 400) ($Y + 400) $R 270
P wood_gable ($X - 200) $Y $R 90; P wood_gable ($X - 200) ($Y + 400) $R 270; P stone_gable ($X + 600) $Y $R 90; P stone_gable ($X + 600) ($Y + 400) $R 270
# E: stone stairs; H: a stone hut (roof rising east); I: a wood hut (roof rising west); a doorway between them
Add "WRTeleport $($X + 300) $($Y - 300)", 'wait 2'
P stone_stairs ($X + 800) ($Y + 400) $T 0
P wood_door ($X + 400) ($Y - 600) $T 0; P stone_doorway ($X + 200) ($Y - 400) $T 90; P stone_wall ($X + 600) ($Y - 400) $T 90; P stone_pillar ($X + 600) ($Y - 600) $T 0
P stone_roof ($X + 400) ($Y - 400) $S1 0; P stone_gable ($X + 400) ($Y - 600) $S1 0; P stone_upper_wall ($X + 600) ($Y - 400) $S1 90
P wood_wall $X ($Y - 600) $T 0; P wood_wall ($X - 200) ($Y - 400) $T 90
P wood_roof $X ($Y - 400) $S1 180; P wood_gable $X ($Y - 600) $S1 180; P wood_upper_wall ($X - 200) ($Y - 400) $S1 90
# furniture in the house (floor top = T)
Add "WRTeleport $($X + 100) $($Y + 200)", 'wait 2'
P storage_chest ($X - 120) ($Y + 120) $T 90; P table ($X + 20) ($Y - 60) $T 0; P chair ($X + 20) ($Y + 20) $T 180
P bed ($X - 60) ($Y + 470) $T 90; P bedroll ($X + 110) ($Y + 470) $T 90; P rug ($X + 20) ($Y + 400) $T 0
P banner ($X - 180) ($Y + 300) $T 0; P trophy_mount ($X + 40) ($Y - 185) $T 0; P torch ($X + 500) ($Y + 120) $T 0; P brazier ($X + 450) ($Y - 100) $T 0
# the yard: the claim first (a Hearth needs one), the stations in a row, the Hearth; around it the defences and farm
Ghost claim_totem "$($X + 1100) $($Y - 1300)" '0 -30'
Ghost workbench "$($X + 1100) $($Y - 900)" '0 -30'
Ghost forge "$($X + 1100) $($Y - 600)" '0 -30'
Ghost armorer "$($X + 1100) $($Y - 300)" '0 -30'
Ghost alchemy_table "$($X + 1100) $Y" '0 -30'
Ghost hearth "$($X + 1100) $($Y + 700)" '0 -32'
Ghost palisade_wall "$($X - 200) $($Y - 1400)" '270 -30'
Ghost palisade_gate "$($X + 300) $($Y - 1400)" '270 -30'
Ghost guard_tower "$($X - 1200) $($Y - 1000)" '270 -25'
Ghost crop_plot "$($X - 1300) $Y" '180 -30'
Ghost animal_pen "$($X + 600) $($Y - 2200)" '270 -25'
Add 'WRCmd claimdeposit', 'WRGoTo hearth', 'wait 1', 'WRCmd hearthfeed', 'wait 1', 'WRView tp', 'WRMenu none 0'
Hour 12
Look "$($X + 2200) $($Y - 1500)" '145 -10'; Shot 'noon_se'
Look "$($X - 1400) $($Y + 1800)" '-40 -12'; Shot 'noon_nw'
Look "$($X - 1500) $($Y - 1600)" '40 -8'; Shot 'noon_sw'
Cam "$($X + 1100) $($Y - 1100) $($T + 900) 135 -30"; if (-not $Baseline) { Shot 'noon_high'; Add 'wr.Cam off' }
Cam "$($X + 150) $($Y + 150) $($T + 170) 200 -12"; if (-not $Baseline) { Shot 'noon_inside'; Add 'wr.Cam off' }
Hour 19.7
Look "$($X + 2200) $($Y - 1500)" '145 -10'; Shot 'dusk_se'
Hour 0.5
Look "$($X + 2200) $($Y - 1500)" '145 -10'; Shot 'night_se'
Look "$($X - 1400) $($Y + 1800)" '-40 -12'; Shot 'night_nw'
Cam "$($X + 150) $($Y + 150) $($T + 170) 200 -12"; if (-not $Baseline) { Shot 'night_inside'; Add 'wr.Cam off' }

# ---------------------------------------------------------------- frame time, same spots and times before / after
Begin 'perf'
Hour 12
Add 'stat unit', 't.MaxFPS 0'
foreach ($spot in @(@('base', "$($X + 2200) $($Y - 1500)", '145 -10'), @('beach', '-151000 77000', '153 -6'), @('slope', '-120000 10000', '20 -4'), @('forest', '-90000 -72500', '90 -3'))) {
	Look $spot[1] $spot[2] 6
	Add 'wait 4'
	if (-not $Baseline) { Add 'wr.FrameStats 4', 'wait 5' }
	UIShot $spot[0]
}
Add 'stat unit', 't.MaxFPS 60'

# ---------------------------------------------------------------- the sea
Begin 'coast'
Hour 12
Look '-153700 78300' '153 -6'; Shot 'beach_volcanic_noon'
Look '-153700 78300' '100 -14'; Shot 'beach_volcanic_noon_side'
Look '174100 77500' '24 -6'; Shot 'beach_tundra_noon'
Look '-19360 -184185' '264 -28'; Shot 'cliff_volcanic_noon'
Look '18370 -174740' '276 -22'; Shot 'cliff_tundra_noon'
Cam '-160500 79000 1500 160 -12'; if (-not $Baseline) { Shot 'waves_low'; Add 'wr.Cam off' }
Hour 19.4
Look '-153700 78300' '153 -6'; Shot 'beach_volcanic_dusk'
Look '-19360 -184185' '264 -28'; Shot 'cliff_volcanic_dusk'
Hour 0.5
Look '-153700 78300' '153 -6'; Shot 'beach_volcanic_night'
Look '174100 77500' '24 -6'; Shot 'beach_tundra_night'
Look '18370 -174740' '276 -22'; Shot 'cliff_tundra_night'

# ---------------------------------------------------------------- biomes at noon, dusk and dawn
Begin 'biomes'
foreach ($h in @(12, 19.6, 6.4)) {
	Hour $h
	Look '-90000 -72500' '90 -3'; Shot "volcanic_forest_$h"
	Look '-90000 -40000' '160 -2'; Shot "volcanic_valley_$h"
	Look '66000 15500' '90 -3'; Shot "tundra_forest_$h"
	Look '100000 -60000' '30 -2'; Shot "tundra_valley_$h"
}
# Into a low sun through the trees (the light shafts): west at dusk, east at dawn.
Hour 19.6
Look '-88500 -70000' '185 2'; Shot 'shafts_dusk_volcanic'
Look '67500 18000' '185 2'; Shot 'shafts_dusk_tundra'
Hour 6.4
Look '-91500 -70000' '5 2'; Shot 'shafts_dawn_volcanic'
Hour 0.5
Look '-90000 -72500' '90 -3'; Shot 'volcanic_night'
Look '66000 15500' '90 -3'; Shot 'tundra_night'
Hour 12
Look '-5000 0' '0 -10'; Shot 'ridge_noon'

# ---------------------------------------------------------------- stations, the Hearth and the totem up close
Begin 'stations'
foreach ($h in @(12, 0.5)) {
	Hour $h
	Add 'WRView fp'
	Look "$($X + 1000) $($Y - 750)" '0 -24'; Shot "workbench_forge_$h"
	Look "$($X + 1000) $($Y - 150)" '0 -24'; Shot "armorer_alchemy_$h"
	Look "$($X + 1700) $($Y - 450)" '180 -20'; Shot "stations_back_$h"
	Look "$($X + 1000) $($Y + 700)" '0 -28'; Shot "hearth_$h"
	Look "$($X + 1000) $($Y - 1300)" '0 4'; Shot "totem_$h"
	Add 'WRView tp'
	Cam "$($X - 20) $($Y + 60) $($T + 160) 200 -25"; if (-not $Baseline) { Shot "chest_$h"; Add 'wr.Cam off' }
}

# ---------------------------------------------------------------- the camera against a wall (frames while turning)
# Just outside C's west wall, back to it: the boom swings into the wall and out again.
Begin 'camera'
Hour 12
Add 'WRView tp'
Look "$($X - 260) $($Y + 400)" '180 -10'
foreach ($yaw in @(180, 150, 120, 100, 90)) { Add "WRLook $yaw -10", 'wait 0.15'; Shot "wall_$yaw" }
# Facing west the arm reaches back into the wall (pulled in); turning to face the wall frees it, and it eases back out
# (shots right after the turn, then a moment later). HighResShot takes its picture on the next frame.
if (Wanted) {
	Add 'WRLook 10 -10', 'wait 1.5', 'WRLook 180 -10', 'wait 1.5', 'WRLook 10 -10', 'HighResShot 1600x900', 'wait 0.25', 'HighResShot 1600x900', 'wait 1.2', 'HighResShot 1600x900', 'wait 1.5'
	[void]$labels.Add('camera_away_0s'); [void]$labels.Add('camera_away_0.3s'); [void]$labels.Add('camera_away_1.5s')
}
Look "$($X - 1200) $($Y - 800)" '270 -5'; Shot 'tower_close'

# ---------------------------------------------------------------- foliage: two frames a second apart (wind)
# A free camera (no survivor breathing in the frame) over a stand of trees and shrubs: what moved is the wind.
Begin 'foliage'
Hour 12
Look '-90000 -73500' '90 -3'
if ($Baseline) { Add 'WRLook 60 -8' } else { Cam '-90400 -71200 9850 60 -12' }
if (Wanted) { Add 'HighResShot 1600x900', 'wait 1.0', 'HighResShot 1600x900', 'wait 1.5'; [void]$labels.Add('foliage_a'); [void]$labels.Add('foliage_b') }
if (-not $Baseline) { Add 'wr.Cam off' }

# ---------------------------------------------------------------- distant decoration: does it pop in, and what would more cost
# From 200 m up, 400 m short of a volcanic forest: decoration as it is, then with every draw distance tripled.
Begin 'popin'
Hour 12
Look '-90000 -100000' '90 -10' 6
Cam '-90000 -112000 30000 90 -20'
Add 'stat unit', 't.MaxFPS 0', 'wait 3'
if (-not $Baseline) { Add 'wr.FrameStats 4', 'wait 5' }
UIShot 'distance_1x'
Add 'r.ViewDistanceScale 3', 'wait 4'
if (-not $Baseline) { Add 'wr.FrameStats 4', 'wait 5' }
UIShot 'distance_3x'
Add 'r.ViewDistanceScale 1', 'stat unit', 't.MaxFPS 60'; if (-not $Baseline) { Add 'wr.Cam off' }

# ---------------------------------------------------------------- A/B look checks (only with -Only checks)
# The same view with one thing switched off: the valley mist, the sea's moving surface, its painted highlights.
if ($Only -contains 'checks' -and -not $Baseline) {
	Begin 'checks'
	Hour 0.5
	Look '18370 -174740' '276 -22'; Shot 'cliff_tundra_night'
	Add 'wr.MistScale 0', 'wait 1'; Shot 'cliff_tundra_night_nomist'; Add 'wr.MistScale 1'
	Hour 19.4
	Look '-19360 -184185' '264 -28'; Shot 'cliff_dusk'
	Add 'wr.SeaParam Master_Intensity 0', 'wait 1'; Shot 'cliff_dusk_no_wpo'; Add 'wr.SeaParam Master_Intensity 2'
	Add 'wr.SeaParam FakeSpec_Intensity 0', 'wait 1'; Shot 'cliff_dusk_no_fakespec'; Add 'wr.SeaParam FakeSpec_Intensity 0.5'
	Add 'wr.SeaParam Refraction 0', 'wait 1'; Shot 'cliff_dusk_no_refraction'; Add 'wr.SeaParam Refraction 0.1'
}

$emit = $true
Add 'wait 2', 'WRSay worldshots-done', 'wait 2'

# ---------------------------------------------------------------- run
$cmdLine = ($cmds -join '|')
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=worldshots -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=f -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmdLine`" -abslog=`"$logs\client.log`"" -PassThru
$deadline = (Get-Date).AddSeconds(300 + 4 * $cmds.Count)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$logs\server.log" -Pattern 'worldshots-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_worldshots.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$files = @(Get-ChildItem $shotDir -Include 'ScreenShot*.png', 'HighresScreenshot*.png' -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
	$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
	Copy-Item $files[$i].FullName $dest -Force
}
Select-String -Path "$logs\server.log" -Pattern 'BUILD |refused|no support|already there|overlaps|too far|need \d' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' } | Set-Content "$logs\build.txt"
Select-String -Path "$logs\client.log" -Pattern 'LogWildreach: (FRAMESTATS|GHOST |SEA |CMD .*-> (need|no|some|over|too))' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' }
$errors = @(Select-String -Path "$logs\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Select-Object -First 8)
if ($errors) { "client errors:"; $errors | ForEach-Object { '  ' + $_.Line.Trim() } }
"{0} of {1} shots (logs: {2}, out: {3})" -f [Math]::Min($files.Count, $labels.Count), $labels.Count, $logs, $Out
$lock.ReleaseMutex()
