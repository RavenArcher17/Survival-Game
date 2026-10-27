# Frame-time benchmark on L_Island. Nothing had measured the game's frame rate before the polish pass, which adds
# animation, sound, effects and world detail; run this before and after a change and compare the rows it adds to
# Docs/PERFORMANCE.md (what each number means is there too).
#
# Client runs: a dedicated server and a rendered client (1920x1080 windowed, frame rate uncapped, VSync off) that bot
# commands drive along a fixed route, the walk test's WalkA: 20 s looking around the north camp (WalkA's start), 90 s
# walking east along WalkA's route (about 400 m, past a Basaltback herd and the packs that come out on the way), then a
# long move to the tundra's east camp (25 s: terrain streaming in after a 2.5 km jump). The client's CSV profiler
# records every frame of those 135 s. Each level runs -Runs times, interleaved, to show how much the numbers move.
# Server runs: the server with a headless player walking the same stretch, once with the usual wildlife (a pack comes
# out within 250 m of a player) and once with every island pack out (wildlife.activationRadius past the island's size:
# all 169 creatures, as on a busy server with players all over), timing the server's work per frame.
# Other agents' builds and renders share this machine, so every run records the load around it: CPU used by other
# programs (sampled during the run), GPU used by other programs (the client's own CSV) and other Unreal processes.
# Repeat runs when it's quiet and read the best consistent ones.
#   benchmark.ps1                         Epic + Medium, 2 runs each, then both server runs; appends to Docs/PERFORMANCE.md
#   benchmark.ps1 -Levels epic -Runs 1    one quick run
#   benchmark.ps1 -Label "streaming"      names the rows (what changed)
#   benchmark.ps1 -NoServer | -NoClient   skip a part;  -NoAppend prints only;  -Warmup adds an unrecorded run per level first
#   benchmark.ps1 -Exe <packaged Wildreach.exe>   the client from a packaged build instead of the editor binary
#   benchmark.ps1 -Tune significance.enabled:0     tuning overrides for every server (A/B comparisons)
param(
	[string[]]$Levels = @('epic', 'medium'),
	[int]$Runs = 2,
	[int]$Port = 7866,
	[string]$Label = '',
	[switch]$NoServer,
	[switch]$NoClient,
	[switch]$NoAppend,
	[switch]$Warmup,
	[switch]$NoSound,
	[string]$Exe = '',
	[string]$Tune = ''
)
# Unreal otherwise starts with UnrealBuildTool's platform check, which waits for any build running on the machine.
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$out = Join-Path $env:TEMP ("wildreach-bench-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$csvDir = Join-Path $root 'Saved\Profiling\CSV'
if ($Levels.Count -eq 1) { $Levels = @($Levels[0] -split ',') }
$levelIndex = @{ low = 0; medium = 1; high = 2; epic = 3; cinematic = 4 }
$commit = (& git -C $root rev-parse --short HEAD 2>$null)
if (@(& git -C $root status --porcelain --untracked-files=no 2>$null).Count -gt 0) { $commit = "$commit+changes" }

# The route (the walk test's WalkA): the north camp, 400 m east along its line to the pass, then the tundra's east camp.
$camp = '-100400 -129600'
$walkTo = '-60400 -129600'
$far = '136800 51400'
$phases = 'camp:0:20,walk:20:110,move:110:135'

function Stamp([string]$line)
{
	# Unreal log lines start [2026.10.26-23.24.33:952] (UTC).
	if ($line -match '^\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]')
	{
		return (Get-Date -Year $Matches[1] -Month $Matches[2] -Day $Matches[3] -Hour $Matches[4] -Minute $Matches[5] -Second $Matches[6] -Millisecond $Matches[7])
	}
	return $null
}
function FirstLine([string]$path, [string]$pattern)
{
	if (-not (Test-Path $path)) { return $null }
	Select-String -Path $path -Pattern $pattern | Select-Object -First 1 | ForEach-Object { $_.Line }
}
function WaitFor([string]$path, [string]$pattern, [int]$seconds, $proc)
{
	$deadline = (Get-Date).AddSeconds($seconds)
	while ((Get-Date) -lt $deadline)
	{
		if (FirstLine $path $pattern) { return $true }
		if ($proc -and $proc.HasExited) { return $false }
		Start-Sleep -Seconds 2
	}
	return $false
}
# What else is running: other agents' editors, games, shader compiles and C++ builds.
function OtherProcs([int[]]$mine)
{
	$p = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $mine -notcontains $_.Id })
	$ue = @($p | Where-Object { $_.ProcessName -match '^UnrealEditor' }).Count
	$game = @($p | Where-Object { $_.ProcessName -eq 'Wildreach' }).Count
	$scw = @($p | Where-Object { $_.ProcessName -eq 'ShaderCompileWorker' }).Count
	$cl = @($p | Where-Object { $_.ProcessName -match '^(cl|link|UnrealBuildTool)$' }).Count
	$py = @($p | Where-Object { $_.ProcessName -match '^(python|blender)' }).Count
	"other: $ue UE, $game game, $scw SCW, $cl build, $py py"
}
# Samples the machine every few seconds while a run goes: CPU used by everything except this run's processes, and the
# GPU's 3D engine use per process (Windows' GPU Engine counters), so a busy GPU elsewhere shows up next to the numbers.
$cores = [Environment]::ProcessorCount
function LoadSample($procs)
{
	$total = $null
	$gpu = @{}
	try
	{
		$set = (Get-Counter '\Processor(_Total)\% Processor Time', '\GPU Engine(*engtype_3D)\Utilization Percentage' -ErrorAction Stop).CounterSamples
		foreach ($x in $set)
		{
			if ($x.Path -match 'processor\(_total\)') { $total = $x.CookedValue }
			elseif ($x.InstanceName -match 'pid_(\d+)_') { $gpu[[int]$Matches[1]] = $gpu[[int]$Matches[1]] + $x.CookedValue }
		}
	}
	catch { }
	$cpu = 0.0
	foreach ($p in $procs) { try { $p.Refresh(); $cpu += $p.TotalProcessorTime.TotalSeconds } catch { } }
	$ids = @($procs | ForEach-Object { $_.Id })
	$others = @{}
	foreach ($k in $gpu.Keys) { if ($ids -notcontains $k -and $gpu[$k] -ge 1) { $others[$k] = $gpu[$k] } }
	[pscustomobject]@{ At = Get-Date; Total = $total; Mine = $cpu; GpuMine = (($ids | ForEach-Object { $gpu[$_] } | Measure-Object -Sum).Sum); GpuOthers = $others }
}
function OtherCpu($samples)
{
	# Average total CPU minus this run's own share between consecutive samples.
	$vals = @()
	for ($i = 1; $i -lt $samples.Count; $i++)
	{
		$dt = ($samples[$i].At - $samples[$i - 1].At).TotalSeconds
		if ($dt -le 0 -or $null -eq $samples[$i].Total) { continue }
		$mine = 100.0 * ($samples[$i].Mine - $samples[$i - 1].Mine) / ($dt * $cores)
		$vals += [Math]::Max(0.0, $samples[$i].Total - $mine)
	}
	if ($vals.Count -eq 0) { return $null }
	[Math]::Round(($vals | Measure-Object -Average).Average)
}
function OtherGpu($samples)
{
	# Average GPU use of other programs, and the busiest of them by name.
	if ($samples.Count -eq 0) { return 'GPU others ?' }
	$byPid = @{}
	$sum = 0.0
	foreach ($x in $samples) { foreach ($k in $x.GpuOthers.Keys) { $byPid[$k] = $byPid[$k] + $x.GpuOthers[$k] / $samples.Count; $sum += $x.GpuOthers[$k] / $samples.Count } }
	$mine = ($samples | ForEach-Object { $_.GpuMine } | Measure-Object -Average).Average
	$top = @($byPid.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 3 | Where-Object { $_.Value -ge 3 } | ForEach-Object {
		$n = (Get-Process -Id $_.Key -ErrorAction SilentlyContinue).ProcessName; if (-not $n) { $n = "pid $($_.Key)" }; "$n $([Math]::Round($_.Value))" })
	"GPU others {0:N0}%{1} (this run {2:N0}%)" -f $sum, $(if ($top) { ' (' + ($top -join ', ') + ')' } else { '' }), $mine
}
function Analyse([string]$csv, [string]$extra)
{
	$json = & node (Join-Path $PSScriptRoot 'benchmark_csv.mjs') $csv $extra.Split(' ', [StringSplitOptions]::RemoveEmptyEntries)
	if ($LASTEXITCODE -ne 0 -or -not $json) { return $null }
	$json | ConvertFrom-Json
}
function StartServer([string]$world, [string]$extra)
{
	Remove-Item (Join-Path $root "Saved\ServerDB\world_$world.json") -ErrorAction SilentlyContinue
	$log = "$out\$world-server.log"
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=$world $extra -abslog=`"$log`"" -PassThru -WindowStyle Hidden
	if (-not (WaitFor $log 'LoadMap\(/Game/Maps/L_Island\)' 180 $s)) { Write-Warning "server $world did not come up (log: $log)" }
	$s
}
function Stop([object[]]$procs)
{
	foreach ($p in $procs) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } }
	Start-Sleep -Seconds 2
}

