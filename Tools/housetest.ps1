# Building test on L_Island: a bot builds a two-room, two-storey wooden house on the flat spot south of the middle
# volcanic spawn, the way a player would, and walks around it.
#   ground floor: 2 x 2 foundations, outer walls with a front door and a window, an inner wall with a doorway (two rooms)
#   stairs in the north-east cell up to the first floor (that cell stays open above them), first-floor walls and windows
#   a gable roof: four sloped roof pieces meeting at a ridge, four gables closing the sides
# Some pieces go through the build ghost exactly as a player places them (aim, the ghost snaps, click), and the test
# checks where they snapped: foundations levelling with their neighbour, a wall on a cell edge, a ceiling on top of the
# storey you aim into, a first-floor wall from the first floor. It also checks what must be refused (a ceiling with no
# walls under it yet, a wall on top of the door, a floor over the stairs), demolishing, the stairs climb, and walking in
# through the front door and the inner doorway.
#   housetest.ps1          headless
#   housetest.ps1 -Shots   rendered client: the house from three sides, in at the front door, down the stairs
# The island's world save is backed up and put back afterwards.
param([switch]$Shots)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$db = Join-Path $root 'Saved\ServerDB'
$out = Join-Path $env:TEMP ("wildreach-house-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$name = 'HouseBuilder'
$worldSave = Join-Path $db 'world_L_Island.json'
Remove-Item (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue
if (Test-Path $worldSave) { Copy-Item $worldSave "$out\world_backup.json" }

# Cells: A (X, Y) south-west, B east of it, C north of A, D north-east. Ground ~4797 here; foundations sit 60 cm in.
$X = -132000; $Y = -56400; $Zf = 4734
$T = $Zf + 100; $S1 = $T + 300; $R = $T + 600   # ground floor, first floor, roof level
function P([string]$piece, [double]$x, [double]$y, [double]$z, [double]$yaw) { "WRPlace $piece $x $y $z $yaw" }
# Through the ghost: stand somewhere, look (yaw, pitch; first person), pick the piece, let it snap, place it.
function Ghost([string]$piece, [string]$at, [string]$look) { "WRTeleport $at|wait 2|WRLook $look|WRGhost $piece|wait 1|WRGhostPlace|wait 0.5" }
function Shot([string]$at, [string]$look) { if ($Shots) { "WRTeleport $at|wait 3|WRLook $look|wait 2|HighResShot 1600x900|wait 1" } else { $null } }

$cmds = @(
	'WRBotPause 1', 'wait 2', 'WRGive wood 1200', 'WRView fp',
	# Foundations: A at a known height; B, C and D through the ghost, aimed at the ground in their cell from outside.
	"WRTeleport $($X - 500) $Y", 'wait 2', (P wood_foundation $X $Y $Zf 0),
	(Ghost wood_foundation "$($X - 500) $($Y + 400)" '0 -20'),          # C, from the west
	(Ghost wood_foundation "$($X + 900) $Y" '180 -20'),                # B, from the east
	(Ghost wood_foundation "$($X + 900) $($Y + 400)" '180 -20'),       # D
	# A ceiling over A with nothing under it yet: the ghost says no support (and so does the server).
	(Ghost wood_floor "$($X - 100) $Y" '0 -38'),
	# Ground floor, from the middle of the house: outer walls, the front door (B east), a window (A west), the inner
	# wall between C and D and a doorway between A and B. C's west wall goes through the ghost from outside.
	"WRTeleport $($X + 100) $($Y + 200)", 'wait 2',
	(P wood_wall $X ($Y - 200) $T 0), (P wood_wall ($X + 400) ($Y - 200) $T 0),
	(P wood_wall $X ($Y + 600) $T 0), (P wood_wall ($X + 400) ($Y + 600) $T 0),
	(P wood_window ($X - 200) $Y $T 90),
	(P wood_door ($X + 600) $Y $T 90), (P wood_wall ($X + 600) ($Y + 400) $T 90),
	(P wood_doorway ($X + 200) $Y $T 90), (P wood_wall ($X + 200) ($Y + 400) $T 90),
	(Ghost wood_wall "$($X - 700) $($Y + 400)" '0 -19'),               # C west, aimed at the ground just outside it
	# First-floor floors: C's through the ghost (stand in C, aim at its floor: the ceiling goes on top of that storey),
	# A's and B's placed; D stays open for the stairs, which rise from D's north side south onto B's floor.
	(Ghost wood_floor "$($X - 100) $($Y + 400)" '0 -38'),
	(P wood_floor $X $Y $S1 0), (P wood_floor ($X + 400) $Y $S1 0),
	(P wood_stairs ($X + 400) ($Y + 400) $T 270),
	# Refused: a wall where the door is, a floor over the stairs.
	(P wood_wall ($X + 600) $Y $T 90), (P wood_floor ($X + 400) ($Y + 400) $S1 0),
	# First-floor walls (stacked on the ground-floor ones), windows east and west. B's south wall comes from upstairs.
	(P wood_wall $X ($Y - 200) $S1 0), (P wood_wall $X ($Y + 600) $S1 0), (P wood_wall ($X + 400) ($Y + 600) $S1 0),
	(P wood_window ($X - 200) $Y $S1 90), (P wood_wall ($X - 200) ($Y + 400) $S1 90),
	(P wood_window ($X + 600) $Y $S1 90), (P wood_wall ($X + 600) ($Y + 400) $S1 90),
	# Demolish: a pillar at A's outer corner goes back up for half its cost.
	(P wood_pillar ($X - 200) ($Y - 200) $T 0), 'wait 1', "WRDemolish $($X - 200) $($Y - 200) $T",
	# Up the stairs: onto their foot, walk south up them onto B's floor (counts as arrived 4 m short: on B).
	"WRTeleport $($X + 400) $($Y + 550)", 'wait 2',
	$(if ($Shots) { "WRLook 270 12|wait 2|HighResShot 1600x900|wait 1" }),             # up the stairs from their foot
	"WRWalkTo $($X + 400) $($Y - 500)", 'wait 6',
	$(if ($Shots) { "WRLook 90 -32|wait 2|HighResShot 1600x900|wait 1" }),             # down the stairs
	# From B's floor: its south wall through the ghost (aim at the floor near the edge), then the roof.
	'WRLook 270 -70', 'WRGhost wood_wall', 'wait 1', 'WRGhostPlace', 'wait 0.5',
	(P wood_roof $X $Y $R 90), (P wood_roof ($X + 400) $Y $R 90), (P wood_roof $X ($Y + 400) $R 270), (P wood_roof ($X + 400) ($Y + 400) $R 270),
	(P wood_gable ($X - 200) $Y $R 90), (P wood_gable ($X - 200) ($Y + 400) $R 270),
	(P wood_gable ($X + 600) $Y $R 90), (P wood_gable ($X + 600) ($Y + 400) $R 270),
	'wait 1', 'WRView tp',
	(Shot "$($X + 1300) $($Y - 900)" '135 -4'),                                          # from the south-east (front door)
	(Shot "$($X - 900) $($Y + 1300)" '315 -4'),                                          # from the north-west
	(Shot "$($X - 900) $($Y - 900)" '45 -4'),                                            # from the south-west
	# In through the front door: open it from outside, walk west through B and the doorway into A.
	"WRTeleport $($X + 800) $Y", 'wait 2', 'WRLook 180 -5', 'WRInteract', 'wait 1',
	$(if ($Shots) { "WRView fp|WRLook 180 -8|wait 2|HighResShot 1600x900|wait 1|WRView tp" }),  # in at the door
	"WRWalkTo $($X - 300) $Y", 'wait 6', 'WRSay housetest-done', 'wait 2'
)
# (a foreach, not Where-Object: piping drops the entries after an empty $(if ...) one)
$cmds = @(foreach ($c in $cmds) { if ($c) { $c } }) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7836 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$render = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7836?Path=fire?WRName=$name -game $render -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
$deadline = (Get-Date).AddSeconds($(if ($Shots) { 360 } else { 240 }))
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$out\server.log" -Pattern 'housetest-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 3; break }
}
if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
# The house survives a restart: wait for the world autosave (every 60 s) to hold all of it, then load it again.
$deadline = (Get-Date).AddSeconds(75)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path "$out\server.log" -Pattern 'WORLD saved 34 pieces' -Quiet)) { Start-Sleep -Seconds 3 }
if (-not $s.HasExited) { Stop-Process -Id $s.Id -Force }
$s2 = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7836 -abslog=`"$out\server2.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	if ((Test-Path "$out\server2.log") -and (Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded' -Quiet)) { Start-Sleep -Seconds 3; break }
}
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }
Start-Sleep -Seconds 2
if (Test-Path "$out\world_backup.json") { Copy-Item "$out\world_backup.json" $worldSave -Force } else { Remove-Item $worldSave -ErrorAction SilentlyContinue }

# ---- results
$notices = @(Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','').Trim() })
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') })
$ghosts = @(Select-String -Path "$out\client.log" -Pattern 'GHOST (\S+) at V\(X=([-\d.]+), Y=([-\d.]+), Z=([-\d.]+)\) yaw ([-\d.]+) (valid|invalid) ?(.*)$' | ForEach-Object {
	$m = $_.Matches[0].Groups; [pscustomobject]@{ piece = $m[1].Value; x = [double]$m[2].Value; y = [double]$m[3].Value; z = [double]$m[4].Value; yaw = [double]$m[5].Value; valid = $m[6].Value -eq 'valid'; why = $m[7].Value.Trim() } })
$walks = @(Select-String -Path "$out\client.log" -Pattern 'WALK arrived .* at V\(X=([-\d.]+), Y=([-\d.]+), Z=([-\d.]+)\)' | ForEach-Object {
	$m = $_.Matches[0].Groups; [pscustomobject]@{ x = [double]$m[1].Value; y = [double]$m[2].Value; z = [double]$m[3].Value } })
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function GhostAt([int]$i, [string]$piece, [double]$x, [double]$y, [double]$z, [bool]$valid, [string]$what, [double]$yaw = -1) {
	$g = if ($i -lt $ghosts.Count) { $ghosts[$i] } else { $null }
	$ok = $g -and $g.piece -eq $piece -and [math]::Abs($g.x - $x) -lt 1 -and [math]::Abs($g.y - $y) -lt 1 -and [math]::Abs($g.z - $z) -lt 1 -and $g.valid -eq $valid -and ($yaw -lt 0 -or [math]::Abs((($g.yaw - $yaw) % 360 + 360) % 360) -lt 1)
	Check "ghost: $what" $ok $(if ($g) { "{0} at {1:N0} {2:N0} {3:N0} yaw {4:N0} {5} {6}" -f $g.piece, $g.x, $g.y, $g.z, $g.yaw, $(if ($g.valid) { 'valid' } else { 'invalid' }), $g.why } else { 'missing' })
}
function Count([string]$text) { @($notices | Where-Object { $_ -eq $text }).Count }

GhostAt 0 wood_foundation $X ($Y + 400) $Zf $true 'a foundation levels with the one next to it (C)'
GhostAt 1 wood_foundation ($X + 400) $Y $Zf $true 'a foundation levels with the one next to it (B)'
GhostAt 2 wood_foundation ($X + 400) ($Y + 400) $Zf $true 'a foundation levels with the one next to it (D)'
GhostAt 3 wood_floor $X $Y $S1 $false 'a ceiling with no walls under it is refused before you click'
GhostAt 4 wood_wall ($X - 200) ($Y + 400) $T $true 'a wall snaps to the cell edge you aim at' 90
GhostAt 5 wood_floor $X ($Y + 400) $S1 $true 'a ceiling goes on top of the storey you aim into'
GhostAt 6 wood_wall ($X + 400) ($Y - 200) $S1 $true 'aiming at the first floor builds a first-floor wall' 0
Check 'built 4 foundations' ((Count 'Built Wood foundation') -eq 4)
Check 'built 13 walls (7 on the ground floor, 6 upstairs)' ((Count 'Built Wood wall') -eq 13) "$(Count 'Built Wood wall')"
Check 'built the front door, the inner doorway and 3 windows' ((Count 'Built Wood doorway + door') -eq 1 -and (Count 'Built Wood doorway (no door)') -eq 1 -and (Count 'Built Wood window wall') -eq 3)
Check 'built 3 first-floor floors' ((Count 'Built Wood floor / ceiling') -eq 3)
Check 'built the stairs' ((Count 'Built Wood stairs (one storey)') -eq 1)
Check 'built 4 roof pieces and 4 gables' ((Count 'Built Wood roof (sloped)') -eq 4 -and (Count 'Built Wood gable (roof side)') -eq 4)
Check 'refused: a ceiling before its walls' ((Count 'no support (wood spans 4)') -eq 1)
Check 'refused: a wall on top of the door' ((Count 'something is already there (Wood doorway + door)') -eq 1)
Check 'refused: a floor over the stairs' ((Count 'the stairs need this opening') -eq 1)
$unexpected = @($notices | Where-Object { $_ -match '^(no support|something is already|the stairs|need |too far|no building|not yours|nothing to)' } | Where-Object { $_ -notin 'no support (wood spans 4)', 'something is already there (Wood doorway + door)', 'the stairs need this opening' })
Check 'nothing else refused' ($unexpected.Count -eq 0) ($unexpected -join '; ')
Check 'demolished the pillar for half back' ((Count 'Demolished Wood pillar (half refunded)') -eq 1)
$supports = @($server | ForEach-Object { if ($_ -match 'BUILD \S+ placed (\S+) at .*\(support (\d+)\)') { [pscustomobject]@{ piece = $Matches[1]; support = [int]$Matches[2] } } })
$roofMax = ($supports | Where-Object { $_.piece -in 'wood_roof', 'wood_gable' } | Measure-Object -Property support -Maximum).Maximum
Check 'upstairs walls stand on the walls below (roof and gables 2 or less from the ground)' ($roofMax -le 2) "max $roofMax"
Check 'nothing collapsed' (@($server | Where-Object { $_ -match 'collapsed' }).Count -eq 0)
Check 'climbed the stairs onto the first floor' ($walks.Count -ge 1 -and $walks[0].z -gt $S1 + 60 -and $walks[0].y -lt $Y + 200) $(if ($walks.Count -ge 1) { "at {0:N0} {1:N0} {2:N0}" -f $walks[0].x, $walks[0].y, $walks[0].z })
Check 'the front door opens' ((Count 'Opened') -eq 1)
Check 'walked in through the front door and the inner doorway' ($walks.Count -ge 2 -and $walks[1].x -lt $X + 100 -and $walks[1].z -lt $T + 150) $(if ($walks.Count -ge 2) { "at {0:N0} {1:N0} {2:N0}" -f $walks[1].x, $walks[1].y, $walks[1].z })
$reload = @(Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded (\d+) pieces' -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.Matches[0].Groups[1].Value })
$reloadCollapsed = @(Select-String -Path "$out\server2.log" -Pattern 'collapsed' -ErrorAction SilentlyContinue).Count
Check 'a server restart loads the whole house back (34 pieces, nothing collapses)' ($reload.Count -eq 1 -and $reload[0] -eq 34 -and $reloadCollapsed -eq 0) $(if ($reload.Count) { "$($reload[0]) pieces, $reloadCollapsed collapsed" } else { 'not loaded' })
$errors = @(Select-String -Path "$out\server.log", "$out\server2.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
# This run's own screenshots (other sessions may be saving theirs into the same folder).
if ($Shots) { Select-String -Path "$out\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value } }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
