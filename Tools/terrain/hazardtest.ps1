# Biome gear-check hazards on L_Island: a frost-main player in the lava river takes damage, a fire-main
# player (Burning blood = lava resistance) doesn't; on the glacier ice the fire player loses grip and
# the frost player (Permafrost = ice resistance) keeps it. Cell positions come from Data/island_dressing.json.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-hazard-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$bots = @(
	@{ N = 'Frosty'; P = 'frost'; Lava = 'WRTeleport -40400 85000'; Ice = 'WRTeleport 48400 4000' },
	@{ N = 'Blaze';  P = 'fire';  Lava = 'WRTeleport -48800 85600'; Ice = 'WRTeleport 109000 4000' }
)
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7822 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$clients = foreach ($b in $bots) {
	$steps = @('WRBotPause 1', 'wait 2', $b.Lava, 'wait 3', 'WRHazard', 'wait 4', 'WRHazard', 'WRHeal', $b.Ice, 'wait 3', 'WRHazard') -join '|'
	Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7822?Path=$($b.P)?WRName=$($b.N) -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$steps`" -abslog=`"$out\$($b.N).log`"" -PassThru -WindowStyle Hidden
}
Start-Sleep -Seconds 45
foreach ($p in @($clients) + $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path "$out\server.log" -Pattern 'HAZARDS loaded' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ','' }
Select-String -Path "$out\Frosty.log", "$out\Blaze.log" -Pattern 'NOTICE hazard' | ForEach-Object { "$($_.Filename): " + ($_.Line -replace '^.*NOTICE ','') }
"server errors: $((Select-String -Path "$out\server.log" -Pattern 'LogWildreach: Error|Ensure condition failed|Accessed None' | Measure-Object).Count)"
