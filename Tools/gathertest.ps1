# Held tools and harvestable bodies on L_Island (the gather pass): the bot chops a tree with the sword held and one
# with a crude hatchet held, breaks rock with the sword and with a stone pick, wears the hatchet out (it gathers at
# the base rate broken), repairs it at a workbench and chops at its rate again; then kills a Pumicehog, butchers its
# body with the sword until it's gone, kills another, walks straight through its body, and butchers that one
# bare-handed. Counts come from the "+N Wood (tool)" notices and the server's GATHER / BODY lines.
#   gathertest.ps1 [-Port 7873]          headless
#   gathertest.ps1 -Shots [-Out <dir>]   rendered client: the hatchet and the pick in hand, a body, from two angles, day and night
param([int]$Port = 7873, [switch]$Shots, [string]$Out = '')
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$db = Join-Path $root 'Saved\ServerDB'
$logs = Join-Path $env:TEMP ("wildreach-gathertest-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
if ($Shots) {
	# One rendered run at a time on this machine (as uishots.ps1).
	$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
	try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
}
$name = 'Gatherer'
Remove-Item (Join-Path $db "players\$name.json"), (Join-Path $db 'world_gathertest.json') -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date

# Gathering: the bot walks to the nearest tree / rock next to a known one and swings light attacks (WRGather).
function Gather([string]$at, [string]$group, [double]$secs) { "WRTeleport $at|wait 2|WRGather $group|WRBotPause 0|wait $secs|WRBotPause 1|WRGather off|wait 1" }
$labels = @()
function Shot([string]$label, [string]$setup) { if ($Shots) { $script:labels += $label; "$setup|wait 1.5|HighResShot 1600x900|wait 1.5" } else { $null } }
function UIShot([string]$label, [string]$setup) { if ($Shots) { $script:labels += $label; "$setup|wait 1|shot showui|wait 1.5" } else { $null } }
# A creature put down at a known spot near the survival test's hut site and held still at once (it would wander off,
# or come up to you), then faced from 2 m north of it (WRGoTo stands you there; it finds a body too).
$BX = -131000; $BY = -56500
function Put([string]$id) { "WRTeleport $BX $($BY + 700)|wait 2|WRClearCreatures|WRSpawnAt $id $BX $BY|wait 0.3|WRHold 120|wait 2|WRGoTo $id|wait 1.5|WRLook 270 -10|wait 0.5" }
function Finish([string]$id) { "WRCreatureHP $id 0.01|wait 0.5|WRPress light|wait 2" }
function Butcher([int]$n) { (@(1..$n) | ForEach-Object { 'WRPress light|wait 1.3' }) -join '|' }

$cmds = @(
	'WRBotPause 1', 'WRFlow hints off', 'wait 2', 'WRHour 11', 'WRGive crude_hatchet 1', 'WRGive stone_pick 1', 'wait 1',
	(Gather '-130596 -59750' 'tree' 9),                       # tree 2020, the starting sword in hand: the base rate
	# Rendered, the hatchet comes out by its hotbar key as a player's would (the bar fills sword, food, drink,
	# potion, then the tools given here: key 5); headless has no hotbar, so the server command.
	$(if ($Shots) { 'WRKey Five|wait 1|WRUIState' } else { 'WREquip crude_hatchet' }), 'wait 1',
	(UIShot 'hud_hatchet' 'WRLook 0 -5'),                     # the hatchet selected, its durability over the hotbar
	(Shot 'hatchet_hand_a' 'WRArtCam me 60|wait 2'),          # the hatchet in the hand, two angles
	(Shot 'hatchet_hand_b' 'WRArtCam me 230|wait 2'),
	$(if ($Shots) { 'WRArtCam off|wait 1' }),
	(Gather '-129046 -58811' 'tree' 9),                       # tree 2037, the hatchet held
	'WREquip copper_sword', 'wait 1',
	(Gather '-134077 -58676' 'rock' 6),                       # rock 1004, the sword held
	'WREquip stone_pick', 'wait 1',
	(Shot 'pick_hand_a' 'WRArtCam me 70|wait 2'),
	(Shot 'pick_hand_b' 'WRArtCam me 250|wait 2'),
	$(if ($Shots) { 'WRArtCam off|wait 1' }),
	(Gather '-131858 -59449' 'rock' 6),                       # rock 998, the pick held
	# Worn out: broken it gathers at the base rate; mended at a workbench it's back to its rate.
	'WREquip crude_hatchet', 'wait 1', 'WRCmd wear 500', 'wait 1',
	(Gather '-135802 -59658' 'tree' 5),                       # tree 2018, the hatchet broken
	'WRCmd repair weapon', 'wait 1',                          # no station here
	'WRGive wood 10', 'WRGive stone 10', 'WRTeleport -131400 -56250', 'wait 2', 'WRPlace workbench -131670 -56250 4803 0', 'wait 1',
	'WRGoTo workbench', 'wait 1', 'WRCmd repair weapon', 'wait 1',
	(Gather '-135802 -59658' 'tree' 5),                       # and on, mended
	# Bodies: a Pumicehog hit with the hatchet (a modest blow), killed, and butchered with the sword...
	(Put 'pumicehog'), 'WRPress light', 'wait 1.5', (Finish 'pumicehog'),
	'WREquip copper_sword', 'wait 1',
	(Shot 'body_a' 'WRArtCam pumicehog 50|wait 2'),
	(Shot 'body_b' 'WRArtCam pumicehog 200|wait 2'),
	$(if ($Shots) { 'WRArtCam off|wait 1|WRGoTo pumicehog|wait 1|WRLook 270 -10|wait 1' }),
	(UIShot 'body_hint' 'WRFlow hints on|WRFlow hint body|wait 1'),
	$(if ($Shots) { 'WRFlow hints off' }),
	(Butcher 5),
	# ...a big Basaltback killed with the sword, walked straight through, then butchered bare-handed.
	(Put 'basaltback'), (Finish 'basaltback'),
	"WRTeleport $($BX - 700) $BY", 'wait 2', "WRWalkTo $($BX + 700) $BY", 'wait 7',
	$(if ($Shots) { 'WRHour 23|wait 3' }),
	(Shot 'body_night_a' 'WRArtCam basaltback 50|wait 2'),
	(Shot 'body_night_b' 'WRArtCam basaltback 220|wait 2'),
	(Shot 'hatchet_night' 'WREquip crude_hatchet|wait 1|WRArtCam me 60|wait 2'),
	$(if ($Shots) { 'WRArtCam off|WREquip copper_sword|wait 1|WRHour 11|wait 2' }),
	'WRGoTo basaltback', 'wait 1.5', 'WRLook 270 -10', 'WRUnequip weapon', 'wait 1',
	(Butcher 10),
	'WREquip copper_sword', 'wait 12',                        # the 10 s player save catches up
	'WRSkillStat gathertest_done'                             # the end marker (a CMD line in the server log)
)
# (a foreach, not Where-Object: piping drops the entries after an empty $(if ...) one)
$cmds = @(foreach ($c in $cmds) { if ($c) { $c } }) -join '|'

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=gathertest -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$render = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=$name -game $render -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$logs\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
$deadline = (Get-Date).AddSeconds($(if ($Shots) { 480 } else { 360 }))
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$logs\server.log" -Pattern 'skillstat gathertest_done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Copy-Item (Join-Path $db "players\$name.json") "$logs\player.json" -ErrorAction SilentlyContinue
Remove-Item (Join-Path $db 'world_gathertest.json'), (Join-Path $db "players\$name.json") -ErrorAction SilentlyContinue

# ---- results
$notices = @(Select-String -Path "$logs\client.log" -Pattern 'NOTICE ' | ForEach-Object { ($_.Line -replace '^.*NOTICE ','').Trim() })
$server = @(Select-String -Path "$logs\server.log" -Pattern 'LogWildreach: ' | ForEach-Object { ($_.Line -replace '^.*LogWildreach: ','') })
function Yields([string]$item, [string]$why) {
	$pat = if ($why) { "^\+(\d+) $item  \($([regex]::Escape($why))\)$" } else { "^\+(\d+) $item$" }
	@($notices | ForEach-Object { if ($_ -match $pat) { [int]$Matches[1] } })
}
function Avg($xs) { if ($xs.Count -eq 0) { 0 } else { ($xs | Measure-Object -Average).Average } }
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }

