# Island wildlife on L_Island: the server places every site in Data/island_spawns.json, a pack comes out when a player
# arrives (on the ground, all of it, none dying on the way in), goes quiet once nobody's near and comes back on a return.
# The bot visits an Emberhorn herd on the volcanic side, a Frostantler herd on the tundra side (the Emberhorns go
# quiet meanwhile: wildlife.despawnSeconds is shortened to 10 for the test), then the Emberhorns again.
#   wildlifetest.ps1          headless
#   wildlifetest.ps1 -Shots   rendered client: a screenshot of each herd
param([switch]$Shots)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-wildlife-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$name = 'Ranger'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue

$data = Get-Content -Raw (Join-Path $root 'Data\island_spawns.json') | ConvertFrom-Json
$fire = $data.sites | Where-Object { $_.creature -eq 'emberhorn' } | Select-Object -First 1
$frost = $data.sites | Where-Object { $_.creature -eq 'frostantler' } | Select-Object -First 1
$total = ($data.sites | Measure-Object -Property count -Sum).Sum
# Stand 20 m south of a site, looking north at it.
function Visit($site) { "WRTeleport $([int]$site.x) $([int]$site.y + 2000)|wait 8|WRLook 270 -10|WRWildlife|wait 1" }
function Shot { if ($Shots) { 'wait 2|HighResShot 1600x900|wait 1' } else { $null } }

$cmds = @(
	'WRBotPause 1', 'wait 3', 'WRWildlife', 'wait 1',          # at the camp
	(Visit $fire), (Shot),
	(Visit $frost), (Shot),
	'wait 16', 'WRWildlife', 'wait 1',                          # the Emberhorns have gone quiet by now
	(Visit $fire),                                              # ...and come back out
	'wait 3'
)
$cmds = @(foreach ($c in $cmds) { if ($c) { $c } }) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7835 -wrworld=wildlife -wrtune=wildlife.despawnSeconds:10 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$render = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7835?Path=fire?WRName=$name -game $render -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
$deadline = (Get-Date).AddSeconds(150)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (@(Select-String -Path "$out\server.log" -Pattern ': wildlife -> ' -ErrorAction SilentlyContinue).Count -ge 5) { Start-Sleep -Seconds 4; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_wildlife.json') -ErrorAction SilentlyContinue

# ---- results
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') })
$reports = @($server | Where-Object { $_ -match ': wildlife -> (.*)$' } | ForEach-Object { ($_ -replace '^.*: wildlife -> ','') })
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Near([string]$report, [string]$id) { if ($report -match "within 300 m: [^;]*\b$id (\d+)") { [int]$Matches[1] } else { 0 } }
function Active([string]$report) { if ($report -match 'active (\d+), out (\d+)') { [int]$Matches[1] } else { -1 } }
$at = { param($site) '{0:N0} {1:N0}' -f $site.x, $site.y }
$fireOut = @($server | Where-Object { $_ -match "^WILDLIFE emberhorn out at V\(X=$([int]$fire.x)\." })

Check "the server placed every site ($($data.sites.Count) sites, $total creatures)" (@($server | Where-Object { $_ -match "^WILDLIFE $($data.sites.Count) sites, $total creatures" }).Count -eq 1)
Check 'five wildlife reports' ($reports.Count -ge 5) ("{0}" -f $reports.Count)
if ($reports.Count -ge 5) {
	Check 'nothing out far from the player at the start (only sites near the camp)' ((Active $reports[0]) -ge 0 -and (Active $reports[0]) -le 6) $reports[0]
	Check "the Emberhorn herd came out around the player ($(& $at $fire))" ((Near $reports[1] 'emberhorn') -ge $fire.count) $reports[1]
	Check "the Frostantler herd came out on the tundra side ($(& $at $frost))" ((Near $reports[2] 'frostantler') -ge $frost.count) $reports[2]
	Check 'no Emberhorns over on the tundra' ((Near $reports[2] 'emberhorn') -eq 0) $reports[2]
	Check 'packs come out only near players (most sites stay quiet)' ((Active $reports[2]) -ge 1 -and (Active $reports[2]) -le 12) $reports[2]
	Check 'the Emberhorns went quiet with nobody near' (@($server | Where-Object { $_ -match '^WILDLIFE emberhorn at .* went quiet \((\d+) gone\)' -and [int]$Matches[1] -ge 1 }).Count -ge 1)
	Check 'and came back out when the player returned' (((Near $reports[4] 'emberhorn') -ge $fire.count) -and $fireOut.Count -ge 2) ("{0} | out events {1}" -f $reports[4], $fireOut.Count)
}
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None')
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
if ($Shots) { Select-String -Path "$out\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value } }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
