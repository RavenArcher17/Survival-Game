# Fresh-player survival loop on L_Island, end to end: spawn with the starting kit, gather wood / stone / fiber from the
# island's trees, rocks and shrubs bare-handed, craft the crude hatchet + pick by hand, check they gather more, build a
# hut (foundation, three walls, a door, a roof), place a workbench inside, walk in through the door, craft the stone
# tools at the bench and check they gather more again. Everything the hut and tools cost is gathered, nothing given;
# the bot gathers with light attacks (WRGather), like a player clicking at a tree.
#   survivaltest.ps1          headless
#   survivaltest.ps1 -Shots   rendered client: screenshots of a tree before/after felling and the hut from 3 angles + inside
# The island's world save is backed up and put back afterwards, so the test hut never stays in your world.
param([switch]$Shots)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$db = Join-Path $root 'Saved\ServerDB'
$shotDir = Join-Path $root 'Saved\Screenshots'
$before = Get-Date
$out = Join-Path $env:TEMP ("wildreach-survival-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$name = 'Survivor'
$playerSave = Join-Path $db "players\$name.json"
$worldSave = Join-Path $db 'world_L_Island.json'
Remove-Item $playerSave -ErrorAction SilentlyContinue
if (Test-Path $worldSave) { Copy-Item $worldSave "$out\world_backup.json" }

# Gathering: the bot walks to the nearest tree / rock / shrub and swings light attacks at it (WRGather), from a spot
# next to a known one; what it gets shows up as "+N Wood (tool)" notices.
function Gather([string]$at, [string]$group, [double]$secs) { "WRTeleport $at|wait 2|WRGather $group|WRBotPause 0|wait $secs|WRBotPause 1|WRGather off" }
function Shot([string]$at, [string]$look) { if ($Shots) { "WRTeleport $at|wait 3|WRLook $look|wait 2|HighResShot 1600x900|wait 1" } else { $null } }
# Hut site 49 m south of the middle volcanic spawn (outside its safe zone): a flat 4 m cell, ground ~4794.
$X = -132000; $Y = -56400; $Z = 4734   # foundation sunk 60 cm (building.foundationSink): its floor is a 40 cm step
$T = $Z + 100

$cmds = @(
	'WRBotPause 1', 'wait 2',
	'WRCraft crude_hatchet none',                                                         # nothing to make it from yet
	(Shot '-131246 -59750' '0 -5'),                                                         # tree 2020 before
	(Gather '-130596 -59750' 'tree' 14),                                                    # tree 2020, bare hands
	(Shot '-131246 -59750' '0 -5'),                                                         # ...and felled
	(Gather '-134077 -58676' 'rock' 6),                                                     # rock 1004, bare hands
	'WRCraft crude_hatchet none', 'WRCraft crude_pick none', 'wait 1',                     # by hand, no station
	(Gather '-129046 -58811' 'tree' 35),                                                    # tree 2037 and on, crude hatchet
	(Gather '-131858 -59449' 'rock' 6),                                                     # rock 998, crude pick
	(Gather '-132959 -59639' 'shrub' 8),                                                    # shrub 1462 and on (fiber)
	# The hut: everything placed from 6 m east of it.
	"WRTeleport $($X + 600) $Y", 'wait 2',
	"WRPlace wood_foundation $X $Y $Z 0",
	"WRPlace wood_wall $X $($Y + 200) $T 0", "WRPlace wood_wall $X $($Y - 200) $T 0", "WRPlace wood_wall $($X - 200) $Y $T 90",
	"WRPlace wood_door $($X + 200) $Y $T 90",
	"WRPlace wood_roof $X $Y $($T + 300) 0",
	"WRPlace workbench $($X - 120) $($Y + 100) $T 90", 'wait 1',
	# In through the door: open it, walk in (the walk counts as arrived 4 m short of its goal past the back wall,
	# i.e. inside; a shut door leaves it stuck outside), craft at the bench.
	"WRTeleport $($X + 400) $Y", 'wait 2', 'WRInteract', 'wait 1', "WRWalkTo $($X - 500) $Y", 'wait 6',
	'WRCraft stone_hatchet none', 'WRCraft stone_pick none', 'wait 1',
	# E just inside the door, with the bench in reach too, closes the door (the nearer one wins).
	"WRWalkTo $($X + 500) $Y", 'wait 3', 'WRInteract', 'wait 1',
	$(if ($Shots) { "WRTeleport -135802 -59658|wait 3|WRGather tree|WRBotPause 0|wait 1.7|HighResShot 1600x900|wait 6|WRBotPause 1|WRGather off" }  # mid-chop
	  else { Gather '-135802 -59658' 'tree' 8 }),                                           # tree 2018, stone hatchet
	(Shot "$($X + 1000) $($Y + 400)" '202 -8'),                                             # hut from the door side
	(Shot "$($X - 900) $($Y + 900)" '315 -8'),                                              # from the northwest
	(Shot "$($X - 300) $($Y - 1100)" '75 -8'),                                              # from the south
	$(if ($Shots) { "WRTeleport $($X + 330) $Y|wait 3|WRView fp|WRLook 180 -12|wait 2|HighResShot 1600x900|wait 1|WRView tp" }),  # in at the door
	'wait 3'
) | Where-Object { $_ }
$cmds = $cmds -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7833 -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$render = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7833?Path=fire?WRName=$name -game $render -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
# Wait for the bot to run its list (the last command is a wait), then let the 10 s player autosave catch up.
$deadline = (Get-Date).AddSeconds($(if ($Shots) { 420 } else { 330 }))
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$out\client.log" -Pattern 'Stone pickaxe' -Quiet -ErrorAction SilentlyContinue) {
		Start-Sleep -Seconds $(if ($Shots) { 75 } else { 30 }); break
	}
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
if (Test-Path "$out\world_backup.json") { Copy-Item "$out\world_backup.json" $worldSave -Force } else { Remove-Item $worldSave -ErrorAction SilentlyContinue }

