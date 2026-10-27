# Building test on the arena: claim, foundations, walls, floor, span limit, code-locked door, chest,
# crop; a second player is blocked by the claim and the lock; raid damage + collapse; then a server
# restart reloads everything that was built. Its own world save (-wrworld=buildtest), fresh each run.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-build-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$world = Join-Path $root 'Saved\ServerDB\world_buildtest.json'
Remove-Item $world -ErrorAction SilentlyContinue
Remove-Item (Join-Path $root 'Saved\ServerDB\players\Builder.json'), (Join-Path $root 'Saved\ServerDB\players\Raider.json') -ErrorAction SilentlyContinue

$builder = @(
	'WRBotPause 1', 'WRTeleport 2400 3000', 'wait 1',
	'WRGive wood 500', 'WRGive stone 300', 'WRGive copper_ingot 10', 'WRGive hide 10', 'WRGive fiber 20', 'WRGive seeds 2',
	'WRPlace claim_totem 2000 3000 0 0',
	'WRPlace wood_foundation 2400 3600 0 0', 'WRPlace wood_foundation 2800 3600 0 0',
	'WRPlace wood_wall 2400 3400 100 0', 'WRPlace wood_wall 2200 3600 100 90',
	'WRPlace reinforced_door 2800 3400 100 0',
	'WRPlace wood_floor 2400 3600 400 0',
	'WRTeleport 2600 4400', 'wait 1',
	'WRPlace wood_floor 2400 4000 400 0', 'WRPlace wood_floor 2400 4400 400 0', 'WRPlace wood_floor 2400 4800 400 0', 'WRPlace wood_floor 2400 5200 400 0',
	'WRPlace wood_floor 3600 4400 400 0',
	'WRTeleport 2400 3000', 'wait 1',
	'WRPlace storage_chest 2500 3700 100 0',
	'WRPlace crop_plot 3600 2400 0 0',
	'WRTeleport 2800 3150', 'wait 1', 'WRSetCode 1234', 'wait 1', 'WRInteract',
	'WRTeleport 3600 2120', 'wait 1', 'WRPlace crop_plot 3600 2400 0 0', 'WRInteract'
) -join '|'
$raider = @(
	'WRBotPause 1', 'wait 14', 'WRTeleport 3200 3000', 'wait 1', 'WRGive wood 50',
	'WRPlace wood_foundation 3200 3600 0 0',
	'WRTeleport 2800 3150', 'wait 1', 'WRInteract', 'WRCode 0000', 'WRCode 1234', 'WRInteract',
	'WRTeleport 2400 3200', 'wait 1', 'WRHitPiece 20', 'WRHitPiece 20', 'wait 1',
	'WRTeleport 2400 3700', 'wait 1', 'WRHitPiece 60', 'wait 2', 'WRSay buildtest-done'
) -join '|'

