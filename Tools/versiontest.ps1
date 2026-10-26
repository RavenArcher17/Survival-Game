# Versions in the game: a server on another version (0.1.9, set on its command line) turns a 0.2.0 client away, the
# client lands back in the main menu saying why ("the host has a different version"), and the menu's update check,
# pointed at a local stand-in for GitHub saying v9.9.9 is out, offers the update. Then a client on the server's
# version joins fine.
#   versiontest.ps1
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-version-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$port = 7873
$version = ((Get-Content (Join-Path $PSScriptRoot '..\Config\DefaultGame.ini') | Where-Object { $_ -match '^ProjectVersion=' } | Select-Object -First 1) -replace '^ProjectVersion=', '').Trim()
$old = '[/Script/EngineSettings.GeneralProjectSettings]:ProjectVersion=0.1.9'
# (Test-Path first: Select-String on a log that isn't there yet throws, even with -ErrorAction SilentlyContinue.)
function Has([string]$file, [string]$pattern) { (Test-Path $file) -and (Select-String -Path $file -Pattern $pattern -Quiet) }
[System.IO.File]::WriteAllText("$out\feed.mjs", "import http from 'http'; http.createServer((q, r) => { r.writeHead(200, { 'Content-Type': 'application/json' }); r.end(JSON.stringify({ tag_name: 'v9.9.9' })); }).listen($port, '127.0.0.1');")
$node = Start-Process node -ArgumentList "`"$out\feed.mjs`"" -PassThru -WindowStyle Hidden

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7838 -wrworld=versiontest `"-ini:Game:$old`" -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 25
# Not a bot: a plain player whose join fails falls back to the menu.
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7838?WRName=Newer -game -nullrhi -unattended -nosound -wrupdatefeed=http://127.0.0.1:$port/latest -abslog=`"$out\client.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline -and -not ((Has "$out\client.log" 'MENU update check') -and (Has "$out\client.log" 'MENU shows'))) { Start-Sleep -Seconds 3 }
Start-Sleep -Seconds 2
if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
# The same version as the server joins.
$c2 = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7838?WRName=Same -game -nullrhi -unattended -nosound -nomenu `"-ini:Game:$old`" -abslog=`"$out\client2.log`"" -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline -and -not (Has "$out\server.log" 'Join succeeded: Same')) { Start-Sleep -Seconds 3 }
foreach ($p in $c2, $s, $node) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\world_versiontest.json') -ErrorAction SilentlyContinue

$client = @(if (Test-Path "$out\client.log") { Select-String -Path "$out\client.log" -Pattern 'LogWildreach: ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') } })
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
$failure = [string]($client | Where-Object { $_ -match '^MENU network failure' } | Select-Object -First 1)
$shows = [string]($client | Where-Object { $_ -match '^MENU shows' } | Select-Object -First 1)
$check = [string]($client | Where-Object { $_ -match '^MENU update check' } | Select-Object -First 1)
Check "the server (0.1.9) turned the $version client away" ($failure -match 'Outdated') $failure
Check 'the menu says why' ($shows -match ('different version of Wildreach \(you have ' + [regex]::Escape($version))) $shows
Check 'the menu offers the newer release' ($check -match ('this is ' + [regex]::Escape($version) + ', the latest release is v9\.9\.9: offering the update')) $check
Check 'a client on the same version joins' (Has "$out\server.log" 'Join succeeded: Same')
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
