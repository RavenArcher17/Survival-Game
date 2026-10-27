# Runs every Wildreach test, one after another, and prints a summary: the automation tests (Tools\test.ps1), then each
# bot test (a headless server + scripted client). A bot test passes when it exits 0 and ends with its "N checks, M failed"
# line; one that prints no checks is reported as RAN, not passed. Each test's full output goes to the run folder.
#   alltests.ps1                     build, then everything (about 40 minutes)
#   alltests.ps1 -SkipBuild          use the current build
#   alltests.ps1 -Only ranged,tame   just those (names as in the table: unit, mp, persist, craft, build, survival, house,
#                                    sky, tame, boss, social, menus, flow, spell, ranged, ai, wildlife, version, updater, walk, hazard)
param([switch]$SkipBuild, [string[]]$Only = @())
# Every Unreal start otherwise runs UnrealBuildTool's platform check, which waits for any build running on the machine.
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$root = Split-Path $PSScriptRoot
$run = Join-Path $env:TEMP ("wildreach-alltests-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $run | Out-Null
$tests = @(
	@{ name = 'unit'; script = 'test.ps1'; what = 'automation tests: rules, combat scenarios, ranged, taming, creatures, clock, wildlife sites, versions' },
	@{ name = 'mp'; script = 'mptest.ps1'; what = 'server + bots, PvE' },
	@{ name = 'persist'; script = 'persisttest.ps1'; what = 'save, restart, load' },
	@{ name = 'craft'; script = 'crafttest.ps1'; what = 'stations, Hearth tiers, sockets, every ranged weapon and ammo, bow wear and repair' },
	@{ name = 'build'; script = 'buildtest.ps1'; what = 'claims, locks, raids, collapse, world save' },
	@{ name = 'survival'; script = 'survivaltest.ps1'; what = 'a fresh player: gather, crude tools, hut, workbench' },
	@{ name = 'house'; script = 'housetest.ps1'; what = 'a two-storey house' },
	@{ name = 'sky'; script = 'skytest.ps1'; what = 'day and night, the clock, lights' },
	@{ name = 'tame'; script = 'tametest.ps1'; what = 'food, riding, tranq knockout, narcotic, kibble, eggs, waking, saved tames' },
	@{ name = 'boss'; script = 'bosstest.ps1'; what = 'the six bosses' },
	@{ name = 'social'; script = 'socialtest.ps1'; what = 'PvP preset, clans, death bags, life skills' },
	@{ name = 'menus'; script = 'menutest.ps1'; what = 'the menus by mouse and keyboard, host and a friend who joins: tabs, crafting, settings, build bar' },
	@{ name = 'flow'; script = 'flowtest.ps1'; what = 'death screen and respawn: killer, bag, back at the camp; a bed respawn; a hint remembered' },
	@{ name = 'spell'; script = 'spelltest.ps1'; what = 'the Paths menu and tree spells' },
	@{ name = 'ranged'; script = 'rangedtest.ps1'; what = 'every ranged weapon at a Cinderfang' },
	@{ name = 'ai'; script = 'aitest.ps1'; what = 'hunting, sleeping, fleeing, special attacks' },
	@{ name = 'wildlife'; script = 'wildlifetest.ps1'; what = 'island wildlife: packs come out near players, go quiet, come back' },
	@{ name = 'version'; script = 'versiontest.ps1'; what = 'another version can''t join and is told why; the menu offers a newer release' },
	@{ name = 'updater'; script = 'updatertest.ps1'; what = 'auto-update end to end with fake releases (no Unreal)' },
	@{ name = 'walk'; script = 'terrain\walktest.ps1'; what = 'island traversal' },
	@{ name = 'hazard'; script = 'terrain\hazardtest.ps1'; what = 'lava and ice gear checks' }
)
if ($Only.Count -gt 0) { $Only = $Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ }; $tests = $tests | Where-Object { $Only -contains $_.name } }

function Stamp($text) { "[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $text }
if (-not $SkipBuild) {
	Stamp 'building WildreachEditor'
	$buildLog = Join-Path $run 'build.log'
	& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' WildreachEditor Win64 Development "-Project=$root\Wildreach.uproject" -WaitMutex *> $buildLog
	if ($LASTEXITCODE -ne 0) { Stamp "BUILD FAILED (see $buildLog)"; exit 1 }
}

$rows = @()
foreach ($t in $tests) {
	$script = Join-Path $PSScriptRoot $t.script
	if (-not (Test-Path $script)) { $rows += [pscustomobject]@{ test = $t.name; result = 'MISSING'; checks = ''; minutes = 0; what = $t.what }; continue }
	Stamp "$($t.name): $($t.what)"
	$log = Join-Path $run "$($t.name).log"
	$start = Get-Date
	& powershell -NoProfile -ExecutionPolicy Bypass -File $script *> $log
	$code = $LASTEXITCODE
	$text = Get-Content $log -ErrorAction SilentlyContinue
	$summary = $text | Where-Object { $_ -match '^\d+ (checks|tests), \d+ failed' } | Select-Object -Last 1
	$failedChecks = $text | Where-Object { $_ -match '^FAIL ' }
	$result = if ($code -ne 0) { 'FAIL' } elseif ($summary) { 'PASS' } else { 'RAN' }
	$rows += [pscustomobject]@{ test = $t.name; result = $result; checks = $(if ($summary) { ($summary -replace ' \((log|logs): .*\)$', '') } else { '' }); minutes = [math]::Round(((Get-Date) - $start).TotalMinutes, 1); what = $t.what }
	foreach ($f in $failedChecks) { "    $f" }
}

''
$rows | Format-Table test, result, checks, minutes, what -AutoSize | Out-String -Width 200
$bad = @($rows | Where-Object { $_.result -eq 'FAIL' -or $_.result -eq 'MISSING' })
$ran = @($rows | Where-Object { $_.result -eq 'RAN' })
"{0} passed, {1} failed, {2} ran without checks (outputs: {3})" -f @($rows | Where-Object { $_.result -eq 'PASS' }).Count, $bad.Count, $ran.Count, $run
if ($bad.Count -gt 0) { exit 1 }
