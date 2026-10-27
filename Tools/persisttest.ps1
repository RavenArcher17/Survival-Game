# Persistence + altar test (M2 gate): build a character, restart the server, rejoin, check it's intact.
# Phase 1: a frost main earns and spends points, takes fire as its subclass at the fire altar, spends fire.
# Phase 2: a fresh server; the player rejoins asking for a fire main, and the save wins (frost main, fire sub).
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-persist-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
$save = Join-Path $db 'players\PersistBot.json'
Remove-Item $save, (Join-Path $db 'world_persisttest.json') -ErrorAction SilentlyContinue

function Saved { if (Test-Path $save) { try { Get-Content $save -Raw | ConvertFrom-Json } catch { $null } } }
function Seen($file, $pattern) { (Test-Path $file) -and (Select-String -Path $file -Pattern $pattern -Quiet) }
# Runs a server and the bot until $done is true (or $seconds pass), then stops both.
function Run-Phase($name, $url, $cmds, $seconds, [scriptblock]$done) {
	$slog = Join-Path $out "$name-server.log"; $clog = Join-Path $out "$name-client.log"
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7781 -wrworld=persisttest -abslog=`"$slog`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 20
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7781$url -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$clog`"" -PassThru -WindowStyle Hidden
	$deadline = (Get-Date).AddSeconds($seconds)
	while ((Get-Date) -lt $deadline -and -not (& $done $slog $clog)) { Start-Sleep -Seconds 2 }
	foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
	Start-Sleep -Seconds 2
	return @{ Server = $slog; Client = $clog }
}

# Phase 1 is done once the player autosave (every 10 s) holds the fire subclass and its spent ranks.
$p1 = Run-Phase 'phase1' '?Path=frost?WRName=PersistBot' 'WRGivePoints frost 40|WRAutoSpend frost 30|WRGoToAltar fire|WRInteract|WRGivePoints fire 30|WRAutoSpend fire 25' 45 {
	$j = Saved; $fire = if ($j) { $j.paths.paths | Where-Object { $_.id -eq 'fire' } }
	$j -and $j.paths.sub -eq 'fire' -and $fire -and @($fire.ranks.PSObject.Properties).Count -ge 5
}
$saved = Saved
# Phase 2 is done once the bot has reported its paths.
$p2 = Run-Phase 'phase2' '?Path=fire?WRName=PersistBot' '' 30 { param($slog, $clog) Seen $clog 'BOT .* main=' }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Lines($file, $pattern) { if (Test-Path $file) { @(Select-String -Path $file -Pattern $pattern | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ', '').Trim() }) } else { @() } }
$altar = Lines $p1.Server 'LogWildreach: ALTAR ' | Select-Object -First 1
Check 'the fire altar gives a frost main the fire subclass' ($altar -match 'PersistBot takes fire as subclass -> ok') $altar
function Ranks($id) { $p = $saved.paths.paths | Where-Object { $_.id -eq $id }; if ($p) { @($p.ranks.PSObject.Properties | ForEach-Object { $_.Value }) | Measure-Object -Sum | ForEach-Object { [int]$_.Sum } } else { 0 } }
Check 'the save holds the main, the subclass and both paths'' ranks' ($saved -and $saved.paths.main -eq 'frost' -and $saved.paths.sub -eq 'fire' -and (Ranks 'frost') -gt 0 -and (Ranks 'fire') -gt 0) $(if ($saved) { "main $($saved.paths.main), sub $($saved.paths.sub), frost ranks $(Ranks 'frost'), fire ranks $(Ranks 'fire')" } else { 'no save' })
Check 'the save holds the quick slots' ($saved -and @($saved.paths.quickSlots) -contains 'frost.frost_shard' -and @($saved.paths.quickSlots) -contains 'fire.fire_bolt') $(if ($saved) { (@($saved.paths.quickSlots) | Where-Object { $_ }) -join ', ' })
$load = Lines $p2.Server '^.*LOAD PersistBot' | Select-Object -First 1
Check 'a fresh server loads the player from the save' ($load -match 'main=frost\(30\) sub=fire\(25\)') $load
$joined = Lines $p2.Server 'Player joined: PersistBot' | Select-Object -First 1
Check 'the save wins over the join URL''s ?Path=fire' ($joined -match 'main=frost') $joined
$bot = Lines $p2.Client 'BOT .* main=' | Select-Object -Last 1
Check 'the player plays on with the same spent points' ($bot -match 'main=frost\(30/\d+\) sub=fire\(25\)') ($bot -replace '^BOT \S+ ', '')
$logs = @($p1.Server, $p1.Client, $p2.Server, $p2.Client | Where-Object { Test-Path $_ })
$errors = @(Select-String -Path $logs -Pattern 'Error:|Ensure condition failed|Accessed None')
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