function Start-Server($log) { Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7815 -wrworld=buildtest -abslog=`"$log`"" -PassThru -WindowStyle Hidden }
function Start-Bot($name, $cmds, $log) { Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7815?Path=fire?WRName=$name -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$log`"" -PassThru -WindowStyle Hidden }
function Lines($file, $pattern) { if (Test-Path $file) { @(Select-String -Path $file -Pattern $pattern | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ', '').Trim() }) } else { @() } }

$s = Start-Server "$out\server1.log"; Start-Sleep -Seconds 20
$b = Start-Bot 'Builder' $builder "$out\builder.log"; Start-Sleep -Seconds 2
$r = Start-Bot 'Raider' $raider "$out\raider.log"
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline -and (Lines "$out\server1.log" 'CHAT .*buildtest-done').Count -eq 0) { Start-Sleep -Seconds 2 }
foreach ($p in $b, $r) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
# The world autosaves every 60 s: wait for a save after the last piece came down.
$lastBuild = 0; $deadline = (Get-Date).AddSeconds(70)
do {
	Start-Sleep -Seconds 2
	$log = @(if (Test-Path "$out\server1.log") { Get-Content "$out\server1.log" })
	for ($i = 0; $i -lt $log.Count; $i++) { if ($log[$i] -match 'LogWildreach: BUILD ') { $lastBuild = $i } }
	$savedAfter = @($log | Select-Object -Skip ($lastBuild + 1) | Where-Object { $_ -match 'LogWildreach: WORLD saved' }).Count -gt 0
} while ((Get-Date) -lt $deadline -and -not $savedAfter)
if (-not $s.HasExited) { Stop-Process -Id $s.Id -Force }
$saved = if (Test-Path $world) { try { Get-Content $world -Raw | ConvertFrom-Json } catch { $null } }

# Restart: does the world come back?
$s2 = Start-Server "$out\server2.log"
$deadline = (Get-Date).AddSeconds(40)
while ((Get-Date) -lt $deadline -and (Lines "$out\server2.log" 'WORLD loaded').Count -eq 0) { Start-Sleep -Seconds 2 }
Start-Sleep -Seconds 3   # anything that would collapse on load does it now
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$bn = @(Lines "$out\builder.log" 'NOTICE ' | ForEach-Object { $_ -replace '^.*NOTICE ', '' })
$rn = @(Lines "$out\raider.log" 'NOTICE ' | ForEach-Object { $_ -replace '^.*NOTICE ', '' })
function Built([string]$what) { @($bn | Where-Object { $_ -eq "Built $what" }).Count }
Check 'the claim totem goes down' ((Built 'Land claim totem') -eq 1)
Check 'foundations, walls and a code-lock door go up' ((Built 'Wood foundation') -eq 2 -and (Built 'Wood wall') -eq 2 -and (Built 'Reinforced door (code lock)') -eq 1)
Check 'floors go up within the span from support' ((Built 'Wood floor / ceiling') -eq 3) "$(Built 'Wood floor / ceiling') floors"
Check 'floors past the span are refused' (@($bn | Where-Object { $_ -match '^no support \(wood spans \d+\)' }).Count -eq 3) (($bn | Where-Object { $_ -match '^no support' } | Select-Object -First 1))
Check 'a chest goes down' ((Built 'Storage chest') -eq 1)
Check 'placing from too far away is refused' (@($bn | Where-Object { $_ -eq 'too far away' }).Count -eq 1)
Check 'a crop plot goes down and is planted' ((Built 'Crop plot') -eq 1 -and @($bn | Where-Object { $_ -match '^Planted\. Ready in' }).Count -eq 1) ($bn | Where-Object { $_ -match '^Planted' } | Select-Object -First 1)
Check 'the owner sets a code and opens the door' (($bn -contains 'Code set; door locked') -and ($bn -contains 'Opened'))
Check 'another player can''t build in the claim' ($rn -contains "Builder's land claim")
Check 'the locked door keeps them out until the right code' (@($rn | Where-Object { $_ -like 'Locked.*' }).Count -ge 1 -and ($rn -contains 'Wrong code') -and ($rn -contains 'Code accepted') -and (@($rn | Where-Object { $_ -in 'Opened', 'Closed' }).Count -ge 1)) (($rn | Select-Object -Skip 1 -First 4) -join '; ')
$server = Lines "$out\server1.log" 'LogWildreach: '
Check 'raid hits wear a wall down, then destroy it' (@($rn | Where-Object { $_ -match '^wood_wall hp \d+' }).Count -ge 1 -and @($server | Where-Object { $_ -match '^BUILD \S+ \(wood_wall\) destroyed by ' }).Count -eq 1) (($rn | Where-Object { $_ -match 'hp|destroyed' }) -join '; ')
$collapsed = @($server | Where-Object { $_ -match '^BUILD \S+ collapsed \(no support\)' }).Count
Check 'floors with nothing under them collapse' (@($server | Where-Object { $_ -match '^BUILD \S+ \(wood_floor\) destroyed by ' }).Count -eq 1 -and $collapsed -eq 2) "$collapsed collapsed"
# Built: claim, 2 foundations, 2 walls, door, 3 floors, chest, crop = 11; less a wall, a floor and 2 collapsed floors = 7.
$last = Lines "$out\server1.log" 'WORLD saved' | Select-Object -Last 1
Check 'the world saves what still stands' ($last -eq 'WORLD saved 7 pieces') $last
$door = if ($saved) { $saved.pieces | Where-Object { $_.piece -eq 'reinforced_door' } | Select-Object -First 1 }
$crop = if ($saved) { $saved.pieces | Where-Object { $_.piece -eq 'crop_plot' } | Select-Object -First 1 }
Check 'the save keeps the door''s code and the planted crop' ($door -and $door.code -eq '1234' -and $door.locked -and $crop -and $crop.planted -and $crop.cropSecondsLeft -gt 0) $(if ($saved) { "door code '$($door.code)' locked $($door.locked); crop planted $($crop.planted), $([int]$crop.cropSecondsLeft) s left" } else { 'no world save' })
$loaded = Lines "$out\server2.log" 'WORLD loaded' | Select-Object -First 1
Check 'a restarted server loads every piece, and nothing collapses' ($loaded -match '^WORLD loaded 7 pieces' -and (Lines "$out\server2.log" 'collapsed').Count -eq 0) $loaded
$logs = @("$out\server1.log", "$out\server2.log", "$out\builder.log", "$out\raider.log" | Where-Object { Test-Path $_ })
$errors = @(Select-String -Path $logs -Pattern 'Error:|Ensure condition failed|Accessed None')
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($x in $results) { "{0,-5} {1}{2}" -f $(if ($x.ok) { 'PASS' } else { 'FAIL' }), $x.what, $(if ($x.detail) { "  ($($x.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
