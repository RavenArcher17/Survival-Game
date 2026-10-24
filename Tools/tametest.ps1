# End-to-end taming test on the arena: offer food (Ashstrider), ride it, knockout + feed (Cinderfang),
# steal and hatch a Kilnmite egg, give orders, then restart the server and check tames were saved.
# Taming times are shortened with -wrtune so the test runs in about two minutes.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-tame-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Tamer.json'), (Join-Path $db 'world_L_CombatArena.json') -ErrorAction SilentlyContinue
$tune = '-wrtune=taming.easySeconds:4,taming.mediumSeconds:6,taming.hardSeconds:8,taming.tameLimit:5'

$cmds = @(
	'WRBotPause 1', 'WRGive raw_meat 5', 'WRGive berries 5',
	'WRTeleport 1500 -2200', 'wait 1', 'WRSpawn ashstrider 1', 'wait 1', 'WRGoTo ashstrider', 'wait 1', 'WRInteract', 'wait 7', 'WRTameStat',
	'WRTame ride', 'wait 2', 'WRTameStat', 'WRDismount', 'wait 1', 'WRTameStat', 'wait 1', 'WRTame stay', 'wait 1',
	'WRTeleport -1500 -5000', 'wait 1', 'WRSpawn cinderfang 1', 'wait 1', 'WRGoTo cinderfang', 'WRTorpor 1000', 'wait 1', 'WRTameStat', 'WRInteract', 'wait 9', 'WRTameStat',
	'WRGoTo nest kilnmite', 'wait 1', 'WRInteract', 'wait 1', 'WREquip egg_kilnmite', 'wait 2', 'WRTameStat', 'WRTeleport 900 -3300', 'wait 5', 'WRTameStat', 'WRTame aggressive'
) -join '|'

function Start-Server($log) {
	Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7811 $tune -abslog=`"$log`"" -PassThru -WindowStyle Hidden
}
$s = Start-Server "$out\server.log"
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7811?Path=fire?WRName=Tamer -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 75   # past the 60 s world autosave
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
Select-String -Path "$out\server.log" -Pattern 'TAME |WORLD saved' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"

# Restart: the tames should come back.
$s2 = Start-Server "$out\server2.log"
Start-Sleep -Seconds 20
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }
Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
