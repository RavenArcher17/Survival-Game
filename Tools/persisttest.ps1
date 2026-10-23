# Persistence + altar test (M2 gate): build a character, restart the server, rejoin, check it's intact.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-persist-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$save = Join-Path (Split-Path $project) 'Saved\ServerDB\players\PersistBot.json'
Remove-Item $save -ErrorAction SilentlyContinue

function Run-Phase($name, $url, $cmds, $seconds) {
	$slog = Join-Path $out "$name-server.log"; $clog = Join-Path $out "$name-client.log"
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7781 -abslog=`"$slog`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 20
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7781$url -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$clog`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds $seconds
	foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
	Start-Sleep -Seconds 2
	return @{ Server = $slog; Client = $clog }
}

# Phase 1: frost main, earn and spend, take fire at the fire altar, earn and spend fire.
$p1 = Run-Phase 'phase1' '?Path=frost?WRName=PersistBot' 'WRGivePoints frost 40|WRAutoSpend frost 30|WRGoToAltar fire|WRInteract|WRGivePoints fire 30|WRAutoSpend fire 25' 30
"=== phase 1"
Select-String -Path $p1.Server -Pattern 'ALTAR|SAVE |PATH apply .*PersistBot|CMD ' | Select-Object -Last 8 | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]','').Trim() }
Select-String -Path $p1.Client -Pattern 'NOTICE' | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]','').Trim() }
"save file exists: $(Test-Path $save)"
if (Test-Path $save) { Get-Content $save -Raw }

# Phase 2: fresh server, rejoin asking for fire main; the save should win.
$p2 = Run-Phase 'phase2' '?Path=fire?WRName=PersistBot' '' 25
"=== phase 2"
Select-String -Path $p2.Server -Pattern 'LOAD |PATH apply' | Select-Object -First 4 | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]','').Trim() }
Select-String -Path $p2.Client -Pattern 'BOT ' | Select-Object -Last 1 | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]','').Trim() }
"errors: $((Select-String -Path $p1.Server, $p2.Server -Pattern 'Error:|Ensure condition failed' | Measure-Object).Count)"