# ---------------------------------------------------------------- client runs
$clientRows = @()
$order = @()
if (-not $NoClient)
{
	# A warm-up run per level first fills the shader cache (the editor binary compiles missing shaders on first sight).
	if ($Warmup) { foreach ($l in $Levels) { $order += [pscustomobject]@{ Level = $l.ToLower(); Run = 0 } } }
	for ($r = 1; $r -le $Runs; $r++) { foreach ($l in $Levels) { $order += [pscustomobject]@{ Level = $l.ToLower(); Run = $r } } }
}
# One rendered capture at a time machine-wide (uishots takes the same lock): two would share the GPU and skew both.
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
foreach ($o in $order)
{
	$tag = "$($o.Level)$($o.Run)"
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
	$name = 'Bench'
	Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
	$csvName = "bench_${tag}_$(Get-Date -Format 'HHmmss').csv"
	$before = OtherProcs @()
	$s = StartServer 'bench' $(if ($Tune) { "-wrtune=$Tune" } else { '' })
	$cmds = @('WRBotPause 1', "scalability $($levelIndex[$o.Level])", 'r.VSync 0', 't.MaxFPS 0',
		"WRTeleport $camp", 'WRLook 0 -8', 'wait 15', 'WRHeal',
		"csvprofile startfile=$csvName", 'csvprofile start',
		'WRLook 0 -8', 'wait 5', 'WRLook 90 -8', 'wait 5', 'WRLook 180 -8', 'wait 5', 'WRLook 270 -8', 'wait 5',
		"WRWalkTo $walkTo", 'wait 90',
		'WRHeal', "WRTeleport $far", "WRWalkTo $far", 'wait 1', 'WRLook 200 -8', 'wait 24',
		'csvprofile stop', 'wait 8', 'quit') -join '|'
	$sound = if ($NoSound) { '-nosound' } else { '' }
	$clientTune = if ($Tune) { "-wrtune=$Tune" } else { '' }
	$clientArgs = "127.0.0.1:${Port}?Path=fire?WRName=$name -game -windowed -ResX=1920 -ResY=1080 -unattended -nosplash $sound $clientTune -wrbot `"-ExecCmds=t.MaxFPS 0`" `"-wrbotcmds=$cmds`" -abslog=`"$out\$tag-client.log`""
	$c = if ($Exe) { Start-Process $Exe -ArgumentList $clientArgs -PassThru } else { Start-Process $ue -ArgumentList "`"$project`" $clientArgs" -PassThru }
	$samples = @()
	$peak = 0
	$deadline = (Get-Date).AddSeconds(420)
	while ((Get-Date) -lt $deadline -and -not $c.HasExited)
	{
		$samples += LoadSample @($c, $s)
		try { $c.Refresh(); $peak = [Math]::Max($peak, $c.PrivateMemorySize64) } catch { }
		Start-Sleep -Seconds 4
	}
	$during = OtherProcs @($c.Id, $s.Id)
	Stop @($c, $s)
	$lock.ReleaseMutex()
	$csvFile = Get-ChildItem $csvDir -Filter $csvName -ErrorAction SilentlyContinue | Select-Object -First 1
	if (-not $csvFile)
	{
		Write-Warning "$tag : no CSV (client log: $out\$tag-client.log)"
		continue
	}
	Move-Item $csvFile.FullName "$out\$csvName" -Force
	$a = Analyse "$out\$csvName" "--phases $phases"
	$clog = "$out\$tag-client.log"
	$maxIter = @(Select-String -Path $clog -Pattern 'Max iterations' -ErrorAction SilentlyContinue).Count
	$slow = @(Select-String -Path $clog -Pattern 'Streaming performance changed: .* -> (Slow|Critical)' -ErrorAction SilentlyContinue).Count
	if ($o.Run -eq 0) { "warm-up run done ($($a.all.avg) ms average)"; continue }
	$clientRows += [pscustomobject]@{ Level = $o.Level; Run = $o.Run; A = $a; CpuOther = (OtherCpu $samples); GpuOther = (OtherGpu $samples); Procs = $during; PeakMB = [Math]::Round($peak / 1MB); MaxIter = $maxIter; Slow = $slow; Csv = "$out\$csvName" }
}

# ---------------------------------------------------------------- server runs
$serverRows = @()
$scenarios = @()
if (-not $NoServer)
{
	$scenarios += [pscustomobject]@{ Name = 'usual'; What = 'usual wildlife (packs within 250 m)'; Tune = $Tune }
	$scenarios += [pscustomobject]@{ Name = 'all'; What = 'every pack out (169 creatures)'; Tune = (@('wildlife.activationRadius:10000000', $Tune) | Where-Object { $_ }) -join ',' }
}
foreach ($sc in $scenarios)
{
	$name = 'BenchServer'
	Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
	$csvName = "bench_server_$($sc.Name)_$(Get-Date -Format 'HHmmss').csv"
	# A dedicated server can't be told "csvprofile stop" from outside, so it captures a fixed number of frames (it runs
	# at 30 Hz) from boot and the window the player was on the route is cut out of that by the log's timestamps.
	$s = StartServer "bench$($sc.Name)" "$(if ($sc.Tune) { '-wrtune=' + $sc.Tune } else { '' }) `"-ExecCmds=csvprofile startfile=$csvName,csvprofile frames=5400`""
	$cmds = @('WRBotPause 1', 'wait 3', "WRTeleport $camp", 'wait 15', 'WRWildlife', "WRWalkTo $walkTo", 'wait 60', 'WRWildlife', 'wait 600') -join '|'
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=$name -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\server-$($sc.Name)-client.log`"" -PassThru -WindowStyle Hidden
	$slog = "$out\bench$($sc.Name)-server.log"
	$samples = @()
	$peak = 0
	$deadline = (Get-Date).AddSeconds(360)
	while ((Get-Date) -lt $deadline -and -not $s.HasExited)
	{
		$samples += LoadSample @($c, $s)
		try { $s.Refresh(); $peak = [Math]::Max($peak, $s.PrivateMemorySize64) } catch { }
		$done = @(Select-String -Path $slog -Pattern ': wildlife -> ' -ErrorAction SilentlyContinue).Count -ge 2
		$csvFile = Get-ChildItem $csvDir -Filter $csvName -ErrorAction SilentlyContinue | Select-Object -First 1
		# The capture is complete once its metadata row is written.
		if ($csvFile -and $csvFile.Length -gt 0 -and (Select-String -Path $csvFile.FullName -Pattern '^\[HasHeaderRowAtEnd\]' -Quiet)) { break }
		Start-Sleep -Seconds 4
	}
	$during = OtherProcs @($c.Id, $s.Id)
	Stop @($c, $s)
	$csvFile = Get-ChildItem $csvDir -Filter $csvName -ErrorAction SilentlyContinue | Select-Object -First 1
	$reports = @(Select-String -Path $slog -Pattern ': wildlife -> ' -ErrorAction SilentlyContinue)
	$start = Stamp (FirstLine $slog 'LogCsvProfiler: Display: Capture Starting')
	if (-not $csvFile -or $reports.Count -lt 2 -or -not $start)
	{
		Write-Warning "server $($sc.Name): no complete capture or wildlife reports (log: $slog)"
		continue
	}
	Move-Item $csvFile.FullName "$out\$csvName" -Force
	$from = ((Stamp $reports[0].Line) - $start).TotalSeconds
	$to = ((Stamp $reports[1].Line) - $start).TotalSeconds
	$a = Analyse "$out\$csvName" ("--from {0:F1} --to {1:F1}" -f $from, $to)
	$outCount = if ($reports[1].Line -match 'active (\d+), out (\d+)') { "$($Matches[2]) in $($Matches[1]) packs" } else { '?' }
	$maxIter = @(Select-String -Path $slog -Pattern 'Max iterations' -ErrorAction SilentlyContinue).Count
	$serverRows += [pscustomobject]@{ Scenario = $sc.What; Out = $outCount; A = $a; CpuOther = (OtherCpu $samples); GpuOther = (OtherGpu $samples); Procs = $during; PeakMB = [Math]::Round($peak / 1MB); MaxIter = $maxIter; Csv = "$out\$csvName" }
}

# ---------------------------------------------------------------- summary
$date = Get-Date -Format 'yyyy-MM-dd HH:mm'
$lbl = if ($Label) { $Label } else { $commit }
if ($Tune) { $lbl = "$lbl ($Tune)" }
function F($x, [string]$f = '{0:N1}') { if ($null -eq $x) { '-' } else { $f -f [double]$x } }
$clientLines = @()
foreach ($r in $clientRows)
{
	$x = $r.A.all
	$ph = "camp $(F $r.A.camp.avg)/$(F $r.A.camp.p99), walk $(F $r.A.walk.avg)/$(F $r.A.walk.p99), move $(F $r.A.move.avg)/$(F $r.A.move.p99) (streaming done $(F $r.A.move.streamSettle) s)"
	$load = "CPU others $(F $r.CpuOther '{0:N0}')%, $($r.GpuOther); $($r.Procs)"
	$clientLines += "| $date | $lbl | $($r.Level) | $($r.Run) | $(F $x.avg '{0:N2}') ($(F $x.fps '{0:N0}') fps) | $(F $x.p95) | $(F $x.p99) | $(F $x.game) | $(F $x.draw) | $(F $x.gpu) | $($x.hitch50) / $($x.hitch100) | $(F $x.max '{0:N0}') | $(F $x.gcMax) ($($x.gcFrames)) | $($x.memMB) MB | $ph | $($r.MaxIter) | $load |"
}
$serverLines = @()
foreach ($r in $serverRows)
{
	$x = $r.A.all
	$load = "CPU others $(F $r.CpuOther '{0:N0}')%; $($r.Procs)"
	$serverLines += "| $date | $lbl | $($r.Scenario) | $($r.Out) | $(F $x.work '{0:N2}') | $(F $x.workP95 '{0:N2}') | $(F $x.workP99 '{0:N2}') | $(F $x.workMax '{0:N0}') | $($x.hitch50) | $($x.top) | ticks/frame $(F $x.ticks.total '{0:N0}') (creature $(F $x.ticks.creature '{0:N0}'), movement $(F $x.ticks.cmc '{0:N0}'), spring arm $(F $x.ticks.springArm '{0:N0}')) | $($r.PeakMB) MB | $($r.MaxIter) | $load |"
}
''
'Client (frame ms: average (fps) | p95 | p99 | game | draw | GPU | hitches >50/>100 ms | worst | GC worst ms (frames) | memory | phases avg/p99 | Max-iterations warnings | load)'
$clientLines
''
'Server (game thread work ms per frame at 30 Hz: average | p95 | p99 | worst | frames >50 ms | top categories | ticks | memory | Max-iterations warnings | load)'
$serverLines
"logs and CSVs: $out"

if (-not $NoAppend -and ($clientLines.Count + $serverLines.Count) -gt 0)
{
	# New rows go at the end of each table (the marker comment follows the table's last row).
	$doc = Join-Path $root 'Docs\PERFORMANCE.md'
	$text = [System.IO.File]::ReadAllText($doc)
	if ($clientLines.Count) { $text = $text.Replace('<!-- client rows -->', (($clientLines -join "`n") + "`n<!-- client rows -->")) }
	if ($serverLines.Count) { $text = $text.Replace('<!-- server rows -->', (($serverLines -join "`n") + "`n<!-- server rows -->")) }
	[System.IO.File]::WriteAllText($doc, $text)
	"appended to $doc"
}
