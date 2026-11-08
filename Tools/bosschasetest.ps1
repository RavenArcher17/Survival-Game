# A boss chases you past its arena's floor and walks home when you leave (0.7 polish, from Raven's play: "it can't walk
# far and it glitches back or doesn't pursue"). On the arena map, Grimjaw (a mini boss: its floor is 22 m, it chases
# within 65 m of home): the player walks it 30-40 m from home, it follows and fights; then 200 m off, it resets after
# resetSeconds and walks home on foot (no teleport).
#   bosschasetest.ps1 [-Port 7899]
param([int]$Port = 7899)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-bosschase-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\Worlds\bosschasetest'
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$tune = '-wrtune=bosses.damageScale:0.02'
$hero = @('WRBotPause 1', 'WRTeleport 0 4200', 'wait 3', 'WRSpawn grimjaw 1', 'wait 5', 'WRCmd bossinfo',
	# Away from it, past its old 22 m: it follows.
	'WRTeleport 0 700', 'wait 3', 'WRCmd bossinfo', 'WRHeal', 'wait 5', 'WRCmd bossinfo', 'WRHeal', 'wait 4', 'WRCmd bossinfo',
	# Out of its reach for good: it resets and walks home.
	'WRTeleport 0 -18000', 'wait 12', 'WRCmd bossinfo', 'wait 4', 'WRCmd bossinfo', 'wait 14', 'WRCmd bossinfo', 'WRSay bosschase-done')
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=$Port -wrworld=bosschasetest $tune -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 20
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=Hero -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($hero -join '|')`" -abslog=`"$out\hero.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(150)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path "$out\server.log" -Pattern 'CHAT .*bosschase-done' -Quiet -ErrorAction SilentlyContinue)) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$lines = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: (BOSS|BOSSINFO|COMBAT Grimjaw)' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ', '').Trim() })
$lines | Where-Object { $_ -match '^(BOSS |BOSSINFO)' } | ForEach-Object { "  $_" }
$info = @($lines | Where-Object { $_ -match '^BOSSINFO Grimjaw: ([\d.]+) m from home, nearest player ([\d.-]+) m, ([a-z ]+), ([\d.]+)/([\d.]+) hp' } | ForEach-Object {
	[pscustomobject]@{ home = [double]$Matches[1]; player = [double]$Matches[2]; state = $Matches[3]; hp = [double]$Matches[4]; max = [double]$Matches[5] } })
Check 'Grimjaw is spawned and engaged' ($info.Count -ge 1 -and $info[0].state -eq 'fighting') $(if ($info) { "$($info[0].state)" })
# Infos 1-3: the player 30-40 m from its home.
$followed = @($info | Select-Object -Skip 1 -First 3 | Where-Object { $_.state -eq 'fighting' -and $_.home -gt 22 -and $_.player -lt 12 })
Check 'it follows the player past its old 22 m and stays in the fight' ($followed.Count -ge 1) (($info | Select-Object -Skip 1 -First 3 | ForEach-Object { "$($_.home) m from home, player $($_.player) m, $($_.state)" }) -join '; ')
$reset = @($lines | Where-Object { $_ -match '^BOSS Grimjaw reset \(nobody within 65 m\): walks home' }).Count
Check 'once the player is gone it resets and walks home (no teleport)' ($reset -eq 1 -and @($info | Select-Object -Skip 4 | Where-Object { $_.state -eq 'walking home' -and $_.home -gt 3 }).Count -ge 1)
Check 'it gets home on foot, at full health' (@($lines | Where-Object { $_ -match '^BOSS Grimjaw is home' }).Count -eq 1 -and @($lines | Where-Object { $_ -match "couldn't walk home" }).Count -eq 0 -and $info[-1].state -eq 'idle' -and $info[-1].hp -eq $info[-1].max -and $info[-1].home -lt 4)
$errors = @(Select-String -Path "$out\server.log", "$out\hero.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
