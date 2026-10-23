# Building test on the arena: claim, foundations, walls, floor, span limit, code-locked door, chest,
# crop; a second player is blocked by the claim and the lock; raid damage + collapse; then a server
# restart reloads everything that was built.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-build-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path $root 'Saved\ServerDB\world_L_CombatArena.json') -ErrorAction SilentlyContinue
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
	'WRTeleport 3600 2250', 'wait 1', 'WRInteract'
) -join '|'
$raider = @(
	'WRBotPause 1', 'wait 14', 'WRTeleport 3200 3000', 'wait 1', 'WRGive wood 50',
	'WRPlace wood_foundation 3200 3600 0 0',
	'WRTeleport 2800 3150', 'wait 1', 'WRInteract', 'WRCode 0000', 'WRCode 1234', 'WRInteract',
	'WRTeleport 2400 3200', 'wait 1', 'WRHitPiece 20', 'WRHitPiece 20', 'wait 1',
	'WRTeleport 2400 3700', 'wait 1', 'WRHitPiece 60', 'wait 2'
) -join '|'

function Start-Server($log) { Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7815 -abslog=`"$log`"" -PassThru -WindowStyle Hidden }
function Start-Bot($name, $cmds, $log) { Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7815?Path=fire?WRName=$name -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$log`"" -PassThru -WindowStyle Hidden }

$s = Start-Server "$out\server1.log"; Start-Sleep -Seconds 20
$b = Start-Bot 'Builder' $builder "$out\builder.log"; Start-Sleep -Seconds 2
$r = Start-Bot 'Raider' $raider "$out\raider.log"
Start-Sleep -Seconds 45
foreach ($p in $b, $r) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 65   # let the 60 s world autosave run
foreach ($p in $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

"=== Builder"; Select-String -Path "$out\builder.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
"=== Raider"; Select-String -Path "$out\raider.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
"=== Server"; Select-String -Path "$out\server1.log" -Pattern 'BUILD .*(destroyed|collapsed)|WORLD saved' | Select-Object -Last 6 | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }

# Restart: does the world come back?
$s2 = Start-Server "$out\server2.log"; Start-Sleep -Seconds 25
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }
"=== After restart"; Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded|collapsed' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"server errors: $((Select-String -Path "$out\server1.log", "$out\server2.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
