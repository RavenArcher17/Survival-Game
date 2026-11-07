# Placing a piece: through the build ghost, as a player does it, on the island's flat spot south of the middle spawn.
#   Free: the ghost faces the way you look, the rotate key and the wheel turn it from there, any angle.
#   Its shape: stations, the Hearth and the chest show their real models; structure pieces their real planks.
#   On a foundation: furniture snaps to the marks and sits square, finer on the fine grid; the fine grid takes walls on inner
#   lines and pillars on their crossings; a foundation placed at an angle takes what's built on and beside it to that angle.
# Every spot is checked as the ghost shows it and as the server takes it.
param([int]$Port = 7853)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-buildaim-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$env:UE_SKIP_UBT_SDK_SETUP = '1'
Remove-Item -Recurse -Force (Join-Path $root 'Saved\Worlds\buildaimtest') -ErrorAction SilentlyContinue

$X = -132000; $Y = -56400; $Zf = 4734; $T = $Zf + 100
# A foundation at 30 degrees, west of the first. Its local +X edge's middle, its local +Y edge's middle, a cell beyond.
$X2 = $X - 1200; $Y2 = $Y; $Z2 = 4874; $T2 = $Z2 + 100   # (the ground's a little higher over there)
$c30 = [math]::Cos([math]::PI / 6); $s30 = [math]::Sin([math]::PI / 6)
function Local2([double]$lx, [double]$ly) { '{0:0} {1:0}' -f ($X2 + $lx * $c30 - $ly * $s30), ($Y2 + $lx * $s30 + $ly * $c30) }