$swordWood = Yields 'Wood' ''; $hatchetWood = Yields 'Wood' 'Crude hatchet'; $brokenWood = Yields 'Wood' 'Crude hatchet broken: repair it'
$swordStone = Yields 'Stone' ''; $pickStone = Yields 'Stone' 'Stone pickaxe'
Check 'a sword held chops at the base rate' ($swordWood.Count -ge 4) ("{0} chops, avg {1:N1}" -f $swordWood.Count, (Avg $swordWood))
Check 'a hatchet held chops more than the sword' ($hatchetWood.Count -ge 4 -and (Avg $hatchetWood) -gt (Avg $swordWood)) ("avg {0:N1} vs {1:N1} ({2} chops)" -f (Avg $hatchetWood), (Avg $swordWood), $hatchetWood.Count)
Check 'a pick held breaks more stone than the sword' ($pickStone.Count -ge 2 -and $swordStone.Count -ge 2 -and (Avg $pickStone) -gt (Avg $swordStone)) ("avg {0:N1} vs {1:N1}" -f (Avg $pickStone), (Avg $swordStone))
# Durability: the hatchet's goes down one a chop (the server logs what's held and its durability before each hit).
$dur = @($server | ForEach-Object { if ($_ -match '^GATHER .* held crude_hatchet (\d+)$') { [int]$Matches[1] } })
$firstRun = @($dur | Where-Object { $_ -gt 0 } | Select-Object -First 4)
Check 'the hatchet loses durability every chop' ($firstRun.Count -ge 4 -and $firstRun[0] -eq 80 -and $firstRun[1] -eq 79 -and $firstRun[3] -eq 77) ($firstRun -join ' > ')
Check 'the sword wears when it chops too' (@($server | Where-Object { $_ -match '^GATHER .* held copper_sword (\d+)$' } | ForEach-Object { [int]($_ -replace '^.* (\d+)$','$1') } | Select-Object -Unique).Count -ge 3)
Check 'worn to 0 it breaks' ((@($server | Where-Object { $_ -match '^ITEM crude_hatchet broke' }).Count -ge 1) -and ($notices -contains 'crude_hatchet durability 0'))
# (playtest XP levels woodcutting fast and its perks add wood, so the tool in use is read from the server's GATHER lines)
$broken = @($server | Where-Object { $_ -match '^GATHER .* held crude_hatchet 0$' })
Check 'broken, it gathers at the base rate' ($broken.Count -ge 2 -and @($broken | Where-Object { $_ -notmatch ', tool none, ' }).Count -eq 0 -and $brokenWood.Count -ge 2) ("{0} chops, avg {1:N1} vs the sword's {2:N1}" -f $broken.Count, (Avg $brokenWood), (Avg $swordWood))
Check 'repair needs a workbench' ($notices -contains 'no station nearby' -or $notices -contains 'tools are repaired at a workbench')
Check 'repaired at the workbench' ($notices -contains 'Repaired Crude hatchet')
$repairAt = -1; for ($i = 0; $i -lt $server.Count; $i++) { if ($server[$i] -match '^REPAIR .* crude_hatchet at workbench') { $repairAt = $i; break } }
$mended = @(if ($repairAt -ge 0) { $server[$repairAt..($server.Count - 1)] | Where-Object { $_ -match '^GATHER .* held crude_hatchet \d+$' } })
Check 'mended, it chops at its rate again' ($mended.Count -ge 2 -and $mended[0] -match 'tool crude_hatchet, held crude_hatchet 80$') (($mended | Select-Object -First 2 | ForEach-Object { $_ -replace '^GATHER \S+ ', '' }) -join ' | ')
# Bodies.
$save = if (Test-Path "$logs\player.json") { Get-Content "$logs\player.json" -Raw | ConvertFrom-Json } else { $null }
$downs = @($server | Where-Object { $_ -match '^BODY .* down: (\d+) hits' })
$blade = @($server | Where-Object { $_ -match '^BODY .* harvested by .* \(blade, copper_sword\)' })
$bare = @($server | Where-Object { $_ -match '^BODY .* harvested by .* \(blunt, bare hands\)' })
function Items($lines) { $n = 0; foreach ($l in $lines) { foreach ($m in [regex]::Matches($l, '\+(\d+) ')) { $n += [int]$m.Groups[1].Value } }; $n }
Check 'both kills stay down as bodies' ($downs.Count -ge 2) (($downs | Select-Object -First 1) -replace '^BODY ', '')
# Nothing goes in at the kill: the raw meat carried at the end is exactly what the hits on the bodies took.
$taken = 0; foreach ($l in @($blade) + @($bare)) { if ($l -match '\+(\d+) Raw meat') { $taken += [int]$Matches[1] } }
$meat = if ($save) { (@(@($save.inventory.slots) + @($save.inventory.weapon) | Where-Object { $_.id -eq 'raw_meat' }) | Measure-Object -Property count -Sum).Sum } else { -1 }
Check 'no loot straight into the bag at the kill' ($taken -gt 0 -and $meat -eq $taken) ("{0} raw meat carried, {1} cut from bodies" -f $meat, $taken)
Check 'hitting the body takes its loot, hit by hit' ($blade.Count -ge 2 -and (@($notices | Where-Object { $_ -match '^\+\d+ Raw meat.*\(Copper sword\)' }).Count -ge 1)) ("{0} hits, {1} items" -f $blade.Count, (Items $blade))
Check 'the body is gone once emptied' (@($server | Where-Object { $_ -match '^BODY .* emptied and gone' }).Count -ge 2)
# A blade keeps all of what a hit cuts; bare hands half (gather.body.bareShare): per hit, and over the whole body.
$bigLoot = @($downs | Where-Object { $_ -match '\(basaltback\)' } | ForEach-Object { $n = 0; foreach ($m in [regex]::Matches($_, ' x(\d+)')) { $n += [int]$m.Groups[1].Value }; $n } | Select-Object -First 1)
$bladePerHit = if ($blade.Count) { (Items $blade) / $blade.Count } else { 0 }
$barePerHit = if ($bare.Count) { (Items $bare) / $bare.Count } else { 0 }
Check 'bare hands get less from a body than a blade' ($bare.Count -ge 2 -and $barePerHit -gt 0 -and $barePerHit -lt $bladePerHit -and $bigLoot.Count -eq 1 -and (Items $bare) -le [math]::Ceiling($bigLoot[0] / 2)) ("{0:N2} vs {1:N2} a hit; {2} of the Basaltback's {3}" -f $barePerHit, $bladePerHit, (Items $bare), $(if ($bigLoot.Count) { $bigLoot[0] } else { '?' }))
# Walked along a line through the Basaltback's body (90 cm across): it gets there without being turned aside.
$walk = @(Select-String -Path "$logs\client.log" -Pattern 'WALK arrived after .* at V\(X=([-\d.]+), Y=([-\d.]+)' | Select-Object -Last 1)
$off = if ($walk.Count) { [math]::Abs([double]$walk[0].Matches[0].Groups[2].Value - $BY) } else { 999 }
Check 'a body does not block the way' ($walk.Count -eq 1 -and $off -lt 40 -and @($server | Where-Object { $_ -match '^BODY .*\(basaltback\) down' }).Count -eq 1) ("arrived {0:N0} cm off the line" -f $off)
# The first blow on the first Pumicehog is the hatchet's (held since the repair).
$blow = @($server | Where-Object { $_ -match '^COMBAT WRCharacter\S* -> WRCreature\S* dmg=([\d.]+) .*result=hit' } | Select-Object -First 1)
$dmg = if ($blow.Count -gt 0 -and $blow[0] -match 'dmg=([\d.]+)') { [double]$Matches[1] } else { 0 }
Check 'a hatchet swung at a creature hits it (modest damage)' ($dmg -gt 0 -and $dmg -lt 12) ("{0} damage (a copper sword: 12)" -f $dmg)
$errors = @(Select-String -Path "$logs\server.log", "$logs\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None')
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
if ($Shots) {
	$files = @(Get-ChildItem $shotDir -Filter '*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
	for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
		$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
		Copy-Item $files[$i].FullName $dest -Force
		$dest
	}
}
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
