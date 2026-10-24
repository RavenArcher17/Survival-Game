# Boss test on the arena with two players. Hero fights every boss in turn (dev damage commands
# stand in for a long fight); Helper joins the Vulkarn fight and deals ~3%, so Vulkarn scales to
# two fighters and Helper gets no loot (10% rule). Checks phases, attacks landing, floods/anvils,
# wing grounding, the Warden's shell, Ignessa's wisps, regen, the Pale Hunter's blizzard, and loot.
param([int]$Seconds = 200)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-boss-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Hero.json'), (Join-Path $db 'players\Helper.json') -ErrorAction SilentlyContinue
$tune = '-wrtune=bosses.damageScale:0.05,bosses.partChance:1'

$fight = { param($id, $extra) @("WRSpawn $id 1", 'wait 7', 'WRHeal', 'WRBossDmgPct 30 none', 'wait 3', 'WRBossDmgPct 25 none', 'wait 6') + $extra + @('WRHeal', 'WRBossDmg 9999999 none', 'wait 8') }
$hero = @('WRBotPause 1', 'WRTeleport 0 4200', 'wait 2', 'WRSpawn vulkarn 1', 'wait 14', 'WRHeal', 'WRBossDmgPct 30 frost', 'wait 3', 'WRBossDmgPct 25 none', 'wait 8',
	'WRHeal', 'WRArtifacts 1', 'WRBossDmgPct 30 none', 'wait 6', 'WRHeal', 'WRBossDmg 9999999 none', 'wait 8', 'WRArtifacts 0')
$hero += & $fight 'skaldrith' @('WRBossDmg 10 fire', 'WRBossDmg 10 fire', 'WRBossDmg 10 fire', 'wait 3')
$hero += & $fight 'obsidian_warden' @('WRBossDmgPct 30 frost', 'WRBossDmgPct 30 frost', 'wait 2')
$hero += & $fight 'ignessa' @('wait 6')
$hero += & $fight 'grimjaw' @('wait 4', 'WRBossDmgPct 0 none')
$hero += & $fight 'pale_hunter' @('wait 6', 'WRBossDmg 10 fire')
$helper = @('WRBotPause 1', 'WRTeleport 300 4200', 'wait 12', 'WRBossDmgPct 3 none')

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7812 $tune -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7812?Path=fire?WRName=Hero -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($hero -join '|')`" -abslog=`"$out\hero.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7812?Path=frost?WRName=Helper -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($helper -join '|')`" -abslog=`"$out\helper.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds $Seconds
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\server.log" -Pattern 'BOSS ' | Where-Object { $_.Line -notmatch 'winds up' } | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') }
"attacks wound up: $((Select-String -Path "$out\server.log" -Pattern 'winds up' | Measure-Object).Count)"
"--- notices ---"
Select-String -Path "$out\hero.log", "$out\helper.log" -Pattern 'NOTICE .*(defeated|Loot)' | ForEach-Object { "$($_.Filename): " + ($_.Line -replace '^.*NOTICE ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'Error:|Ensure condition failed|Accessed None' | Measure-Object).Count)"
"log: $out"