$cmds = @('WRBotPause 1', 'wait 2', 'WRGive wood 2000', 'WRGive stone 500', 'WRGive hide 50', 'WRGive fiber 50', 'WRGive copper_ore 20', 'WRGive copper_ingot 20', 'WRView fp',
	# Free, on open ground: it faces the way you look; the rotate key and the wheel turn it from there.
	'WRSay phase-free', 'WRSnap off', "WRTeleport $($X - 500) $Y", 'wait 2', 'WRLook 0 -30', 'WRGhost bed', 'wait 1', 'WRGhostInfo',
	'WRLook 40 -30', 'wait 0.4', 'WRGhostInfo', 'WRLook 123 -30', 'wait 0.4', 'WRGhostInfo',
	'WRGhostTurn', 'wait 0.3', 'WRGhostInfo', 'WRGhostTurn 1', 'wait 0.3', 'WRGhostInfo', 'WRGhostTurn -1', 'wait 0.3', 'WRGhostInfo',
	# Grid on, furniture on open ground stays free too. Looking level, past your reach: it comes down on the ground, still placeable.
	'WRSnap grid', 'WRGhost storage_chest', 'WRLook 77 -30', 'wait 0.5', 'WRGhostInfo', 'WRLook 0 -3', 'wait 0.5', 'WRGhostInfo', 'WRLook 0 60', 'wait 0.5', 'WRGhostInfo',
	# Shapes.
	'WRSay phase-shapes', 'WRGhost armorer', 'wait 0.5', 'WRGhostInfo', 'WRGhost workbench', 'wait 0.5', 'WRGhostInfo', 'WRGhost forge', 'wait 0.5', 'WRGhostInfo', 'WRGhost hearth', 'wait 0.5', 'WRGhostInfo',
	'WRGhost claim_totem', 'wait 0.5', 'WRGhostInfo', 'WRGhost wood_wall', 'wait 0.5', 'WRGhostInfo', 'WRGhost bed', 'wait 0.5', 'WRGhostInfo',
	# Furniture on a foundation: on the marks, square, the rotate key a quarter turn.
	'WRSay phase-grid', "WRPlace wood_foundation $X $Y $Zf 0", 'wait 1', 'WRGhost storage_chest', "WRLookAt $($X + 37) $($Y + 62) $T", 'wait 0.6', 'WRGhostInfo', 'WRGhostTurn', 'wait 0.4', 'WRGhostInfo', 'WRGhostPlace', 'wait 1',
	# The fine grid: finer marks for furniture, eighths of a turn; a wall on an inner line, a pillar on a crossing.
	'WRSay phase-fine', 'WRSnap fine', 'WRGhost brazier', "WRLookAt $($X - 60) $($Y - 105) $T", 'wait 0.6', 'WRGhostInfo', 'WRGhostTurn', 'wait 0.4', 'WRGhostInfo', 'WRGhostPlace', 'wait 1',
	'WRGhost wood_wall', "WRLookAt $($X + 97) $($Y - 20) $T", 'wait 0.6', 'WRGhostPlace', 'wait 1',
	'WRGhost wood_pillar', "WRLookAt $($X - 110) $($Y + 90) $T", 'wait 0.6', 'WRGhostPlace', 'wait 1',
	# The plain grid still means the edges: same aim, the nearest edge.
	'WRSnap grid', 'WRGhost wood_wall', "WRLookAt $($X + 97) $($Y - 20) $T", 'wait 0.6', 'WRGhostInfo',
	# A foundation placed at an angle: furniture square to it, walls on its own edges (and they may share a corner), a neighbour on its own grid.
	'WRSay phase-angle', "WRTeleport $($X2 - 500) $Y2", 'wait 2', "WRPlace wood_foundation $X2 $Y2 $Z2 30", 'wait 1',
	'WRGhost wood_wall', "WRLookAt $(Local2 190 5) $T2", 'wait 0.6', 'WRGhostPlace', 'wait 1',
	'WRGhost wood_wall', "WRLookAt $(Local2 5 190) $T2", 'wait 0.6', 'WRGhostPlace', 'wait 1',
	'WRGhost storage_chest', "WRLookAt $(Local2 -96 -104) $T2", 'wait 0.6', 'WRGhostPlace', 'wait 1',
	'WRGhost wood_foundation', "WRLookAt $(Local2 -400 0) $($Z2 + 60)", 'wait 0.6', 'WRGhostPlace', 'wait 1',
	'WRSay buildaim-done', 'wait 1') -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -wrworld=buildaimtest -wrtune=wildlife.enabled:0 -unattended -nosound -port=$Port -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:$($Port)?Path=fire?WRName=Placer -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(160)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path "$out\server.log" -Pattern 'CHAT .*buildaim-done' -Quiet)) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Clean($line) { ($line -replace '^.*LogWildreach: (Verbose: )?', '').Trim() }
$client = @(Select-String -Path "$out\client.log" -Pattern 'LogWildreach: ' | ForEach-Object { Clean $_.Line })
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { Clean $_.Line })
function Ghosts([string]$phase) {
	$on = $false; $list = @()
	foreach ($l in $client) {
		if ($l -match 'CHAT .*phase-') { $on = $l -match "phase-$phase\b" }
		elseif ($on -and $l -match '^GHOST (\S+) at V\(X=(-?[\d.]+), ?Y=(-?[\d.]+), ?Z=(-?[\d.]+)\) yaw (-?\d+) (valid|invalid) (.*)\| snap (\d) step (\d+) look (.*)$') {
			$list += [pscustomobject]@{ piece = $Matches[1]; x = [double]$Matches[2]; y = [double]$Matches[3]; z = [double]$Matches[4]; yaw = [double]$Matches[5]; valid = $Matches[6] -eq 'valid'; why = $Matches[7].Trim(); snap = [int]$Matches[8]; step = [double]$Matches[9]; look = $Matches[10].Trim(); line = $l }
		}
	}
	, $list
}
function Near([double]$a, [double]$b, [double]$slack = 1.5) { [math]::Abs($a - $b) -le $slack }
function NearYaw([double]$a, [double]$b, [double]$slack = 1.5) { $d = (($a - $b) % 360 + 540) % 360 - 180; [math]::Abs($d) -le $slack }
function Built([string]$piece, [double]$x, [double]$y) {
	foreach ($l in $server) { if ($l -match "^BUILD Placer placed $piece at V\(X=(-?[\d.]+), ?Y=(-?[\d.]+)" -and (Near ([double]$Matches[1]) $x 2) -and (Near ([double]$Matches[2]) $y 2)) { return $true } }
	$false
}

