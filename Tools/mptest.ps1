# Headless multiplayer soak test: dedicated server + scripted bot clients on the combat arena.
# Stops only the processes it starts, then summarises the server/client logs.
#   mptest.ps1                    # 90 s, default bots
#   mptest.ps1 -Seconds 300 -Scenario pvp
param(
	[int]$Seconds = 90,
	[ValidateSet('pve', 'pvp')] [string]$Scenario = 'pve',
	[string]$Map = '/Game/Maps/L_CombatArena',
	[int]$Port = 7777
)

$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-mptest-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null

if ($Scenario -eq 'pve') {
	$bots = @(
		@{ Name = 'FireBot';  Path = 'fire';  Flags = '-wrbot -wrbothunt'; Cmds = 'WRGivePoints fire 60|WRAutoSpend fire 30|WRSetSub frost|WRGivePoints frost 40|WRAutoSpend frost 25|Net PktLag=80' },
		@{ Name = 'FrostBot'; Path = 'frost'; Flags = '-wrbot';            Cmds = 'WRGivePoints frost 20|WRAutoSpend frost 10|WRSetSub fire|WRGivePoints fire 12|WRAutoSpend fire 10' }
	)
} else {
	$bots = @(
		@{ Name = 'DuelA'; Path = 'fire';  Flags = '-wrbotpvp'; Cmds = 'WRGivePoints fire 20|WRAutoSpend fire 10|Net PktLag=60' },
		@{ Name = 'DuelB'; Path = 'frost'; Flags = '-wrbotpvp'; Cmds = 'WRGivePoints frost 20|WRAutoSpend frost 10' }
	)
}

$procs = @()
$serverLog = Join-Path $out 'server.log'
$procs += Start-Process $ue -ArgumentList "`"$project`" $Map -server -nullrhi -unattended -nosound -port=$Port `"-ExecCmds=p.NetShowCorrections 1`" `"-LogCmds=LogWildreach Verbose`" -abslog=`"$serverLog`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
foreach ($b in $bots) {
	$log = Join-Path $out "$($b.Name).log"
	# Headless clients otherwise run at thousands of fps and flood the server with moves.
	$args = "`"$project`" 127.0.0.1:${Port}?Path=$($b.Path)?WRName=$($b.Name) -game -nullrhi -unattended -nosound $($b.Flags) `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($b.Cmds)`" -abslog=`"$log`""
	$procs += Start-Process $ue -ArgumentList $args -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 2
}

Start-Sleep -Seconds $Seconds
foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2

function Count($file, $pattern) { (Select-String -Path $file -Pattern $pattern -ErrorAction SilentlyContinue | Measure-Object).Count }

"=== Wildreach MP test ($Scenario, $Seconds s) logs: $out"
"joins:            $(Count $serverLog 'Player joined')"
"hits:             $(Count $serverLog 'COMBAT .* result=hit')"
"blocked:          $(Count $serverLog 'result=blocked')"
"guard broken:     $(Count $serverLog 'result=guardbroken')"
"dodged (iframes): $(Count $serverLog 'result=dodged')"
"parries:          $(Count $serverLog 'COMBAT .* parried')"
"ripostes:         $(Count $serverLog 'COMBAT riposte')"
"thaws:            $(Count $serverLog 'COMBAT thaw')"
"shatters:         $(Count $serverLog 'COMBAT shatter')"
"burn applied:     $(Count $serverLog 'STATUS .*\+Status.Burn')"
"frostbite:        $(Count $serverLog 'STATUS .*\+Status.Frostbite')"
"frozen:           $(Count $serverLog 'STATUS .*\+Status.Frozen')"
"staggers:         $(Count $serverLog 'STATUS .*\+Status.Staggered')"
"fire elem hits:   $(Count $serverLog 'elem=Element.Fire result=hit')"
"frost elem hits:  $(Count $serverLog 'elem=Element.Frost result=hit')"
"deaths:           $(Count $serverLog 'DEATH ')"
"points earned:    $(Count $serverLog 'PATH .* earned a point')"
"path applies:     $(Count $serverLog 'PATH apply')"
"move corrections: $(Count $serverLog '\*\*\* Server: Error for')  (server-side, all clients)"
"move overflow:    $(Count $serverLog 'Hit limit of 96 saved moves')"
"--- abilities used (server-confirmed) ---"
Select-String -Path $serverLog -Pattern 'ABILITY \S+ used (\S+)' -ErrorAction SilentlyContinue | Group-Object { $_.Matches[0].Groups[1].Value } | Sort-Object Count -Descending | ForEach-Object { "{0,5}  {1}" -f $_.Count, $_.Name }
"--- errors (server) ---"
Select-String -Path $serverLog -Pattern 'Error:|Ensure condition failed|Accessed None|Fatal' -ErrorAction SilentlyContinue | Select-Object -First 15 | ForEach-Object { $_.Line.Trim() }
foreach ($b in $bots) {
	$log = Join-Path $out "$($b.Name).log"
	"--- $($b.Name) ---"
	"welcomed: $(Count $log 'Welcomed by server')  saved-move overflow: $(Count $log 'Hit limit of 96 saved moves')"
	Select-String -Path $log -Pattern 'CMD |PATH ' -ErrorAction SilentlyContinue | Select-Object -First 8 | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]','').Trim() }
	Select-String -Path $log -Pattern 'BOT ' -ErrorAction SilentlyContinue | Select-Object -Last 3 | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]','').Trim() }
	Select-String -Path $log -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Select-Object -First 8 | ForEach-Object { $_.Line.Trim() }
}
