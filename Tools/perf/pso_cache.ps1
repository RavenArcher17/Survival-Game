# The bundled shader pipeline (PSO) cache: record it from play, expand it, commit it (cooks/cookpass-performance.md 3.5,
# Docs/PERFORMANCE.md "PSO cache"). Each player's machine also keeps its own (r.ShaderPipelineCache.SaveUserCache=1,
# Config/DefaultEngine.ini); this bundled one covers the very first launch.
#
#   1. Tools\package.ps1                       (the cook writes the stable keys: NeedsShaderStableKeys=true)
#   2. Run the packaged game with -logPSO and play the route below; quit from the menu (the file is written on exit).
#   3. Tools\perf\pso_cache.ps1 -Build <the package folder>   (collects the recordings and the keys, expands, copies)
#   4. Commit Build\Windows\PipelineCaches\*.spc (small), package again: the cook bundles it.
#
# Route to record (every material and effect the game draws should be on screen once): the main menu and its look
# picker; host; the north camp; walk to the Basaltback herd; a fight with fire and frost abilities; a cave; build mode
# (each piece type); the B / P / inventory / map screens; die once; the tundra east camp; each land you can reach.
# Re-record when materials or effects change a lot (a stale cache only misses PSOs, it never breaks anything).
param(
	[Parameter(Mandatory = $true)] [string]$Build,                  # the packaged game's folder (holds Wildreach\Saved)
	[string]$Engine = '',                                           # engine root; default D:\UnrealEngine\UE_5.8, else C:\Program Files\Epic Games\UE_5.8
	[string]$Cooked = ''                                            # the cook's Metadata\PipelineCaches folder (default: the project's Saved\Cooked\Windows\Wildreach\Metadata\PipelineCaches)
)
$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
if (-not $Engine) {
	$Engine = @('D:\UnrealEngine\UE_5.8', 'C:\Program Files\Epic Games\UE_5.8') | Where-Object { Test-Path (Join-Path $_ 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') } | Select-Object -First 1
}
$cmd = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path $cmd)) { throw "No UnrealEditor-Cmd.exe under '$Engine' (pass -Engine)" }
if (-not $Cooked) { $Cooked = Join-Path $repo 'Saved\Cooked\Windows\Wildreach\Metadata\PipelineCaches' }

# One heavy Unreal job at a time on these machines.
$busy = Get-Process UnrealEditor, UnrealEditor-Cmd, UnrealBuildTool -ErrorAction SilentlyContinue
if ($busy) { throw "An Unreal process is running ($($busy.Name -join ', ')): wait for it to finish" }

$recs = @(Get-ChildItem -Path $Build -Recurse -Filter '*.rec.upipelinecache' -ErrorAction SilentlyContinue)
if ($recs.Count -eq 0) { throw "No *.rec.upipelinecache under $Build: run the packaged game with -logPSO and quit from the menu first" }
$keys = @(Get-ChildItem -Path $Cooked -Filter '*.shk' -ErrorAction SilentlyContinue)
if ($keys.Count -eq 0) { throw "No *.shk under $Cooked: package once with NeedsShaderStableKeys=true (Config/DefaultEngine.ini)" }

$work = Join-Path $repo '.lane-tmp\pso'
New-Item -ItemType Directory -Force $work | Out-Null
Get-ChildItem $work -File | Remove-Item -Force
$recs | ForEach-Object { Copy-Item $_.FullName $work }
$keys | ForEach-Object { Copy-Item $_.FullName $work }
Write-Host "PSO: $($recs.Count) recording(s), $($keys.Count) key file(s)"

$dest = Join-Path $repo 'Build\Windows\PipelineCaches'
New-Item -ItemType Directory -Force $dest | Out-Null
foreach ($sm in @('SM6', 'SM5')) {
	$smKeys = @($keys | Where-Object { $_.Name -match "PCD3D_$sm" })
	if ($smKeys.Count -eq 0) { Write-Host "PSO: no PCD3D_$sm keys, skipped"; continue }
	$out = Join-Path $dest "Wildreach_PCD3D_$sm.spc"
	$toolArgs = @("$repo\Wildreach.uproject", '-run=ShaderPipelineCacheTools', 'expand', "$work\*.rec.upipelinecache", "$work\*PCD3D_$sm*.shk", $out, '-unattended', '-nopause')
	& $cmd @toolArgs
	if ($LASTEXITCODE -ne 0 -or -not (Test-Path $out)) { throw "ShaderPipelineCacheTools expand failed for $sm (exit $LASTEXITCODE)" }
	Write-Host ("PSO: {0} ({1:N0} KB)" -f $out, ((Get-Item $out).Length / 1KB))
}
Write-Host 'PSO: commit Build\Windows\PipelineCaches\*.spc and package again.'