$free = Ghosts 'free'
if ($free.Count -ge 7) {
	Check 'free: the ghost faces the way you look' ((NearYaw $free[0].yaw 0) -and (NearYaw $free[1].yaw 40) -and (NearYaw $free[2].yaw 123)) "yaw $($free[0].yaw), $($free[1].yaw), $($free[2].yaw) at looks 0, 40, 123"
	Check 'free: the rotate key turns it on from there' (NearYaw $free[3].yaw 138) "yaw $($free[3].yaw)"
	Check 'free: the wheel turns it finely, both ways' ((NearYaw $free[4].yaw 133) -and (NearYaw $free[5].yaw 138)) "yaw $($free[4].yaw), $($free[5].yaw)"
	Check 'furniture on open ground stays free with the grid on' ((NearYaw $free[6].yaw 77) -and $free[6].step -eq 0) "yaw $($free[6].yaw), step $($free[6].step)"
	Check 'looking past your reach, it comes down on the ground' ($free.Count -ge 9 -and $free[7].valid -and [math]::Abs($free[7].z - 4800) -lt 300) "$($free[7].line)"
	Check 'looking up at the sky it''s still on the ground, not in the air' ($free.Count -ge 9 -and $free[8].valid -and [math]::Abs($free[8].z - $free[7].z) -lt 300) "$($free[8].line)"
} else { Check 'free placement ran' $false "$($free.Count) ghost readings" }

$shapes = Ghosts 'shapes'
$want = @{ armorer = 'armorer_game'; workbench = 'workbench_game'; forge = 'forge_game'; hearth = 'hearth_game'; claim_totem = 'totem_game' }
foreach ($k in $want.Keys) {
	$g = $shapes | Where-Object { $_.piece -eq $k } | Select-Object -First 1
	Check "the $k ghost is its real model" ($g -and $g.look -eq $want[$k]) "$(if ($g) { $g.look } else { 'no reading' })"
}
foreach ($k in 'wood_wall', 'bed') {
	$g = $shapes | Where-Object { $_.piece -eq $k } | Select-Object -First 1
	$n = if ($g -and $g.look -match '^(\d+) blocks') { [int]$Matches[1] } else { 0 }
	Check "the $k ghost has its real shape, not one box" ($n -gt 3) "$(if ($g) { $g.look } else { 'no reading' })"
}

$grid = Ghosts 'grid'
if ($grid.Count -ge 3) {
	Check 'on a foundation: furniture lands on the marks' ((Near $grid[0].x $X) -and (Near $grid[0].y ($Y + 100))) "at $($grid[0].x - $X), $($grid[0].y - $Y) from its middle, aimed at 37, 62"
	Check '...square to it' ((($grid[0].yaw % 90) -eq 0) -and $grid[0].step -eq 90) "yaw $($grid[0].yaw), step $($grid[0].step)"
	Check '...and the rotate key turns it a quarter' (NearYaw $grid[1].yaw ($grid[0].yaw + 90)) "yaw $($grid[0].yaw) then $($grid[1].yaw)"
	Check 'the server took it there' (Built 'storage_chest' $X ($Y + 100))
} else { Check 'furniture on a foundation ran' $false "$($grid.Count) ghost readings" }

