# The auto-update path end to end, offline. Two fake releases (0.2.0 and 0.2.1 of a stand-in game) built with the real
# make_installer.ps1, a local stand-in for GitHub's releases pages (node) that serves parts of a file as GitHub does and
# counts every byte, and 0.2.0 installed fresh to a scratch folder for each case.
#   The small update: fetches only what changed, the folder ends up exactly as a fresh install, saves and stray files alone.
#   When it can't be done (no partial downloads, a damaged block, an index it can't read, an old-style Setup, too much
#   changed): the whole Setup, and the same result. Both download over no more than 4 connections at once; the whole Setup
#   comes in parts when the server does partial downloads and the parts so far came down right, else in one go.
#   Interrupted: a dropped connection picks up; a killed updater leaves the game as it was and finishes next time.
#   A file in use: everything put back, the game still starts, and next time it works.
#   The updater from 0.6.0 (the one every player has now) still installs a new Setup. Up to date, a tag that isn't a
#   version and no network all just start the game. No shortcuts are made.
#   updatertest.ps1
$ErrorActionPreference = 'Stop'
$out = Join-Path $env:TEMP ("wildreach-updater-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out, "$out\temp" | Out-Null
# Its own temp folder: the updater clears Wildreach-Update in there, and a real install's is nobody's business here.
$env:TEMP = "$out\temp"; $env:TMP = "$out\temp"
$csc = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$fx = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
$repo = Split-Path $PSScriptRoot
$port = 7871
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }

# ---- the block code itself
$unit = Join-Path $out 'PackCheck.cs'
[System.IO.File]::WriteAllText($unit, @'
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Security.Cryptography; using WildreachPack;
static class C {
	static List<string> Names(byte[] data) { var n = new List<string>(); var sha = new SHA256Cng(); Blocks.Cut(new MemoryStream(data), (b, s, l, o) => n.Add(Blocks.Name(sha, b, s, l))); return n; }
	static int Main() {
		var data = new byte[8 << 20]; new Random(7).NextBytes(data);
		var a = Names(data);
		Console.WriteLine("same bytes cut the same: " + a.SequenceEqual(Names(data)));
		long total = 0; bool sizes = true; int count = 0;
		Blocks.Cut(new MemoryStream(data), (b, s, l, o) => { if (o != total) sizes = false; total += l; count++; if (l > Blocks.MaxSize) sizes = false; });
		Console.WriteLine("blocks cover the file, none too big: " + (sizes && total == data.Length));
		// A few bytes put in near the start: everything after still cuts the same, though it's all moved.
		var moved = new byte[data.Length + 50]; Buffer.BlockCopy(data, 0, moved, 0, 100000); Buffer.BlockCopy(data, 100000, moved, 100050, data.Length - 100000);
		var b2 = Names(moved); int shared = b2.Count(n => a.Contains(n));
		Console.WriteLine("an insertion only changes the blocks around it: " + (shared >= a.Count - 2) + " (" + shared + " of " + a.Count + ")");
		foreach (var empty in new[] { 0, 1, 1000 }) { int got = 0; long len = 0; Blocks.Cut(new MemoryStream(new byte[empty]), (b, s, l, o) => { got++; len += l; }); if (len != empty) Console.WriteLine("short file wrong: False"); }
		var text = new byte[300000]; var packed = Blocks.Pack(text, 0, text.Length);
		Console.WriteLine("a block packs and unpacks: " + (packed.Length < text.Length && Blocks.Unpack(packed, 0, packed.Length, text.Length).SequenceEqual(text)));
		Console.WriteLine("a raw block unpacks as itself: " + Blocks.Unpack(data, 5, 1000, 1000).SequenceEqual(data.Skip(5).Take(1000)));
		var ix = new Index { Version = "1.2.3" }; var f = new PackedFile { Path = @"Wildreach\Data\a b.json", Sha = "ab", Size = 5 }; f.Blocks.Add(new BlockRef { Name = "c", Size = 5, Packed = 4, At = 99 }); ix.Files.Add(f);
		var back = Index.Load(ix.Save());
		Console.WriteLine("the index saves and loads: " + (back.Version == "1.2.3" && back.Files[0].Path == f.Path && back.Files[0].Blocks[0].At == 99));
		long at, len2; Console.WriteLine("the footer reads back: " + (Index.ReadFooter(Index.Footer(1234567890123, 55), out at, out len2) && at == 1234567890123 && len2 == 55));
		Console.WriteLine("a Setup with no footer is noticed: " + !Index.ReadFooter(new byte[Index.FooterSize], out at, out len2));
		foreach (var bad in new[] { @"..\evil.exe", @"C:\evil.exe", @"\evil.exe", @"Wildreach\Saved\Worlds\x.json", @"Wildreach\..\..\x", @"a\.\b", "" }) if (Index.IsSafePath(bad)) Console.WriteLine("a path it must refuse got through (" + bad + "): False");
		foreach (var good in new[] { "version.txt", @"Wildreach\Data\items.json", @"Engine\Content\Saved.txt" }) if (!Index.IsSafePath(good)) Console.WriteLine("a path it must allow was refused (" + good + "): False");
		// An index naming a player's save must not load at all.
		var evil = new Index { Version = "9" }; evil.Files.Add(new PackedFile { Path = @"Wildreach\Saved\x", Sha = "a" }); try { Index.Load(evil.Save()); Console.WriteLine("an index that names a save is refused: False"); } catch (InvalidDataException) { Console.WriteLine("an index that names a save is refused: True"); }
		return 0;
	}
}
'@)
& $csc /nologo /optimize /platform:x64 "/out:$out\PackCheck.exe" $unit (Join-Path $PSScriptRoot 'installer\WildreachPack.cs') | Out-Null
foreach ($line in & "$out\PackCheck.exe") { if ($line -match '^(.*?): (True|False)(.*)$') { Check "blocks: $($Matches[1])" ($Matches[2] -eq 'True') $Matches[3].Trim() } }

# ---- two releases of a stand-in game: most of it the same, as a real update is
$fake = Join-Path $out 'FakeGame.cs'
[System.IO.File]::WriteAllText($fake, 'using System; using System.IO; static class FakeGame { static void Main(string[] a) { File.AppendAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launched.txt"), "launched " + string.Join(" ", a) + "\r\n"); } }')
$rand = New-Object Random 11
$content = New-Object byte[] (30MB); $rand.NextBytes($content)
$code = New-Object byte[] (6MB); $rand.NextBytes($code)
foreach ($v in '0.2.0', '0.2.1') {
	$game = Join-Path $out "build-$v\Windows"
	New-Item -ItemType Directory -Force "$game\Wildreach\Content\Paks", "$game\Wildreach\Data" | Out-Null
	& $csc /nologo /target:winexe "/out:$game\Wildreach.exe" $fake | Out-Null
	[System.IO.File]::WriteAllText("$game\release.txt", "this is $v")
	[System.IO.File]::WriteAllBytes("$game\Wildreach\Content\Paks\game.ucas", $content)   # the same in both
	foreach ($n in 1..40) { [System.IO.File]::WriteAllText("$game\Wildreach\Data\item$n.json", "{ `"item`": $n }") }
	if ($v -eq '0.2.0') {
		[System.IO.File]::WriteAllBytes("$game\Wildreach\code.bin", $code)
		[System.IO.File]::WriteAllText("$game\Wildreach\Data\dropped.json", 'gone in 0.2.1')
		[System.IO.File]::WriteAllText("$game\Wildreach\Data\item7.json", '{ "item": 7, "balance": "old" }')
	} else {
		# Bytes put in near the start: all of it moves, little of it changes. A new file; one gone (above).
		$moved = New-Object byte[] ($code.Length + 500)
		[Array]::Copy($code, 0, $moved, 0, 500000); [Array]::Copy($code, 500000, $moved, 500500, $code.Length - 500000)
		[System.IO.File]::WriteAllBytes("$game\Wildreach\code.bin", $moved)
		[System.IO.File]::WriteAllText("$game\Wildreach\Data\added.json", 'new in 0.2.1')
		[System.IO.File]::WriteAllText("$game\Wildreach\Data\item7.json", '{ "item": 7, "balance": "new" }')
	}
	& (Join-Path $PSScriptRoot 'make_installer.ps1') -Build (Join-Path $out "build-$v") -Version $v -Out (Join-Path $out "setup-$v.exe") | Select-Object -Last 1
}
$setupSize = (Get-Item "$out\setup-0.2.1.exe").Length

# ---- GitHub stand-in: /releases/latest redirects to /releases/tag/<tag.txt>; the Setup comes through a redirect, whole or
# in parts (exact ranges only: GitHub's store refuses "the last N bytes"). mode.txt makes it misbehave.
$tagFile = Join-Path $out 'tag.txt'
$modeFile = Join-Path $out 'mode.txt'
[System.IO.File]::WriteAllText($tagFile, 'v0.2.1')
[System.IO.File]::WriteAllText($modeFile, 'normal')
$server = Join-Path $out 'feed.mjs'
[System.IO.File]::WriteAllText($server, @"
import http from 'http'; import fs from 'fs';
const dir = process.argv[2], size = fs.statSync(dir + '/setup-0.2.1.exe').size;
let dropped = false, lastMode = '', open = 0, t0 = 0, sent = 0, best = 0;
function count(n) { let t = 0; try { t = +fs.readFileSync(dir + '/served.txt', 'utf8'); } catch {} fs.writeFileSync(dir + '/served.txt', String(t + n)); }
http.createServer((req, res) => {
  const mode = fs.readFileSync(dir + '/mode.txt', 'utf8').trim();
  const tag = fs.readFileSync(dir + '/tag.txt', 'utf8').trim();
  if (req.url === '/releases/latest') { res.writeHead(302, { Location: 'http://127.0.0.1:$port/releases/tag/' + tag }); return res.end(); }
  if (req.url === '/releases/download/' + tag + '/Wildreach-Setup.exe') {
    res.writeHead(302, { Location: 'http://127.0.0.1:$port/store/setup.exe' }); return res.end();
  }
  if (req.url !== '/store/setup.exe') { res.writeHead(404); return res.end(); }
  if (mode !== lastMode) { dropped = false; lastMode = mode; t0 = Date.now(); sent = 0; best = 0; }
  // How many parts are on their way at once, most at a time (most.txt).
  open++; res.on('close', () => open--);
  let most = 0; try { most = +fs.readFileSync(dir + '/most.txt', 'utf8'); } catch {}
  if (open > most) fs.writeFileSync(dir + '/most.txt', String(open));
  const file = dir + '/setup-' + (mode.includes('oldstyle') ? 'oldstyle' : '0.2.1') + '.exe';
  const whole = fs.statSync(file).size;
  const m = /^bytes=(\d+)-(\d+)$/.exec(req.headers.range || '');
  fs.appendFileSync(dir + '/requests.log', (req.headers.range || 'whole') + '\n');
  if (!m || mode === 'norange') {
    count(whole);
    res.writeHead(200, { 'Content-Length': whole, 'Content-Type': 'application/octet-stream' });
    return fs.createReadStream(file).pipe(res);
  }
  const from = +m[1], to = Math.min(+m[2], whole - 1), n = to - from + 1;
  const big = n > 100000;
  if (mode.includes('drop') && big && !dropped) {
    // Half of this part, then the connection goes: once.
    dropped = true;
    res.writeHead(206, { 'Content-Range': 'bytes ' + from + '-' + to + '/' + whole, 'Content-Length': n });
    const half = Math.floor(n / 2); count(half);
    const s = fs.createReadStream(file, { start: from, end: from + half - 1 });
    s.on('data', d => res.write(d)); s.on('end', () => setTimeout(() => req.socket.destroy(), 100));
    return;
  }
  count(n);
  res.writeHead(206, { 'Content-Range': 'bytes ' + from + '-' + to + '/' + whole, 'Content-Length': n, 'Content-Type': 'application/octet-stream' });
  if (mode === 'damage' && big) {
    const buffer = Buffer.alloc(n), fd = fs.openSync(file, 'r'); fs.readSync(fd, buffer, 0, n, from); fs.closeSync(fd);
    buffer[Math.floor(n / 2)] ^= 0x55;
    return res.end(buffer);
  }
  if (mode.includes('throttle')) {
    // Each connection held to about 2 MB/s, as GitHub's store does at a busy time (140 KB/s each there, however many).
    // rate.txt: bytes a second over everything since this mode began, and the best one connection did on its own.
    const s = fs.createReadStream(file, { start: from, end: to, highWaterMark: 64 * 1024 }), began = Date.now();
    s.on('data', d => { s.pause(); res.write(d); sent += d.length; setTimeout(() => s.resume(), 32); });
    s.on('end', () => {
      res.end();
      if (n > 1000000) best = Math.max(best, n / ((Date.now() - began) / 1000));
      fs.writeFileSync(dir + '/rate.txt', Math.round(sent / ((Date.now() - t0) / 1000)) + ' ' + Math.round(best));
    });
    return;
  }
  if (mode === 'slow') {
    // A trickle, so the test can pull the plug part way.
    const s = fs.createReadStream(file, { start: from, end: to, highWaterMark: 64 * 1024 });
    s.on('data', d => { s.pause(); res.write(d); setTimeout(() => s.resume(), 700); }); s.on('end', () => res.end()); return;
  }
  return fs.createReadStream(file, { start: from, end: to }).pipe(res);
}).listen($port, '127.0.0.1');
"@)
# A real old-style Setup, from 0.6.0's source: the game zipped into it, no index on the end.
$oldSetupSource = Join-Path $out 'OldSetup.cs'
[System.IO.File]::WriteAllLines($oldSetupSource, @(git -C $repo show 'v0.6.0:Tools/installer/WildreachSetup.cs'))
Add-Type -AssemblyName System.IO.Compression.FileSystem
$oldZip = Join-Path $out 'game.zip'
[System.IO.Compression.ZipFile]::CreateFromDirectory("$out\build-0.2.1\Windows", $oldZip, [System.IO.Compression.CompressionLevel]::Fastest, $false)
& $csc /nologo /target:winexe /platform:x64 /optimize "/out:$out\setup-oldstyle.exe" "/resource:$oldZip,game.zip" `
	"/r:$fx\System.IO.Compression.dll" "/r:$fx\System.IO.Compression.FileSystem.dll" "/r:$fx\System.Windows.Forms.dll" `
	"/r:$fx\System.Drawing.dll" "/r:$fx\Microsoft.CSharp.dll" $oldSetupSource | Out-Null
Remove-Item $oldZip

$node = Start-Process node -ArgumentList "`"$server`" `"$($out -replace '\\','/')`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

$install = Join-Path $out 'install'
$feed = "http://127.0.0.1:$port"
function Mode([string]$m) { [System.IO.File]::WriteAllText($modeFile, $m); Remove-Item "$out\served.txt", "$out\requests.log", "$out\most.txt", "$out\rate.txt" -ErrorAction SilentlyContinue }
function Served { if (Test-Path "$out\served.txt") { [long](Get-Content "$out\served.txt") } else { 0L } }
function MostAtOnce { if (Test-Path "$out\most.txt") { [int](Get-Content "$out\most.txt") } else { 0 } }
function Parts { if ((LogText) -match '(fetching|in) (\d+) parts over (\d+) connections') { $Matches[0] } else { 'no parts line' } }
function WholeDownloads { @(Get-Content "$out\requests.log" -ErrorAction SilentlyContinue | Where-Object { $_ -eq 'whole' }).Count }
function Updater([string]$extra, [string]$feedUrl = $feed, [string]$exe = 'WildreachUpdater.exe') {
	$p = Start-Process (Join-Path $install $exe) -ArgumentList "--feed $feedUrl --quiet $extra" -PassThru
	if (-not $p.WaitForExit(180000)) { Stop-Process -Id $p.Id -Force; return $false }
	Start-Sleep -Seconds 2   # the game it started writes its note
	return $true
}
function Launches { if (Test-Path "$install\launched.txt") { @(Get-Content "$install\launched.txt") } else { @() } }
function LogText { if (Test-Path "$install\updater.log") { Get-Content "$install\updater.log" -Raw } else { '' } }
# Fresh 0.2.0, with a player's save and something of their own beside the game.
function FreshInstall {
	Get-ChildItem (Join-Path $env:TEMP 'Wildreach-Update') -Filter '*v0.2.1*' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
	if (Test-Path $install) { Remove-Item -Recurse -Force $install }
	$p = Start-Process (Join-Path $out 'setup-0.2.0.exe') -ArgumentList "--silent --nolaunch --noshortcuts --dir `"$install`"" -PassThru -Wait
	if ($p.ExitCode -ne 0) { throw "0.2.0 didn't install ($($p.ExitCode))" }
	New-Item -ItemType Directory -Force "$install\Wildreach\Saved\Worlds\Island" | Out-Null
	[System.IO.File]::WriteAllText("$install\Wildreach\Saved\Worlds\Island\world.json", 'my world')
	[System.IO.File]::WriteAllText("$install\my notes.txt", 'mine')
}
# The folder is exactly a fresh install of 0.2.1: every file of the release there with the right bytes, and what was dropped gone.
$fresh = Join-Path $out 'fresh'
$p = Start-Process (Join-Path $out 'setup-0.2.1.exe') -ArgumentList "--silent --nolaunch --noshortcuts --dir `"$fresh`"" -PassThru -Wait
function Hashes([string]$root) {
	$h = @{}
	Get-ChildItem $root -Recurse -File | ForEach-Object { $h[$_.FullName.Substring($root.Length + 1)] = (Get-FileHash $_.FullName -Algorithm MD5).Hash }
	$h
}
$freshHashes = Hashes $fresh
function SameAsFresh {
	$now = Hashes $install
	$wrong = @()
	foreach ($k in $freshHashes.Keys) { if ($k -ne 'Uninstall Wildreach.cmd' -and $now[$k] -ne $freshHashes[$k]) { $wrong += $k } }
	if (Test-Path "$install\Wildreach\Data\dropped.json") { $wrong += 'dropped.json still there' }
	if ((Get-Content "$install\version.txt") -ne '0.2.1') { $wrong += 'version.txt' }
	$wrong -join ', '
}
function Kept { (Get-Content "$install\Wildreach\Saved\Worlds\Island\world.json") -eq 'my world' -and (Get-Content "$install\my notes.txt") -eq 'mine' }

try {
	# ---- a fresh install from the new-style Setup
	FreshInstall
	Check '0.2.0 installs silently with the updater and its version' ((Test-Path "$install\WildreachUpdater.exe") -and (Get-Content "$install\version.txt") -eq '0.2.0')
	$source = Hashes "$out\build-0.2.0\Windows"; $now = Hashes $install
	$bad = @($source.Keys | Where-Object { $now[$_] -ne $source[$_] })
	Check 'every file unpacked exactly as packed' ($bad.Count -eq 0) ("{0} files, wrong: {1}" -f $source.Count, ($bad -join ', '))
	$damaged = "$out\setup-damaged.exe"
	Copy-Item "$out\setup-0.2.0.exe" $damaged
	$f = [IO.File]::Open($damaged, 'Open', 'ReadWrite'); $f.Seek(-20MB, 'End') | Out-Null; $b = $f.ReadByte(); $f.Seek(-1, 'Current') | Out-Null; $f.WriteByte($b -bxor 0x55); $f.Close()
	$p = Start-Process $damaged -ArgumentList "--silent --nolaunch --noshortcuts --dir `"$out\damaged`"" -PassThru -Wait
	Check 'a damaged Setup says so instead of installing it' ($p.ExitCode -ne 0) "exit $($p.ExitCode)"

	# ---- the small update
	FreshInstall; Mode 'normal'
	$ok = Updater '-hello'
	$served = Served
	Check 'the small update brings the folder to exactly 0.2.1' ($ok -and (SameAsFresh) -eq '') "wrong: $(SameAsFresh)"
	$smallSize = $served
	Check 'it fetched a small part of the Setup' ($served -lt $setupSize * 0.1 -and $served -gt 0) ("{0:N0} KB of {1:N0} KB" -f ($served / 1KB), ($setupSize / 1KB))
	Check 'without ever asking for the whole file' ((WholeDownloads) -eq 0) ((LogText).Trim())
	Check 'on no more than 4 connections at once' ((MostAtOnce) -ge 1 -and (MostAtOnce) -le 4) ("most at once {0}; {1}" -f (MostAtOnce), (Parts))
	Check 'saves and a player''s own files are untouched' (Kept)
	Check 'it set its running self aside to be replaced' (Test-Path "$install\WildreachUpdater.exe.old")
	Check 'it left no working files behind' (-not (Test-Path "$install\.update"))
	Check 'then started the game with the shortcut''s arguments' (@(Launches).Count -eq 1 -and @(Launches)[0] -eq 'launched -hello') ((Launches) -join ' / ')

	Mode 'normal'
	New-Item -ItemType Directory -Force (Join-Path $env:TEMP 'Wildreach-Update') | Out-Null
	[System.IO.File]::WriteAllText((Join-Path $env:TEMP 'Wildreach-Update\Wildreach-Setup-v0.0.1.exe'), 'left by an earlier update')
	$ok = Updater ''
	Check 'up to date: it starts the game without downloading anything' ($ok -and @(Launches).Count -eq 2 -and (Served) -eq 0) ("{0} launches, {1} bytes" -f @(Launches).Count, (Served))
	Check 'and cleared a whole Setup an earlier update left behind' (-not (Test-Path (Join-Path $env:TEMP 'Wildreach-Update\Wildreach-Setup-v0.0.1.exe')))
	Check 'and cleared the old copy of itself' (-not (Test-Path "$install\WildreachUpdater.exe.old"))

	# ---- when the small way can't be done: the whole Setup, the same result
	FreshInstall; Mode 'oldstyle'
	$ok = Updater ''
	$oldSize = (Get-Item "$out\setup-oldstyle.exe").Length
	Check 'a Setup with no index: the whole Setup instead' ($ok -and (Get-Content "$install\version.txt") -eq '0.2.1' -and (Get-Content "$install\release.txt") -eq 'this is 0.2.1' -and (Served) -ge $oldSize -and (Kept) -and @(Launches).Count -eq 1) ((LogText).Trim())
	Check '...in parts, several at once (no more than 4)' ((WholeDownloads) -eq 0 -and (MostAtOnce) -ge 2 -and (MostAtOnce) -le 4) ("most at once {0}; {1}; {2} whole downloads" -f (MostAtOnce), (Parts), (WholeDownloads))
	FreshInstall; Mode 'oldstyle drop'
	$ok = Updater ''
	Check '...and a dropped connection there picks up too' ($ok -and (Get-Content "$install\version.txt") -eq '0.2.1' -and (Get-Content "$install\release.txt") -eq 'this is 0.2.1' -and (WholeDownloads) -eq 0 -and (LogText) -match 'download interrupted') ((LogText).Trim())
	# Each connection held back, as GitHub is at a busy time: all of them together must be well over what one gets.
	FreshInstall; Mode 'oldstyle throttle'
	$ok = Updater ''
	$rate = if (Test-Path "$out\rate.txt") { (Get-Content "$out\rate.txt").Split(' ') | ForEach-Object { [double]$_ } } else { 0, 1 }
	Check 'each connection held back: together over 2.5 times as fast as one' ($ok -and (Get-Content "$install\version.txt") -eq '0.2.1' -and $rate[0] -ge 2.5 * $rate[1]) ("{0:N0} KB/s in all, one connection at most {1:N0} KB/s" -f ($rate[0] / 1KB), ($rate[1] / 1KB))
	foreach ($case in @(
		@{ mode = 'norange'; what = 'a server without partial downloads' },
		@{ mode = 'damage'; what = 'a block that comes down damaged' })) {
		FreshInstall; Mode $case.mode
		$ok = Updater ''
		$fine = $ok -and (SameAsFresh) -eq '' -and (WholeDownloads) -ge 1 -and (Kept) -and @(Launches).Count -eq 1
		Check "$($case.what): the whole Setup instead, the same result" $fine ("wrong: $(SameAsFresh), {0} whole downloads" -f (WholeDownloads))
		Check '...with nothing left over' (-not (Test-Path "$install\.update") -and -not (Get-ChildItem (Join-Path $env:TEMP 'Wildreach-Update') -Filter '*v0.2.1*' -ErrorAction SilentlyContinue))
	}

	# ---- a dropped connection picks up where it was
	FreshInstall; Mode 'drop'
	$ok = Updater ''
	Check 'a dropped connection picks up, no whole download' ($ok -and (SameAsFresh) -eq '' -and (WholeDownloads) -eq 0 -and (LogText) -match 'download interrupted') "wrong: $(SameAsFresh)"

	# ---- pulled out part way (the PC turned off, the window killed): the game's as it was, and next time finishes
	FreshInstall; Mode 'slow'
	$p = Start-Process (Join-Path $install 'WildreachUpdater.exe') -ArgumentList "--feed $feed --quiet --nolaunch" -PassThru
	$deadline = (Get-Date).AddSeconds(30)
	while ((Get-Date) -lt $deadline -and -not (Test-Path "$install\.update\blocks")) { Start-Sleep -Milliseconds 200 }
	$deadline = (Get-Date).AddSeconds(15)
	while ((Get-Date) -lt $deadline -and @(Get-ChildItem "$install\.update\blocks" -ErrorAction SilentlyContinue).Count -lt 1) { Start-Sleep -Milliseconds 50 }
	if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
	$p.WaitForExit()
	Start-Sleep -Milliseconds 500
	$kept = @(Get-ChildItem "$install\.update\blocks" -ErrorAction SilentlyContinue).Count
	Check 'killed part way: still the old version, its files untouched' ((Get-Content "$install\version.txt") -eq '0.2.0' -and (Get-Content "$install\release.txt") -eq 'this is 0.2.0' -and $kept -ge 1) "$kept blocks fetched"
	$firstTry = Served
	Mode 'normal'
	$ok = Updater '--nolaunch'
	Check 'the next start finishes it' ($ok -and (SameAsFresh) -eq '' -and (WholeDownloads) -eq 0) "wrong: $(SameAsFresh)"
	Check '...without fetching what it already had' ((Served) -lt $smallSize -and $kept -ge 1) ("{0:N0} bytes this time; {1:N0} from scratch, {2} blocks kept" -f (Served), $smallSize, $kept)

	# ---- killed in the middle of swapping files: the version number goes last, so it starts again and comes right
	FreshInstall; Mode 'normal'
	[System.IO.File]::WriteAllText("$install\release.txt", 'this is 0.2.1')   # as if this one had already been replaced
	Copy-Item "$out\build-0.2.1\Windows\Wildreach\Data\item7.json" "$install\Wildreach\Data\item7.json" -Force
	$ok = Updater '--nolaunch'
	Check 'half swapped: the next start finishes the rest' ($ok -and (SameAsFresh) -eq '') "wrong: $(SameAsFresh)"

	# ---- a file in use: all put back, the game starts anyway, and next time it works
	FreshInstall; Mode 'normal'
	$before = Hashes $install
	$lock = [IO.File]::Open("$install\release.txt", 'Open', 'ReadWrite', 'None')
	try { $ok = Updater '-locked' } finally { $lock.Close() }
	$now = Hashes $install
	$changed = @($before.Keys | Where-Object { $_ -ne 'updater.log' -and $now[$_] -ne $before[$_] })
	Check 'a file in use: everything is put back' ($ok -and $changed.Count -eq 0 -and (Get-Content "$install\version.txt") -eq '0.2.0') ("changed: " + ($changed -join ', '))
	Check '...it says so, without downloading the whole game for nothing' ((LogText) -match 'in use' -and (WholeDownloads) -eq 0)
	Check '...and the game still starts' (@(Launches) -contains 'launched -locked') ((Launches) -join ' / ')
	$ok = Updater '--nolaunch'
	Check '...and next time it works' ($ok -and (SameAsFresh) -eq '') "wrong: $(SameAsFresh)"

	# ---- the updater every player has today, from v0.6.0: it knows nothing of blocks, and must still install a new Setup
	$oldSource = Join-Path $out 'OldUpdater.cs'
	[System.IO.File]::WriteAllLines($oldSource, @(git -C $repo show 'v0.6.0:Tools/installer/WildreachUpdater.cs'))
	& $csc /nologo /target:winexe /optimize "/out:$out\OldUpdater.exe" "/r:$fx\System.Windows.Forms.dll" "/r:$fx\System.Drawing.dll" $oldSource | Out-Null
	FreshInstall; Mode 'normal'
	Copy-Item "$out\OldUpdater.exe" "$install\WildreachUpdater.exe" -Force
	$p = Start-Process "$install\WildreachUpdater.exe" -ArgumentList "--feed $feed --nolaunch" -PassThru
	$ok = $p.WaitForExit(180000)
	Check 'the updater from 0.6.0 still installs a new Setup' ($ok -and (SameAsFresh) -eq '' -and (WholeDownloads) -eq 1) "wrong: $(SameAsFresh)"
	Check '...and from then on it''s the new updater' ((Get-FileHash "$install\WildreachUpdater.exe").Hash -eq (Get-FileHash "$out\build-0.2.1\Windows\WildreachUpdater.exe").Hash)

	# ---- nothing to do
	FreshInstall; Mode 'normal'
	[System.IO.File]::WriteAllText($tagFile, 'playtest-2026-10-25')
	$ok = Updater ''
	Check 'a release tag that isn''t a version is never an update' ($ok -and @(Launches).Count -eq 1 -and (Served) -eq 0 -and (Get-Content "$install\version.txt") -eq '0.2.0')
	$clock = [Diagnostics.Stopwatch]::StartNew()
	$ok = Updater '' 'http://127.0.0.1:1'
	Check 'offline: it starts the game anyway, quickly' ($ok -and @(Launches).Count -eq 2 -and $clock.Elapsed.TotalSeconds -lt 15) ("{0:N1} s" -f $clock.Elapsed.TotalSeconds)
}
finally {
	if (-not $node.HasExited) { Stop-Process -Id $node.Id -Force }
}

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail.Trim()))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (files: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
