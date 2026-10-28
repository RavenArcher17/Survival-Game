# Caves on L_Island: a headless player walks into every cave in Data/island_dressing.json on foot (from 30 m outside
# the mouth to the back of the chamber), then mines the nearest ore in there. Each cave must be reached (WALK arrived
# inside the chamber, not stuck in the mouth) and give ore; the walk's end point is logged for each.
#   cavetest.ps1 [-Port 7837] [-Path fire|frost] [-Only 3,4]   (-Only: cave numbers, in the file's order)
param([int]$Port = 7837, [string]$Path = 'fire', [int[]]$Only = @())
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-caves-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$name = 'Spelunker'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $root 'Saved\ServerDB\world_cavetest.json') -ErrorAction SilentlyContinue

$caves = @((Get-Content -Raw (Join-Path $root 'Data\island_dressing.json') | ConvertFrom-Json).caves)
$spawned = $caves.Count
if ($Only.Count -gt 0) { $caves = @(for ($k = 0; $k -lt $caves.Count; $k++) { if ($Only -contains $k) { $caves[$k] } }) }
# Cave local space (AWRCave): the chamber is centred on the cave's origin and the entrance opens toward local +X.
function At($cave, [double]$along) {
	$a = $cave.yaw * [math]::PI / 180
	$s = if ($cave.scale) { [double]$cave.scale } else { 1.0 }
	'{0:F0} {1:F0}' -f ($cave.x + [math]::Cos($a) * $along * $s), ($cave.y + [math]::Sin($a) * $along * $s)
}
$cmds = @('WRBotPause 1', 'wait 3')
$i = 0
foreach ($c in $caves) {
	# 22 m outside the mouth (the rock ends ~17 m out), then walk to 5 m behind the centre ("arrived" = within 4 m: deep in
	# the chamber); clear the burrowers first (within 30 m: the colony too) so the walk and the mining aren't a fight
	# (a Glacimaw kills a bot that only stands there); mine for a few swings; report where the walk ended.
	$cmds += "WRTeleport $(At $c 2200)", 'wait 6', 'WRClearCreatures', 'WRHeal', "WRWalkTo $(At $c -500)", 'wait 14',
		'WRClearCreatures', 'WRHeal', 'WRBotPause 0', 'WRGather node', 'wait 9', 'WRGather off', 'WRBotPause 1', 'wait 2', "WRSay cave$i done", 'wait 1'
	$i++
}
$cmds = $cmds -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=cavetest -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=$Path?WRName=$name -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(60 + 45 * $caves.Count)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (@(Select-String -Path "$out\server.log" -Pattern "cave$($caves.Count - 1) done" -SimpleMatch -ErrorAction SilentlyContinue).Count -ge 1) { Start-Sleep -Seconds 2; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_cavetest.json') -ErrorAction SilentlyContinue

# ---- results: split the logs per cave on the walk starts (client) and the "caveN done" lines (server)
$client = @(Select-String -Path "$out\client.log" -Pattern 'LogWildreach: (WALK .*)$' -ErrorAction SilentlyContinue | ForEach-Object { $_.Matches[0].Groups[1].Value })
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: (.*)$' -ErrorAction SilentlyContinue | ForEach-Object { $_.Matches[0].Groups[1].Value })
$walks = New-Object System.Collections.ArrayList
foreach ($l in $client) {
	if ($l -match '^WALK start ') { [void]$walks.Add('no walk result') }
	elseif ($walks.Count -gt 0 -and $l -match '^WALK (arrived|stuck) ') { $walks[$walks.Count - 1] = $l }
}
$gathers = @(); $got = @()
foreach ($l in $server) {
	if ($l -match '^GATHER \S+ got (\d+) (\S+) from ') { $got += "$($Matches[1]) $($Matches[2])" }
	if ($l -match 'cave\d+ done') { $gathers += ,@($got); $got = @() }
}
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
Check "the server spawned $spawned caves" (@($server | Where-Object { $_ -match "^DRESSING spawned $spawned caves" }).Count -eq 1)
for ($k = 0; $k -lt $caves.Count; $k++) {
	$cv = $caves[$k]
	$end = if ($k -lt $walks.Count) { $walks[$k] } else { 'no walk result' }
	Check ("walks into {0} cave {1} at {2:F0} {3:F0}" -f $cv.biome, $k, $cv.x, $cv.y) ($end -match '^WALK arrived ') $end
	$ore = if ($k -lt $gathers.Count) { @($gathers[$k] | Where-Object { $_ -match '(_ore|sulfur_crystal|lichen)$' }) } else { @() }
	Check "  and mines ore in it" ($ore.Count -ge 1) (($ore | Select-Object -First 3) -join ', ')
}
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