$fine = Ghosts 'fine'
if ($fine.Count -ge 6) {
	Check 'the fine grid: finer marks' ((Near $fine[0].x ($X - 50)) -and (Near $fine[0].y ($Y - 100))) "at $($fine[0].x - $X), $($fine[0].y - $Y), aimed at -60, -105"
	Check '...eighths of a turn' (NearYaw $fine[1].yaw ($fine[0].yaw + 45)) "yaw $($fine[0].yaw) then $($fine[1].yaw)"
	Check 'the server took the fine spot' (Built 'brazier' ($X - 50) ($Y - 100))
	$wall = $fine | Where-Object { $_.piece -eq 'wood_wall' } | Select-Object -First 1
	Check 'a wall on an inner line' ($wall -and $wall.valid -and (Near $wall.x ($X + 100)) -and (Near $wall.y $Y) -and (NearYaw $wall.yaw 90)) "$(if ($wall) { $wall.line })"
	Check 'the server took it' (Built 'wood_wall' ($X + 100) $Y)
	$pillar = $fine | Where-Object { $_.piece -eq 'wood_pillar' } | Select-Object -First 1
	Check 'a pillar on a crossing' ($pillar -and $pillar.valid -and (Near $pillar.x ($X - 100)) -and (Near $pillar.y ($Y + 100))) "$(if ($pillar) { $pillar.line })"
	Check 'the server took it too' (Built 'wood_pillar' ($X - 100) ($Y + 100))
	$edge = $fine | Where-Object { $_.piece -eq 'wood_wall' } | Select-Object -Last 1
	Check 'the plain grid still means the edges' ($edge -and $edge.snap -eq 1 -and (Near $edge.x ($X + 200)) -and (Near $edge.y $Y)) "$(if ($edge) { $edge.line })"
} else { Check 'the fine grid ran' $false "$($fine.Count) ghost readings" }

$angle = Ghosts 'angle'
function Yaw30Ok($g, [double]$quarters) { $g -and (NearYaw $g.yaw (30 + 90 * $quarters)) }
if ($angle.Count -ge 4) {
	$a = $angle[0]; $b = $angle[1]; $f = $angle[2]; $n = $angle[3]
	$lx = ($a.x - $X2) * $c30 + ($a.y - $Y2) * $s30; $ly = -($a.x - $X2) * $s30 + ($a.y - $Y2) * $c30
	Check 'at an angle: a wall on its own edge' ($a.valid -and (Near $lx 200 2) -and (Near $ly 0 2) -and (Yaw30Ok $a 1)) "$($a.line)"
	$lx = ($b.x - $X2) * $c30 + ($b.y - $Y2) * $s30; $ly = -($b.x - $X2) * $s30 + ($b.y - $Y2) * $c30
	Check '...and the next, sharing a corner' ($b.valid -and (Near $lx 0 2) -and (Near $ly 200 2) -and (Yaw30Ok $b 0)) "$($b.line)"
	Check 'the server took both walls' ((Built 'wood_wall' $a.x $a.y) -and (Built 'wood_wall' $b.x $b.y))
	$lx = ($f.x - $X2) * $c30 + ($f.y - $Y2) * $s30; $ly = -($f.x - $X2) * $s30 + ($f.y - $Y2) * $c30
	$square = (($f.yaw - 30) % 90 + 90) % 90
	Check 'furniture is square to it, on its marks' ((Near $lx -100 2) -and (Near $ly -100 2) -and (Near $square 0)) "local $([math]::Round($lx)), $([math]::Round($ly)), yaw $($f.yaw)"
	$lx = ($n.x - $X2) * $c30 + ($n.y - $Y2) * $s30; $ly = -($n.x - $X2) * $s30 + ($n.y - $Y2) * $c30
	Check 'a neighbour foundation on its grid, at its angle' ((Near $lx -400 2) -and (Near $ly 0 2) -and (Yaw30Ok $n 0)) "local $([math]::Round($lx)), $([math]::Round($ly)), yaw $($n.yaw), $($n.line)"
	Check 'the server took the neighbour' (Built 'wood_foundation' $n.x $n.y)
} else { Check 'the foundation at an angle ran' $false "$($angle.Count) ghost readings" }

$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None|mesh .* not found' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
