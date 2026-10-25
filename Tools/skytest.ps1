# Day and night on L_Island (or the arena): the world clock drives the sun, a moon, the sky light and exposure on the
# client; the time carries over a server restart; the clock runs at clock.dayMinutes per day.
# A bot builds a small hut with a torch, a brazier, a claim and a Hearth (the lights a base has at night), then jumps
# the clock to noon, sunset and midnight and reads the client's sky (WRSky) at each.
#   skytest.ps1           headless, island
#   skytest.ps1 -Shots    rendered client: the hut and the island at noon, sunset and midnight, two angles each
#   skytest.ps1 -Arena    the same clock checks on L_CombatArena (no building)
# The map's world save is backed up and put back afterwards.
param([switch]$Shots, [switch]$Arena)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$db = Join-Path $root 'Saved\ServerDB'
$out = Join-Path $env:TEMP ("wildreach-sky-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$name = 'SkyBot'
$map = if ($Arena) { 'L_CombatArena' } else { 'L_Island' }
$worldSave = Join-Path $db "world_$map.json"
Remove-Item (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue
if (Test-Path $worldSave) { Copy-Item $worldSave "$out\world_backup.json" }
Remove-Item $worldSave -ErrorAction SilentlyContinue   # a fresh world: the clock starts at clock.startHour

# The survival test's hut site, 49 m south of the middle volcanic spawn; lights by its door, a claim and Hearth beside it.
$X = -132000; $Y = -56400; $Z = 4734; $T = $Z + 100
function Shot([string]$at, [string]$look) { if ($Shots) { "WRTeleport $at|wait 3|WRLook $look|wait 3|HighResShot 1600x900|wait 1" } else { $null } }
$build = if ($Arena) { @('WRTeleport 0 -600', 'wait 2') } else { @(
	"WRTeleport $($X + 700) $Y", 'wait 2', 'WRGive wood 300', 'WRGive stone 100', 'WRGive copper_ingot 5', 'WRGive fiber 10',
	"WRPlace wood_foundation $X $Y $Z 0",
	"WRPlace wood_wall $X $($Y + 200) $T 0", "WRPlace wood_wall $X $($Y - 200) $T 0", "WRPlace wood_wall $($X - 200) $Y $T 90",
	"WRPlace wood_door $($X + 200) $Y $T 90",
	"WRPlace wood_roof $X $Y $($T + 300) 0", "WRPlace wood_gable $X $($Y + 200) $($T + 300) 0", "WRPlace wood_gable $X $($Y - 200) $($T + 300) 0", "WRPlace wood_upper_wall $($X + 200) $Y $($T + 300) 90",
	"WRPlace torch $($X + 330) $($Y + 150) 4803 0", "WRPlace brazier $($X + 330) $($Y - 150) 4806 0",
	"WRPlace claim_totem $($X + 900) $($Y + 400) 4847 0", "WRPlace hearth $($X + 700) $($Y - 400) 4838 0", 'wait 1') }
# Where to look from, per time of day: the hut from the east and from the north-east (island), or the arena.
$views = if ($Arena) { @(@('0 -1500', '90 -5'), @('1500 1500', '225 15')) } else { @(@("$($X + 1300) $Y", '180 -6'), @("$($X + 1100) $($Y + 900)", '219 10')) }
function Views { if ($Shots) { (@($views | ForEach-Object { Shot $_[0] $_[1] }) -join '|') } else { $null } }

$cmds = @(
	'WRBotPause 1', 'wait 2', 'WRSky'
) + $build + @(
	'WRHour 12', 'wait 3', 'WRSky', (Views),
	'WRHour 19.7', 'wait 3', 'WRSky', (Views),
	'WRHour 0', 'wait 3', 'WRSky', (Views),
	$(if ($Shots -and -not $Arena) { "WRTeleport $($X + 1100) $($Y + 900)|wait 3|WRLook 90 30|wait 3|HighResShot 1600x900|wait 1" }),  # the moon over the south
	# The clock runs: 30 s is 0.2 h at 60 minutes a day.
	'WRHour 21.5', 'wait 2', 'WRSky', 'wait 30', 'WRSky',
	# ...and the world save keeps it (autosave every 60 s): wait for one, then the test restarts the server.
	'wait 40', 'WRSay skytest-done', 'wait 2'
)
$cmds = @(foreach ($c in $cmds) { if ($c) { $c } }) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/$map -server -nullrhi -unattended -nosound -port=7837 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds $(if ($Arena) { 20 } else { 30 })
$render = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7837?Path=fire?WRName=$name -game $render -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
$deadline = (Get-Date).AddSeconds($(if ($Shots) { 420 } else { 300 }))
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$out\server.log" -Pattern 'skytest-done' -Quiet -ErrorAction SilentlyContinue) { break }
}
if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
$deadline = (Get-Date).AddSeconds(70)
while ((Get-Date) -lt $deadline -and @(Select-String -Path "$out\server.log" -Pattern 'WORLD saved').Count -lt 2) { Start-Sleep -Seconds 3 }
if (-not $s.HasExited) { Stop-Process -Id $s.Id -Force }
$s2 = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/$map -server -nullrhi -unattended -nosound -port=7837 -abslog=`"$out\server2.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	if ((Test-Path "$out\server2.log") -and (Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded' -Quiet)) { Start-Sleep -Seconds 2; break }
}
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }
Start-Sleep -Seconds 2
if (Test-Path "$out\world_backup.json") { Copy-Item "$out\world_backup.json" $worldSave -Force } else { Remove-Item $worldSave -ErrorAction SilentlyContinue }

# ---- results
$skies = @(Select-String -Path "$out\client.log" -Pattern 'SKY day (\d+) hour ([-\d.]+) daylight ([-\d.]+) sun ([-\d.]+) deg ([-\d.]+) lux, moon ([-\d.]+) deg ([-\d.]+) lux, min EV100 ([-\d.]+)' | ForEach-Object {
	$m = $_.Matches[0].Groups; [pscustomobject]@{ day = [int]$m[1].Value; hour = [double]$m[2].Value; daylight = [double]$m[3].Value; sun = [double]$m[4].Value; sunLux = [double]$m[5].Value; moon = [double]$m[6].Value; moonLux = [double]$m[7].Value; ev = [double]$m[8].Value } })
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Near([double]$hour) { $skies | Where-Object { [math]::Abs($_.hour - $hour) -lt 0.08 } | Select-Object -First 1 }
function Show($s) { if ($s) { "hour {0:N2}: sun {1:N1} deg {2:N2} lux, moon {3:N1} deg {4:N2} lux, min EV100 {5:N1}" -f $s.hour, $s.sun, $s.sunLux, $s.moon, $s.moonLux, $s.ev } else { 'no reading' } }
$start = $skies | Select-Object -First 1
Check 'a new world starts at clock.startHour (8:00, day 1; the bot joins a few game minutes in)' ($start -and $start.day -eq 1 -and $start.hour -ge 8 -and $start.hour -lt 8.6) (Show $start)
$noon = Near 12; $dusk = Near 19.7; $night = Near 0
Check 'noon: the sun is high and bright, no moon' ($noon -and $noon.sun -gt 55 -and $noon.sunLux -gt 5 -and $noon.moonLux -eq 0 -and $noon.ev -lt -5) (Show $noon)
Check 'sunset: the sun is low in the west and still up' ($dusk -and $dusk.sun -gt 0 -and $dusk.sun -lt 12 -and $dusk.sunLux -gt 0) (Show $dusk)
Check 'midnight: the sun is down and off, the moon is up and lights the island' ($night -and $night.sun -lt -40 -and $night.sunLux -eq 0 -and $night.moon -gt 40 -and $night.moonLux -gt 0.2) (Show $night)
Check 'midnight: exposure keeps the night dark (min EV100 at sky.nightMinEV100)' ($night -and $night.ev -gt 0) (Show $night)
$a = Near 21.5; $later = $skies | Where-Object { $_.hour -gt 21.6 -and $_.hour -lt 22 } | Select-Object -First 1
Check 'the clock runs at an hour per 2.5 minutes (30 s = 0.2 h)' ($a -and $later -and [math]::Abs(($later.hour - $a.hour) - 0.2) -lt 0.05) $(if ($a -and $later) { "{0:N2} -> {1:N2}" -f $a.hour, $later.hour } else { 'missing' })
$loaded = @(Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded .* day (\d+) ([\d.]+)' -ErrorAction SilentlyContinue | ForEach-Object { [pscustomobject]@{ day = [int]$_.Matches[0].Groups[1].Value; hour = [double]$_.Matches[0].Groups[2].Value } })
Check 'a server restart keeps the day and time (not back to 8:00)' ($loaded.Count -eq 1 -and $loaded[0].hour -gt 21.5 -and $loaded[0].hour -lt 23 -and $loaded[0].day -ge 2) $(if ($loaded.Count) { "day {0} {1:N2}" -f $loaded[0].day, $loaded[0].hour } else { 'not loaded' })
if (-not $Arena) {
	$notices = @(Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','').Trim() })
	Check 'built the hut, a torch, a brazier, a claim and a Hearth' ((@('Built Torch', 'Built Brazier', 'Built Land claim totem', 'Built Hearth', 'Built Wood roof (sloped)', 'Built Wood upper wall (under a roof''s high side)') | Where-Object { $notices -notcontains $_ }).Count -eq 0) (($notices | Where-Object { $_ -notmatch '^Built' -and $_ -notmatch '^ok' }) -join '; ')
}
Check 'the client found the map''s sun' (Select-String -Path "$out\client.log" -Pattern 'SKY sun found' -Quiet)
$errors = @(Select-String -Path "$out\server.log", "$out\server2.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
# This run's own screenshots (other sessions may be saving theirs into the same folder).
if ($Shots) { Select-String -Path "$out\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value } }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
