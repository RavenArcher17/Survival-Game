# End-to-end crafting test on the arena: gather-free (materials given), then smelt, craft, infuse,
# equip, socket, upgrade to the Hearth cap, feed the Hearth a core, craft a tier-2 recipe; then every ranged weapon
# and its ammo, and a bow worn by shooting repaired at the workbench.
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
	'WREquip gem_ruby', 'wait 2',
	# Ranged weapons and ammo: spears, bolas, arrows and tranq arrows by hand, bow and blowgun at the workbench,
	# narcotic and darts at the alchemy table, bolts and (Hearth tier 2) the crossbow at the forge.
	'WRGive wood 40', 'WRGive stone 12', 'WRGive fiber 40', 'WRGive hide 4', 'WRGive berries 6', 'WRGive raw_meat 2',
	'WRGive copper_ore 14', 'WRGive tin_ore 3',
	'WRCraft throwing_spear none', 'WRCraft throwing_spear none', 'WRCraft bola none', 'WRCraft stone_arrow none', 'wait 1',
	'WRGoTo workbench', 'wait 2', 'WRCraft hunting_bow none', 'WRCraft blowgun none', 'wait 1',
	'WRGoTo alchemy_table', 'wait 2', 'WRCraft narcotic none', 'WRCraft narcotic none', 'WRCraft torpor_dart none', 'wait 1',
	'WRCraft tranq_arrow none', 'WRCraft tranq_arrow none', 'wait 1',
	'WRGoTo forge', 'wait 2',
	'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'WRCraft copper_ingot none',
	'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'WRCraft copper_ingot none', 'wait 1',
	'WRCraft bronze_ingot none', 'WRCraft bronze_ingot none', 'WRCraft bronze_ingot none', 'wait 1',
	'WRCraft crossbow none', 'WRCraft crossbow_bolt none', 'wait 1', 'WRCraft tranq_bolt none', 'wait 1',
	# The bow wears with each shot and is mended at the workbench, not the forge.
	'WREquip hunting_bow', 'wait 1', 'WRRanged 1', 'wait 1.5', 'WRRepair ranged', 'wait 0.5',
	'WRFire', 'wait 1.3', 'WRFire', 'wait 1.3', 'WRFire', 'wait 1.3', 'WRRepair ranged', 'wait 0.5',
	'WRGoTo workbench', 'wait 2', 'WRRepair ranged', 'wait 1'
) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7810 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7810?Path=fire?WRName=Crafter -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 80
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','') }
Select-String -Path "$out\server.log" -Pattern 'RANGED .* fires' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