# ---- results
$notices = @(Select-String -Path "$out\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','').Trim() })
$server = @(Select-String -Path "$out\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') })
function Yields([string]$item, [string]$tool) {
	$pat = if ($tool) { "^\+(\d+) $item  \($tool\)$" } else { "^\+(\d+) $item$" }
	@($notices | ForEach-Object { if ($_ -match $pat) { [int]$Matches[1] } })
}
function Avg($xs) { if ($xs.Count -eq 0) { 0 } else { ($xs | Measure-Object -Average).Average } }
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }

$bareWood = Yields 'Wood' ''; $crudeWood = Yields 'Wood' 'Crude hatchet'; $stoneWood = Yields 'Wood' 'Stone hatchet'
$bareStone = Yields 'Stone' ''; $crudeStone = Yields 'Stone' 'Crude pick'; $fiber = Yields 'Fiber' ''
Check 'no crude tools for free (needs wood first)' ($notices -contains 'need 3 Wood')
Check 'trees give wood bare-handed' ($bareWood.Count -ge 6) ("{0} chops, avg {1:N1}" -f $bareWood.Count, (Avg $bareWood))
Check 'a tree is used up and vanishes for the client' ((@($server | Where-Object { $_ -match 'used up char_tree' }).Count -ge 1) -and (Select-String -Path "$out\client.log" -Pattern 'HARVEST ledger: [1-9]' -Quiet))
Check 'rocks give stone bare-handed' ($bareStone.Count -ge 1) ("{0} hits, avg {1:N1}" -f $bareStone.Count, (Avg $bareStone))
Check 'shrubs give fiber' (($fiber | Measure-Object -Sum).Sum -ge 8) ("{0} fiber" -f ($fiber | Measure-Object -Sum).Sum)
Check 'crude hatchet crafted by hand' ($notices -contains 'Crafted 1 Crude hatchet')
Check 'crude pick crafted by hand' ($notices -contains 'Crafted 1 Crude pick')
Check 'crude hatchet chops more than bare hands' ($crudeWood.Count -ge 4 -and (Avg $crudeWood) -gt (Avg $bareWood)) ("avg {0:N1} vs {1:N1}" -f (Avg $crudeWood), (Avg $bareWood))
Check 'crude pick breaks more stone than bare hands' ($crudeStone.Count -ge 1 -and (Avg $crudeStone) -gt (Avg $bareStone)) ("avg {0:N1} vs {1:N1}" -f (Avg $crudeStone), (Avg $bareStone))
foreach ($piece in 'Wood foundation', 'Wood doorway + door', 'Wood roof', 'Workbench') { Check "built $piece" ($notices -contains "Built $piece") }
Check 'built three wood walls' (@($notices | Where-Object { $_ -eq 'Built Wood wall' }).Count -eq 3)
Check 'nothing refused while building' (-not ($notices | Where-Object { $_ -match '^(no support|need |too far|something is already|no building|not inside)' -and $_ -ne 'need 3 Wood' })) (($notices | Where-Object { $_ -match '^(no support|need |too far|something is already|no building)' -and $_ -ne 'need 3 Wood' }) -join '; ')
Check 'the door opens' ($notices -contains 'Opened')
Check 'E inside closes the door (not the bench menu)' ($notices -contains 'Closed')
Check 'walked in through the door' (Select-String -Path "$out\client.log" -Pattern 'WALK arrived' -Quiet)
Check 'stone hatchet crafted at the workbench' ($notices -contains 'Crafted 1 Stone hatchet')
Check 'stone pickaxe crafted at the workbench' ($notices -contains 'Crafted 1 Stone pickaxe')
Check 'stone hatchet chops more than the crude one' ($stoneWood.Count -ge 2 -and (Avg $stoneWood) -gt (Avg $crudeWood)) ("avg {0:N1} vs {1:N1}" -f (Avg $stoneWood), (Avg $crudeWood))
$save = if (Test-Path $playerSave) { Get-Content $playerSave -Raw | ConvertFrom-Json } else { $null }
$carried = if ($save) { @($save.inventory.slots | Where-Object { $_.id } | ForEach-Object { $_.id }) } else { @() }
Check 'the save holds all four tools and the starting kit' ((@('crude_hatchet', 'crude_pick', 'stone_hatchet', 'stone_pick', 'minor_healing_potion', 'waterskin') | Where-Object { $carried -notcontains $_ }).Count -eq 0) ($carried -join ', ')
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None')
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
if ($Shots) { Get-ChildItem $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | ForEach-Object { $_.FullName } }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
