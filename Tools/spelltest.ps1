# Tree spells + the Paths menu's learn command, on the arena's training dummies.
# Pyro (fire) learns a node via the menu command, auto-spends into Fire wall (slot 3) and Lava pool (slot 4);
# Cryo (frost) gets Ice lance (slot 3) and Frost nova (slot 4). Each casts at its own dummy.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-spell-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Pyro.json'), (Join-Path $db 'players\Cryo.json') -ErrorAction SilentlyContinue

$pyro = @('WRBotPause 1', 'WRGivePoints fire 70', 'WRLearn fire spreading_flames', 'WRAutoSpend fire 60', 'wait 2',
	'WRTeleport -400 -500', 'wait 2', 'WRLook 0 -20', 'WRCast 3', 'wait 3', 'WRCast 4', 'wait 5') -join '|'
$cryo = @('WRBotPause 1', 'WRGivePoints frost 70', 'WRAutoSpend frost 60', 'wait 2',
	'WRTeleport -400 500', 'wait 2', 'WRLook 0 0', 'WRCast 3', 'wait 2', 'WRCast 4', 'wait 3') -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7823 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c1 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7823?Path=fire?WRName=Pyro -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$pyro`" -abslog=`"$out\pyro.log`"" -PassThru -WindowStyle Hidden
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7823?Path=frost?WRName=Cryo -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cryo`" -abslog=`"$out\cryo.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 40
foreach ($p in $c1, $c2, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\pyro.log", "$out\cryo.log" -Pattern 'NOTICE (Learned|path|needs|not enough)' | ForEach-Object { "$($_.Filename): " + ($_.Line -replace '^.*NOTICE ','') }
$combat = Select-String -Path "$out\server.log" -Pattern 'COMBAT WRCharacter_\d+ -> WRTrainingDummy'
"fire hits on dummies:  $(($combat | Where-Object { $_.Line -match 'Element.Fire' } | Measure-Object).Count)"
"frost hits on dummies: $(($combat | Where-Object { $_.Line -match 'Element.Frost' } | Measure-Object).Count)"
$combat | Where-Object { $_.Line -match 'Element.Frost' } | Select-Object -First 3 | ForEach-Object { $_.Line -replace '^.*LogWildreach: ','' }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'LogWildreach: Error|Ensure condition failed|Accessed None' | Measure-Object).Count)"
