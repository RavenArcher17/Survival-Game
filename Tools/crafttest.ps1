# End-to-end crafting test on the arena: gather-free (materials given), then smelt, craft, infuse,
# equip, socket, upgrade to the Hearth cap, feed the Hearth a core, craft a tier-2 recipe.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-craft-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\players\Crafter.json') -ErrorAction SilentlyContinue

$cmds = @(
	'WRBotPause 1',
	'WRGive copper_ore 24', 'WRGive wood 10', 'WRGive hide 10', 'WRGive fiber 10', 'WRGive berries 10',
	'WRGive ember_essence 1', 'WRGive upgrade_shard 10', 'WRGive gem_ruby 1', 'WRGive tin_ore 4', 'WRGive core_vulkarn 1',
	'WRGoTo forge', 'wait 2',
	'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'wait 1',
	'WRCraft copper_sword fire', 'wait 1', 'WREquip copper_sword', 'wait 1',
	'WRUpgrade weapon', 'WRUpgrade weapon', 'WRUpgrade weapon', 'WRRepair weapon', 'wait 1',
	'WRCraft bronze_ingot none', 'wait 1',
	'WRGoTo armorer', 'wait 2', 'WRCraft hide_armor none', 'wait 1', 'WREquip hide_armor',
	'WRGoTo alchemy_table', 'wait 2', 'WRCraft minor_healing_potion none',
	'WRGoTo hearth', 'wait 2', 'WRInteract', 'wait 1',
	'WRGoTo forge', 'wait 2', 'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'wait 1', 'WRCraft bronze_ingot none', 'WRUpgrade weapon', 'wait 1',
	'WREquip gem_ruby', 'wait 2'
) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7810 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7810?Path=fire?WRName=Crafter -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 45
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
