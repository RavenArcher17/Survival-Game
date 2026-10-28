# Swimming and drinking on L_Island: a survivor walks off the volcanic beach into the sea and swims (CharacterMovement
# in the sea's water volume, floating at the surface), drinks there with E, swims back and walks up the beach, drinks
# at the water's edge, gets nothing from E away from the water, is dropped through the sea floor and through the
# island (the fall-through rescue puts it back on top, never swimming under the ground), and watches a Cinderfang
# chase it to the shore and stop where the water gets deep.
#   watertest.ps1 [-Port 7872]           headless
#   watertest.ps1 -Shots [-Out <folder>] rendered client: the swimmer from two angles by day and by night, swimming
#                                        and treading water, first person, and the "E Drink" prompt in the sea and at
#                                        its edge (one numbered picture each into -Out)
#   watertest.ps1 -Recheck <folder>      only check an earlier run's logs again
param([int]$Port = 7872, [switch]$Shots, [string]$Out = '', [string]$Recheck = '')
$env:UE_SKIP_UBT_SDK_SETUP = '1'   # Unreal otherwise waits for any build running on the machine
if ($Shots) {
	# One rendered run at a time on this machine (as uishots.ps1).
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
}
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = if ($Recheck) { $Recheck } else { Join-Path $env:TEMP ('wildreach-water-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs | Out-Null
if ($Shots) { New-Item -ItemType Directory -Force $Out | Out-Null }
$name = 'Swimmer'
$db = Join-Path $root 'Saved\ServerDB'
Remove-Item (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date

# The volcanic beach west of the hut site: ground ~3 m up, the drawn waterline 50 m west (water.surfaceOffset), 5 m deep
# 98 m out, a gentle 1:20 slope all the way (Tools/terrain heightmap).
$beach = '-153700 78300'; $sea = '-163500 78300'; $edge = '-158700 78300'; $near = '-160800 78300'
$labels = @()
function Shot([string]$label, [string]$prep) { if ($Shots) { $script:labels += $label; "$prep|wait 0.8|HighResShot 1600x900|wait 1.5" } }
function UIShot([string]$label, [string]$prep) { if ($Shots) { $script:labels += $label; "$prep|wait 0.8|shot showui|wait 1.5" } }
$cmds = @(
	'WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 3', 'WRHour 12',
	# Off the beach into the sea, walking: deep enough, it swims.
	"WRTeleport $beach", 'wait 5', 'wr.Water beach', 'WRCmd water stat beach',
	"WRWalkTo $sea", 'wait 30', 'wr.Water sea', 'WRCmd water stat sea',
	# E in the water: a gulp of Water each, one every 1.2 s.
	'WRCmd water thirst 30', 'wait 1', 'WRInteract', 'wait 1.5', 'WRInteract', 'wait 1', 'WRCmd water stat drank',
	(Shot 'day_behind' 'WRLook 0 -12|wait 1'),
	(Shot 'day_side' 'WRLook 90 -28|wait 1'),
	(Shot 'day_swimming' 'WRLook 0 -12|WRAnimDrive 90 4 1|wait 1.8'),
	(UIShot 'day_prompt_sea' 'wait 2.5|WRLook 0 -12|wait 1'),
	(Shot 'day_firstperson' 'WRView fp|WRLook 0 2|wait 1'),
	$(if ($Shots) { 'WRView tp|WRHour 0.5|wait 4' }),
	(Shot 'night_behind' 'WRLook 0 -12|wait 1'),
	(Shot 'night_side' 'WRLook 90 -28|wait 1'),
	(Shot 'night_swimming' 'WRLook 0 -12|WRAnimDrive 90 4 1|wait 1.8'),
	$(if ($Shots) { 'wait 2.5|WRHour 12|wait 4' }),
	# Back up the gentle bank: it swims to the shallows and walks out.
	"WRWalkTo $beach", 'wait 34', 'wr.Water out', 'WRCmd water stat out',
	# At the water's edge (feet a few centimetres over the surface, the water a metre and a half away): E drinks too.
	"WRTeleport $edge", 'wait 3', 'WRLook 180 -20', 'WRCmd water thirst 40', 'wait 1', 'wr.Water edge', 'WRInteract', 'wait 1.5', 'WRCmd water stat edgedrank',
	(UIShot 'day_prompt_edge' 'WRLook 180 -20|wait 1'),
	(Shot 'day_edge_kneel' 'WRCmd water thirst 50|wait 1|WRInteract|wait 0.6'),
	# Up the beach, away from the water: nothing to drink.
	"WRTeleport $beach", 'wait 3', 'wr.Water dry', 'WRCmd water stat drybefore', 'WRInteract', 'wait 1', 'WRCmd water stat dry',
	# Dropped 7 m through the sea floor (into the water under it): put back on the floor, it floats up and swims.
	"WRTeleport $sea", 'wait 4', 'WRCmd water sink 700', 'WRCmd water stat sunk', 'wait 5', 'WRCmd water stat afterseafloor',
	# Dropped 300 m through the beach (far under the island, below where the old sea plane was): back on top.
	"WRTeleport $beach", 'wait 4', 'WRCmd water sink 30000', 'WRCmd water stat deep', 'wait 4', 'WRCmd water stat afterisland',
	# Two Cinderfangs in the shallows see the swimmer 15 m out (1.6 m deep) and chase it: they wade in up to the belly
	# (36 cm for them) and stop there. A Pumicehog dropped in water over its head swims back to the shallows.
	"WRTeleport $near", 'wait 4', 'WRSpawnAt cinderfang -159300 78000', 'WRSpawnAt cinderfang -159300 77700', 'WRSpawnAt pumicehog -160400 78300', 'wait 20', 'WRCmd water creatures',
	(Shot 'day_creature_shore' 'WRLook 0 -8|wait 1'),
	'wait 1', 'WRProbe', 'wait 2'
)
$cmds = @(foreach ($c in $cmds) { if ($c) { $c } }) -join '|'
$render = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }

if (-not $Recheck) {
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=watertest -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=f -game $render -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$logs\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
# The run ends with WRProbe ("PROBE view" in the log; the command line, which the log starts with, doesn't say that).
$deadline = (Get-Date).AddSeconds($(if ($Shots) { 480 } else { 360 }))
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$logs\client.log" -Pattern 'LogWildreach: PROBE view' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $db 'world_watertest.json'), (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue
}

# ---- results
$client = @(Select-String -Path "$logs\client.log" -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: (Warning: |Display: |Error: )?', '' })
$server = @(Select-String -Path "$logs\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: (Warning: |Display: |Error: )?', '' })
$notices = @($client | Where-Object { $_ -like 'NOTICE *' } | ForEach-Object { $_.Substring(7) })
# "WATER client|server [tag] <name> at V(X=.., Y=.., Z=..) mode <m> inwater <0|1> depth <cm> surface <cm> water <n> candrink <0|1> speed <n>"
function State([string]$side, [string]$tag) {
	$lines = if ($side -eq 'client') { $client } else { $server }
	$l = @($lines | Where-Object { $_.StartsWith("WATER $side [$tag] ") }) | Select-Object -Last 1  # (-like reads [..] as a wildcard)
	if ($l -and $l -match 'Z=(-?[\d.]+)\) mode (\w+) inwater (\d) depth (-?[\d.]+) surface (-?[\d.]+) water (-?[\d.]+) candrink (\d)') {
		[pscustomobject]@{ z = [double]$Matches[1]; mode = $Matches[2]; inwater = $Matches[3] -eq '1'; depth = [double]$Matches[4]; surface = [double]$Matches[5]; water = [double]$Matches[6]; candrink = $Matches[7] -eq '1'; line = $l }
	} else { $null }
}
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Short($st) { if ($st) { "{0} z {1:N0} depth {2:N0} water {3:N0} candrink {4}" -f $st.mode, $st.z, $st.depth, $st.water, $st.candrink } else { 'no line' } }

$up = $server | Where-Object { $_ -like 'WATER up:*' } | Select-Object -First 1
Check 'the water volume is built on the server and the client' ($up -and ($client | Where-Object { $_ -like 'WATER up:*' })) "$up"
$b = State 'server' 'beach'; Check 'walks on the beach' ($b -and $b.mode -eq 'Walking' -and -not $b.inwater) (Short $b)
$sc = State 'client' 'sea'; $ss = State 'server' 'sea'
Check 'swims in the sea (client and server agree)' ($sc -and $ss -and $sc.mode -eq 'Swimming' -and $ss.mode -eq 'Swimming' -and $ss.inwater) ("client {0} | server {1}" -f (Short $sc), (Short $ss))
Check 'floats at the surface (not on the sea floor)' ($ss -and $ss.z -gt ($ss.surface - 130) -and $ss.z -lt $ss.surface -and $ss.depth -gt 300) (Short $ss)
$d = State 'server' 'drank'
Check 'E in the water drinks: Water 30 -> 70 in two gulps' (($notices -contains 'You drink sea water (Water 50)') -and ($notices -contains 'You drink sea water (Water 70)') -and $d -and [math]::Round($d.water) -eq 70) ((($notices | Where-Object { $_ -like 'You drink*' }) -join '; ') + " | " + (Short $d))
Check 'the prompt offers a drink in the water' ($sc -and $sc.candrink)
$o = State 'server' 'out'
Check 'swims back and walks up the beach' ($o -and $o.mode -eq 'Walking' -and $o.z -gt 100 -and @($client | Where-Object { $_ -like 'WALK arrived*' }).Count -ge 2) (Short $o)
$e = State 'client' 'edge'; $ed = State 'server' 'edgedrank'
Check 'at the water''s edge E drinks too (standing, not swimming)' ($e -and $e.candrink -and $e.mode -eq 'Walking' -and $ed -and [math]::Round($ed.water) -eq 60) ("edge {0} | after {1}" -f (Short $e), (Short $ed))
$dry = State 'client' 'dry'; $dryS = State 'server' 'dry'; $dryB = State 'server' 'drybefore'
Check 'away from the water there is nothing to drink' ($dry -and -not $dry.candrink -and ($server -contains 'CMD WRPlayerState_0: interact -> nothing to interact with' -or @($server | Where-Object { $_ -like 'CMD *: interact -> nothing to interact with' }).Count -ge 1) -and $dryS -and $dryB -and [math]::Abs($dryS.water - $dryB.water) -lt 1) (Short $dryS)
$sunk = State 'server' 'sunk'; $afterSea = State 'server' 'afterseafloor'
$seaRescue = @($server | Where-Object { $_ -like 'RESCUE WRCharacter*' })
Check 'dropped through the sea floor: back over it, swimming at the surface' ($sunk -and $afterSea -and $sunk.z -lt -400 -and $afterSea.mode -eq 'Swimming' -and $afterSea.depth -gt 300 -and $afterSea.z -gt ($afterSea.surface - 130)) ("sunk {0} | after {1} | {2}" -f (Short $sunk), (Short $afterSea), ($seaRescue -join '; '))
$deep = State 'server' 'deep'; $afterIsland = State 'server' 'afterisland'
Check 'dropped 300 m through the island: put back on top' ($deep -and $afterIsland -and $deep.z -lt -20000 -and $afterIsland.z -gt 0 -and $afterIsland.mode -ne 'Swimming' -and @($server | Where-Object { $_ -like 'RESCUE WRCharacter* under the ground*' }).Count -ge 1) ("deep {0} | after {1}" -f (Short $deep), (Short $afterIsland))
$fangs = @($server | Where-Object { $_ -like 'WATER creature *' })
$fangBad = @($fangs | Where-Object { $_ -match 'mode (\w+) inwater \d depth (-?[\d.]+) .* half (\d+)' -and ($Matches[1] -eq 'Swimming' -or [double]$Matches[2] -gt 0.8 * [double]$Matches[3] + 20) })
$fangWet = @($fangs | Where-Object { $_ -match 'depth (-?[\d.]+)' -and [double]$Matches[1] -gt 0 })
Check 'creatures stay out of deep water (Cinderfangs chasing stop where it''s too deep, a Pumicehog swims back)' ($fangs.Count -ge 3 -and $fangBad.Count -eq 0) ("{0} seen, {1} wading, {2} too deep: {3}" -f $fangs.Count, $fangWet.Count, $fangBad.Count, (($fangs | ForEach-Object { $_ -replace '^WATER creature (\S+) at V\(X=(-?\d+)\.\d+, Y=(-?\d+)\.\d+, Z=(-?\d+)\.\d+\) mode (\w+) .*depth (-?\d+).*', '$1 x $2 z $4 $5 depth $6' }) -join '; '))
$errors = @(Select-String -Path "$logs\server.log", "$logs\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
if ($Shots -and -not $Recheck) {
	$files = @(Get-ChildItem $shotDir -Filter '*Screenshot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
	for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
		$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
		Copy-Item $files[$i].FullName $dest -Force
		$dest
	}
	"{0} of {1} pictures" -f [Math]::Min($files.Count, $labels.Count), $labels.Count
	$lock.ReleaseMutex()
}
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
