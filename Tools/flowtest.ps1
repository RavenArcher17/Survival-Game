# Death and respawn (WRUIFlow) on the arena with the souls death penalty, one scripted player:
# killed by creatures -> the server's death note names them and the bag, the client's death screen comes up, and the
# player is back after the countdown at the camp they started at; then a bed is set and a second death brings them
# back at the bed; a first-session hint fires once and is remembered for the player in GameUserSettings.
param([int]$Port = 7881)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-flow-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path $root 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Flowbot.json'), (Join-Path $db 'world_flowtest.json') -ErrorAction SilentlyContinue

$bot = @(
	'WRBotPause 1', 'WRFlow quiet off', 'WRFlow hints on', 'WRFlow forget', 'wait 2',
	# A hint: learning an ability (hold Q) shows once and is remembered.
	'WRGivePoints fire 3', 'WRAutoSpend fire 3', 'wait 6',
	# Killed by creatures (souls: the bag stays where they fell).
	'WRGive wood 30', 'WRGive stone 20', 'WRTeleport 0 900', 'wait 2', 'WRSpawn magmaw 2', 'wait 45',
	# A bed, set as the respawn point, then a second death.
	'WRClearCreatures', 'WRGive wood 40', 'WRGive hide 8', 'WRGive fiber 12', 'WRTeleport -500 700', 'wait 2', 'WRPlace bed -500 900 0 0', 'wait 1', 'WRTeleport -500 760', 'wait 2', 'WRInteract', 'wait 1',
	'WRTeleport 600 -700', 'wait 2', 'WRKillMe', 'wait 18', 'WRSay flowtest-done'
)
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$port -wrworld=flowtest -wrpreset=pve -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${port}?Path=fire?WRName=Flowbot -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($bot -join '|')`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(150)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	if (Select-String -Path "$out\server.log" -Pattern 'CHAT .*flowtest-done' -Quiet) { Start-Sleep -Seconds 2; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 1

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Lines($file) { @(Select-String -Path $file -Pattern 'LogWildreach: ' | ForEach-Object { $_.Line -replace '^.*LogWildreach: ', '' }) }
# Unreal's compact vectors leave out the zero parts: "V(Y=900.00, Z=27.15)".
function Vec($text) {
	if ($text -notmatch 'V\(([^)]*)\)') { return $null }
	$parts = $Matches[1]
	$v = @(0.0, 0.0, 0.0)
	$i = 0
	foreach ($axis in 'X', 'Y', 'Z') { if ($parts -match "$axis=(-?[\d.]+)") { $v[$i] = [double]$Matches[1] }; $i++ }
	, $v
}
function Dist2D($a, $b) { if ($a -and $b) { [Math]::Sqrt([Math]::Pow($a[0] - $b[0], 2) + [Math]::Pow($a[1] - $b[1], 2)) } else { 1e9 } }
$server = Lines "$out\server.log"
$client = Lines "$out\client.log"
$notes = @($server | Where-Object { $_ -match '^FLOW death note for Flowbot' })
Check 'the server sends a death note naming the killer' ($notes.Count -ge 1 -and $notes[0] -match "killed by 'Magmaw'") $(if ($notes.Count) { $notes[0] })
Check 'the note has the souls bag where they fell' ($notes.Count -ge 1 -and $notes[0] -match 'penalty souls, bag of \d+ stacks') $(if ($notes.Count) { $notes[0] })
$screens = @($client | Where-Object { $_ -match '^FLOW death screen' })
Check 'the client shows the death screen' ($screens.Count -ge 1 -and $screens[0] -match "'Magmaw' \(magmaw\), penalty souls, bag \d+ stacks") $(if ($screens.Count) { $screens[0] })
$backs = @($client | Where-Object { $_ -match '^FLOW back from the dead at' })
$wait = if ($backs.Count -ge 1 -and $backs[0] -match '\(([\d.]+) s after dying\)') { [double]$Matches[1] } else { -1 }
Check 'the player is back after the countdown' ($wait -ge 9 -and $wait -le 14) "$wait s"
$respawnAt = if ($notes.Count -ge 1 -and $notes[0] -match "back at '[^']*' (V\([^)]*\))") { Vec $Matches[1] } else { $null }
Check 'they come back where the note said (their start)' ((Dist2D $respawnAt (Vec $backs[0])) -lt 400) "note $respawnAt, back at $(Vec $backs[0])"
Check 'the island is waited for, then the screen fades in' (@($client | Where-Object { $_ -match '^FLOW island streamed in' }).Count -ge 1)
Check 'the bed is set as the respawn point' (Select-String -Path "$out\client.log" -Pattern 'NOTICE Respawn point set' -Quiet)
$bedNote = @($server | Where-Object { $_ -match "^FLOW death note for Flowbot: .*back at 'your bed'" })
Check 'a second death says they wake at their bed' ($bedNote.Count -ge 1) $(if ($bedNote.Count) { $bedNote[0] })
# (The magmaws may kill them more than once before they're cleared: the bed respawn is the last one.)
$bedBack = if ($backs.Count -ge 2) { Vec $backs[-1] } else { $null }
Check 'and they wake at the bed, standing on it (not sunk into the floor)' ($bedBack -and (Dist2D $bedBack @(-500, 900, 0)) -lt 400 -and $bedBack[2] -gt 60) $(if ($backs.Count -ge 2) { $backs[-1] })
$hints = @($client | Where-Object { $_ -match '^FLOW hint ability' })
Check 'a first-session hint names the real key' ($hints.Count -eq 1 -and $hints[0] -match '\[Q\]') ($hints -join '; ')
$ini = Join-Path $root 'Saved\Config\WindowsEditor\GameUserSettings.ini'
Check 'the hint is remembered for the player' ((Test-Path $ini) -and (Select-String -Path $ini -Pattern '^Hints_Flowbot=.*ability' -Quiet))
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
