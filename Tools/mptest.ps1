# Headless multiplayer soak test: dedicated server + scripted bot clients on the combat arena.
# Stops only the processes it starts, summarises the server/client logs, then checks them.
#   mptest.ps1                    # 90 s, default bots (pve: one hunts the creature packs, one fights a dummy)
#   mptest.ps1 -Seconds 300 -Scenario pvp
param(
	[int]$Seconds = 90,
	[ValidateSet('pve', 'pvp')] [string]$Scenario = 'pve',
	[string]$Map = '/Game/Maps/L_CombatArena',
	[int]$Port = 7785   # not the game's own 7777, so it doesn't clash with a hosted game
)

$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-mptest-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\world_mptest.json') -ErrorAction SilentlyContinue

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
$procs += Start-Process $ue -ArgumentList "`"$project`" $Map -server -nullrhi -unattended -nosound -port=$Port -wrworld=mptest -wrpreset=pvp_standard `"-ExecCmds=p.NetShowCorrections 1`" `"-LogCmds=LogWildreach Verbose`" -abslog=`"$serverLog`"" -PassThru -WindowStyle Hidden
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

$n = [ordered]@{
	joins = Count $serverLog 'Player joined'; hits = Count $serverLog 'COMBAT .* result=hit'; blocked = Count $serverLog 'result=blocked'
	guardbroken = Count $serverLog 'result=guardbroken'; dodged = Count $serverLog 'result=dodged'; parries = Count $serverLog 'COMBAT .* parried'
	ripostes = Count $serverLog 'COMBAT riposte'; thaws = Count $serverLog 'COMBAT thaw'; shatters = Count $serverLog 'COMBAT shatter'
	burn = Count $serverLog 'STATUS .*\+Status.Burn'; frostbite = Count $serverLog 'STATUS .*\+Status.Frostbite'; frozen = Count $serverLog 'STATUS .*\+Status.Frozen'
	staggers = Count $serverLog 'STATUS .*\+Status.Staggered'; fire = Count $serverLog 'elem=Element.Fire result=hit'; frost = Count $serverLog 'elem=Element.Frost result=hit'
	deaths = Count $serverLog 'DEATH '; kills = Count $serverLog 'DEATH WRCreature_\d+ killed by WRCharacter'; points = Count $serverLog 'PATH .* earned a point'; applies = Count $serverLog 'PATH apply'
	corrections = Count $serverLog '\*\*\* Server: Error for'; overflow = Count $serverLog 'Hit limit of 96 saved moves'
}
"=== Wildreach MP test ($Scenario, $Seconds s)"
foreach ($k in $n.Keys) { "{0,-12} {1}" -f $k, $n[$k] }
"--- abilities used (server-confirmed) ---"
$used = Select-String -Path $serverLog -Pattern 'ABILITY \S+ used (\S+)' -ErrorAction SilentlyContinue | Group-Object { $_.Matches[0].Groups[1].Value }
$used | Sort-Object Count -Descending | ForEach-Object { "{0,5}  {1}" -f $_.Count, $_.Name }
foreach ($b in $bots) {
	"--- $($b.Name) ---"
	Select-String -Path (Join-Path $out "$($b.Name).log") -Pattern 'BOT ' -ErrorAction SilentlyContinue | Select-Object -Last 2 | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ', '').Trim() }
}

# ---- checks
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Uses([string]$ability) { $g = $used | Where-Object { $_.Name -eq $ability }; if ($g) { $g.Count } else { 0 } }
Check "both bots join and are welcomed" ($n.joins -eq 2 -and @($bots | Where-Object { (Count (Join-Path $out "$($_.Name).log") 'Welcomed by server') -ge 1 }).Count -eq 2) "$($n.joins) joins"
Check "hits land" ($n.hits -ge 20) "$($n.hits) hits"
Check "fire and frost both land" ($n.fire -gt 0 -and $n.frost -gt 0) "fire $($n.fire), frost $($n.frost)"
Check "statuses build up (burn, frostbite, frozen) and staggers happen" (($n.burn + $n.frostbite + $n.frozen) -ge 2 -and $n.staggers -ge 1) "burn $($n.burn), frostbite $($n.frostbite), frozen $($n.frozen), staggers $($n.staggers)"
$abilities = if ($Scenario -eq 'pve') { 'WRGA_LightAttack', 'WRGA_HeavyAttack', 'WRGA_Block', 'WRGA_Dodge', 'WRGA_FireBolt', 'WRGA_FrostShard', 'WRGA_FlameSlash', 'WRGA_IceSlash', 'WRGA_Bite' } else { 'WRGA_LightAttack', 'WRGA_HeavyAttack', 'WRGA_Block', 'WRGA_Dodge' }
$missing = @($abilities | Where-Object { (Uses $_) -eq 0 })
Check "every kind of ability is used and confirmed by the server" ($missing.Count -eq 0) $(if ($missing) { 'never used: ' + ($missing -join ', ') } else { "$($abilities.Count) kinds" })
if ($Scenario -eq 'pve') { Check "the hunting bot kills creatures" ($n.kills -ge 1) "$($n.kills) kills, $($n.deaths) deaths in all" }
Check "path abilities are granted" ($n.applies -ge 2) "$($n.applies) applies"
# Knockback, staggers and the 80 ms lag bot give a few dozen corrections a run (13-56 seen); a broken sync gives hundreds.
Check "movement stays in sync (no saved-move overflow, few corrections)" ($n.overflow -eq 0 -and $n.corrections -le $Seconds) "$($n.corrections) corrections, $($n.overflow) overflows"
$logs = @($serverLog) + @($bots | ForEach-Object { Join-Path $out "$($_.Name).log" })
$errors = @(Select-String -Path $logs -Pattern 'Error:|Ensure condition failed|Accessed None|Fatal error' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check "no errors in the logs" ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
