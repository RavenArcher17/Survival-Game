# The auto-update path end to end, offline: two tiny fake releases (0.2.0 and 0.2.1 of a stand-in game) built with the
# real make_installer.ps1, a local stand-in for GitHub's releases pages (node), 0.2.0 installed to a scratch folder.
# Then the updater, as the shortcut runs it: finds 0.2.1, downloads its Setup, installs it over the folder (renaming
# itself aside while it runs) and starts the game with the shortcut's arguments; next time it's up to date and starts
# straight away; a release tag that isn't a version, or no network, also just starts the game. No shortcuts are made.
#   updatertest.ps1
$ErrorActionPreference = 'Stop'
$out = Join-Path $env:TEMP ("wildreach-updater-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$csc = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$port = 7871

# A stand-in game: notes each start (and its arguments) in launched.txt beside it.
$fake = Join-Path $out 'FakeGame.cs'
[System.IO.File]::WriteAllText($fake, 'using System; using System.IO; static class FakeGame { static void Main(string[] a) { File.AppendAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launched.txt"), "launched " + string.Join(" ", a) + "\r\n"); } }')
foreach ($v in '0.2.0', '0.2.1') {
	$game = Join-Path $out "build-$v\Windows"
	New-Item -ItemType Directory -Force $game | Out-Null
	& $csc /nologo /target:winexe "/out:$game\Wildreach.exe" $fake | Out-Null
	[System.IO.File]::WriteAllText("$game\release.txt", "this is $v")
	& (Join-Path $PSScriptRoot 'make_installer.ps1') -Build (Join-Path $out "build-$v") -Version $v -Out (Join-Path $out "setup-$v.exe") | Select-Object -Last 1
}

# GitHub stand-in: /releases/latest redirects to /releases/tag/<tag.txt>, /releases/download/<tag>/Wildreach-Setup.exe
# serves the 0.2.1 Setup. Every request is logged.
$tagFile = Join-Path $out 'tag.txt'
[System.IO.File]::WriteAllText($tagFile, 'v0.2.1')
$server = Join-Path $out 'feed.mjs'
[System.IO.File]::WriteAllText($server, @"
import http from 'http'; import fs from 'fs';
const dir = process.argv[2];
http.createServer((req, res) => {
  fs.appendFileSync(dir + '/requests.log', req.url + '\n');
  const tag = fs.readFileSync(dir + '/tag.txt', 'utf8').trim();
  if (req.url === '/releases/latest') { res.writeHead(302, { Location: 'http://127.0.0.1:$port/releases/tag/' + tag }); return res.end(); }
  if (req.url === '/releases/download/' + tag + '/Wildreach-Setup.exe') {
    const file = dir + '/setup-0.2.1.exe';
    res.writeHead(200, { 'Content-Length': fs.statSync(file).size, 'Content-Type': 'application/octet-stream' });
    return fs.createReadStream(file).pipe(res);
  }
  res.writeHead(404); res.end();
}).listen($port, '127.0.0.1');
"@)
$node = Start-Process node -ArgumentList "`"$server`" `"$($out -replace '\\','/')`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

$install = Join-Path $out 'install'
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Updater([string]$feed, [string]$extra) {
	$p = Start-Process (Join-Path $install 'WildreachUpdater.exe') -ArgumentList "--feed $feed $extra" -PassThru
	if (-not $p.WaitForExit(120000)) { Stop-Process -Id $p.Id -Force; return $false }
	Start-Sleep -Seconds 2   # the game it started writes its note
	return $true
}
function Launches { if (Test-Path "$install\launched.txt") { @(Get-Content "$install\launched.txt") } else { @() } }
function Downloads { @(Get-Content "$out\requests.log" -ErrorAction SilentlyContinue | Where-Object { $_ -match '/download/' }).Count }
try {
	$p = Start-Process (Join-Path $out 'setup-0.2.0.exe') -ArgumentList "--silent --nolaunch --noshortcuts --dir `"$install`"" -PassThru -Wait
	Check '0.2.0 installs silently with the updater and its version' ($p.ExitCode -eq 0 -and (Test-Path "$install\WildreachUpdater.exe") -and (Get-Content "$install\version.txt") -eq '0.2.0')

	$ok = Updater "http://127.0.0.1:$port" '-hello'
	$log = Get-Content "$install\updater.log" -Raw -ErrorAction SilentlyContinue
	Check 'the updater found 0.2.1 and installed it' ($ok -and (Get-Content "$install\version.txt") -eq '0.2.1' -and (Get-Content "$install\release.txt") -eq 'this is 0.2.1') $log
	Check 'it downloaded the new Setup once' ((Downloads) -eq 1) ("{0} downloads" -f (Downloads))
	Check 'it set its running self aside to be replaced' (Test-Path "$install\WildreachUpdater.exe.old")
	Check 'then started the game with the shortcut''s arguments' (@(Launches).Count -eq 1 -and @(Launches)[0] -eq 'launched -hello') ((Launches) -join ' / ')
	Check 'the downloaded Setup was cleaned up' (-not (Get-ChildItem (Join-Path $env:TEMP 'Wildreach-Update') -Filter '*v0.2.1*' -ErrorAction SilentlyContinue))

	$ok = Updater "http://127.0.0.1:$port" ''
	Check 'up to date: it starts the game without downloading' ($ok -and @(Launches).Count -eq 2 -and (Downloads) -eq 1) ("{0} launches, {1} downloads" -f @(Launches).Count, (Downloads))
	Check 'and cleared the old copy of itself' (-not (Test-Path "$install\WildreachUpdater.exe.old"))

	[System.IO.File]::WriteAllText($tagFile, 'playtest-2026-10-25')
	$ok = Updater "http://127.0.0.1:$port" ''
	Check 'a release tag that isn''t a version is never an update' ($ok -and @(Launches).Count -eq 3 -and (Downloads) -eq 1 -and (Get-Content "$install\version.txt") -eq '0.2.1')

	$clock = [Diagnostics.Stopwatch]::StartNew()
	$ok = Updater 'http://127.0.0.1:1' ''
	Check 'offline: it starts the game anyway, quickly' ($ok -and @(Launches).Count -eq 4 -and $clock.Elapsed.TotalSeconds -lt 15) ("{0:N1} s" -f $clock.Elapsed.TotalSeconds)
}
finally {
	if (-not $node.HasExited) { Stop-Process -Id $node.Id -Force }
}

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail.Trim()))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (files: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
