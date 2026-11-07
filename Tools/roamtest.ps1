# Wildlife that roams (WRWildlife), and where it comes from (Data/spawn_regions.json). First on fast time: a new world
# fills every region to its target with small packs of the region's kinds; a player stands still for a couple of island
# days and the packs live on around them: they travel and rest by the clock, predators hunt where nobody sees, the
# regions get new packs as numbers drop, packs pass by and come out where they've got to, all on ground their kind
# belongs to, cheaply. Then a region that has lost all its animals (a player standing in it, and a building) fills back
# up a couple of packs at a check, out of the player's sight and away from the building. Then at normal speed: a herd
# walks on with its creatures following; a pack that's been thinned and hurt stays that way out of sight, and through a
# server restart; and a pack that has wandered into a land claim comes out at the edge of it.
#   roamtest.ps1 [-Port 7888]
param([int]$Port = 7888)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-roam-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $logs | Out-Null
$db = Join-Path $root 'Saved\Worlds\roamtest'
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$name = 'Roamer'
# (The old spawn sites are still good places to stand: open ground of their kind.)
$sites = (Get-Content -Raw (Join-Path $root 'Data\island_spawns.json') | ConvertFrom-Json).sites
$walk = Get-Content -Raw (Join-Path $root 'Data\island_walk.json') | ConvertFrom-Json
$regions = @{}
foreach ($r in (Get-Content -Raw (Join-Path $root 'Data\spawn_regions.json') | ConvertFrom-Json).regions) { $regions[$r.id] = $r }
$kinds = @{}
foreach ($k in (Get-Content -Raw (Join-Path $root 'Data\creatures.json') | ConvertFrom-Json).creatures) { if ($k.id) { $kinds[$k.id] = $k } }
$packAnimals = @('cinderfang', 'gloomhowl', 'ashstrider', 'emberhorn', 'frostantler')

function Run([string]$tag, [string]$tune, [object[]]$cmds, [string]$doneWord, [int]$seconds) {
	$slog = "$logs\$tag-server.log"
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=roamtest -wrpreset=pve $tune -abslog=`"$slog`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 25
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=$name -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\$tag-client.log`"" -PassThru -WindowStyle Hidden
	$deadline = (Get-Date).AddSeconds($seconds)
	while ((Get-Date) -lt $deadline) {
		Start-Sleep -Seconds 3
		if (Select-String -Path $slog -Pattern "CHAT .*$doneWord" -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 1; break }
	}
	foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
	Start-Sleep -Seconds 2
	@(Select-String -Path $slog -Pattern 'LogWildreach: ' -ErrorAction SilentlyContinue | ForEach-Object { ($_.Line -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
}

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Packs { (Get-Content -Raw (Join-Path $db 'wildlife.json') | ConvertFrom-Json).packs }
function Dist($ax, $ay, $bx, $by) { [Math]::Sqrt([Math]::Pow($ax - $bx, 2) + [Math]::Pow($ay - $by, 2)) / 100 }
# "WILDLIFE spawned ashstrider x3 in volcanic_lowland at -120000 3200: 12 of 133 there now; 412 m from a player, nothing built"
function Spawned($lines) {
	@($lines | ForEach-Object {
		if ($_ -match '^WILDLIFE spawned (\w+) x(\d+) in (\w+) at (-?\d+) (-?\d+): (\d+) of (\d+) there now; (no players|(\d+) m from a player), (nothing built|(\d+) m from a building)') {
			[pscustomobject]@{ id = $Matches[1]; n = [int]$Matches[2]; region = $Matches[3]; x = [int]$Matches[4]; y = [int]$Matches[5]
				player = $(if ($Matches[9]) { [int]$Matches[9] } else { -1 }); building = $(if ($Matches[11]) { [int]$Matches[11] } else { -1 }) }
		}
	})
}
# "WILDLIFE regions: volcanic_lowland 131/133, ..." -> region -> (have, target)
function Regions($line) {
	$out = @{}
	foreach ($m in [regex]::Matches($line, '(\w+) (\d+)/(\d+)')) { $out[$m.Groups[1].Value] = @([int]$m.Groups[2].Value, [int]$m.Groups[3].Value) }
	$out
}
function Field($line, $word) { if ($line -match "(\d+) $word") { [int]$Matches[1] } else { 0 } }

# ---- on fast time: 30 wildlife seconds to a second, and a 2-minute day to match. The player stands on open lowland.
$stand = $sites | Where-Object { $_.creature -eq 'ashstrider' } | Select-Object -First 1
$fast = @('WRBotPause 1', 'WRFlow quiet on', 'wait 2', "WRTeleport $([int]$stand.x) $([int]$stand.y)", 'wait 5')
foreach ($i in 1..24) { $fast += @('WRHeal', 'wait 10') }
$fast += @('WRSay roam-done')
$start = Run 'fast' '-wrtune=wildlife.stepSeconds:2,wildlife.timeScale:30,wildlife.logSeconds:6,clock.dayMinutes:2' $fast 'roam-done' 330
$newLine = @($start | Where-Object { $_ -match '^WILDLIFE (\d+) packs, (\d+) animals, new; the island.s share is (\d+)' })
$packsAtStart = 0; $animalsAtStart = 0; $share = 0
if ($newLine.Count) { $null = $newLine[0] -match '^WILDLIFE (\d+) packs, (\d+) animals, new; the island.s share is (\d+)'; $packsAtStart = [int]$Matches[1]; $animalsAtStart = [int]$Matches[2]; $share = [int]$Matches[3] }
$regionLines = @($start | Where-Object { $_ -like 'WILDLIFE regions: *' })
$first = if ($regionLines.Count) { Regions $regionLines[0] } else { @{} }
$short = @($regions.Keys | Where-Object { -not $first[$_] -or $first[$_][0] -lt $regions[$_].target - 0.5 -or $first[$_][0] -gt $regions[$_].target + 3 })
Check 'a new world fills every region to its target' ($newLine.Count -eq 1 -and $first.Count -eq $regions.Count -and $short.Count -eq 0) "$packsAtStart packs, $animalsAtStart animals of $share; $(if ($regionLines.Count) { $regionLines[0] -replace '^WILDLIFE regions: ' })"

$born = @(Spawned $start)
$fill = @($born | Where-Object { $_.player -lt 0 })   # the new world's, before anyone is on
$wrongSize = @($fill | Where-Object { $_.n -lt $kinds[$_.id].pack[0] -or $_.n -gt $kinds[$_.id].pack[1] -or $_.n -gt 4 -or ($packAnimals -contains $_.id -and $_.n -lt 3) })
$bySize = ($fill | Group-Object n | Sort-Object Name | ForEach-Object { "x$($_.Name): $($_.Count)" }) -join ', '
Check 'packs are small: none over 4, the pack animals in threes and fours' ($fill.Count -ge 100 -and $wrongSize.Count -eq 0) "$($fill.Count) packs ($bySize)$(if ($wrongSize.Count) { '; e.g. ' + $wrongSize[0].id + ' x' + $wrongSize[0].n })"
$foreign = @($fill | Where-Object { -not $regions[$_.region] -or -not ($regions[$_.region].creatures.PSObject.Properties.Name -contains $_.id) })
$missing = @(foreach ($r in $regions.Values) { foreach ($k in $r.creatures.PSObject.Properties.Name) { if (-not @($fill | Where-Object { $_.region -eq $r.id -and $_.id -eq $k }).Count) { "$k in $($r.id)" } } })
Check 'each region has only its own kinds, and every one of them' ($fill.Count -gt 0 -and $foreign.Count -eq 0 -and $missing.Count -eq 0) "$(if ($foreign.Count) { 'foreign: ' + $foreign[0].id + ' in ' + $foreign[0].region })$(if ($missing.Count) { ' missing: ' + ($missing -join ', ') })"

$saved = @(Packs)
$walkers = @($saved | Where-Object { $kinds[$_.id].archetype -ne 'burrower' })
$homebodies = @($saved | Where-Object { $kinds[$_.id].archetype -eq 'burrower' })
$away = @($walkers | ForEach-Object { Dist $_.x $_.y $_.tx $_.ty } | Sort-Object)
$median = if ($away.Count) { $away[[int]($away.Count / 2)] } else { 0 }
Check 'packs have moved off from where they settled' ($median -gt 150) ("half are over {0:N0} m from their territory" -f $median)
$strayed = @($homebodies | Where-Object { (Dist $_.x $_.y $_.tx $_.ty) -gt 1 })
Check 'burrowers and colonies stay home' ($homebodies.Count -ge 6 -and $strayed.Count -eq 0) "$($homebodies.Count) at home, $($strayed.Count) moved"

# Everyone on ground their kind belongs to.
$bad = @()
foreach ($p in $walkers) {
	$k = $kinds[$p.id]
	$band = $walk.bands.($k.tier)
	$slack = 100   # metres: a pack can stop a cell's width over, and comes back to its centroid after being in view
	$d = [Math]::Abs($p.x) / 100
	$gap = $walk.campGap.($k.behavior)
	$campNear = @($walk.camps | Where-Object { (Dist $_.x $_.y $p.x $p.y) -lt $gap - $slack }).Count
	if (($k.biome -eq 'volcanic') -ne ($p.x -lt 0)) { $bad += "$($p.id) on the wrong side" }
	elseif ($d -lt $band[0] - $slack -or $d -gt $band[1] + $slack) { $bad += "$($p.id) out of its band ($([int]$d) m)" }
	elseif ($campNear) { $bad += "$($p.id) by a camp" }
}
Check 'every pack is on its own side, in its own band, clear of the camps' ($bad.Count -eq 0) (($bad | Select-Object -First 4) -join '; ')

$steps = @($start | Where-Object { $_ -match '^WILDLIFE step: ' })
$travelling = @($steps | ForEach-Object { Field $_ 'travelling' })
$resting = @($steps | ForEach-Object { Field $_ 'resting' })
$hunting = @($steps | ForEach-Object { Field $_ 'hunting' })
$restingLow = ($resting | Measure-Object -Minimum).Minimum; $restingHigh = ($resting | Measure-Object -Maximum).Maximum
Check 'packs travel' ((($travelling | Measure-Object -Maximum).Maximum) -ge 30) "up to $(($travelling | Measure-Object -Maximum).Maximum) at once"
Check 'and rest by the clock (more at one time of day than another)' ($restingHigh -ge $restingLow + 20) "$restingLow to $restingHigh resting"
Check 'predators go hunting' ((($hunting | Measure-Object -Maximum).Maximum) -ge 3) "up to $(($hunting | Measure-Object -Maximum).Maximum) at once"
$hunts = @($start | Where-Object { $_ -match '^WILDLIFE hunt: ' })
$caught = @($hunts | Where-Object { $_ -match ' caught a ' })
Check 'hunts are settled where nobody sees, some caught and some missed' ($caught.Count -ge 1 -and $hunts.Count -gt $caught.Count) "$($caught.Count) of $($hunts.Count) caught; e.g. $($hunts | Select-Object -First 1)"
$newPacks = ($steps | ForEach-Object { Field $_ 'new packs' } | Measure-Object -Sum).Sum
$later = @($born | Where-Object { $_.player -ge 0 })
$close = @($later | Where-Object { $_.player -lt 300 })
Check 'as hunts thin them, the regions get new packs, out of the player''s sight' ($newPacks -ge 1 -and $later.Count -ge 1 -and $close.Count -eq 0) ("{0} new packs, the nearest {1} m from the player" -f $newPacks, (($later | ForEach-Object { $_.player } | Measure-Object -Minimum).Minimum))
$lastAnimals = if ($steps.Count) { if ($steps[$steps.Count - 1] -match '(\d+) animals of (\d+)') { [int]$Matches[1] } else { 0 } } else { 0 }
Check 'the island stays about as full as it should be' ($lastAnimals -ge 0.85 * $share -and $lastAnimals -le 1.15 * $share) "$lastAnimals animals, share $share"

$outs = @($start | Where-Object { $_ -match '^WILDLIFE (\w+) out at V\(X=(-?[\d.]+), Y=(-?[\d.]+)' } | ForEach-Object { if ($_ -match '^WILDLIFE (\w+) out at V\(X=(-?[\d.]+), Y=(-?[\d.]+)') { [pscustomobject]@{ id = $Matches[1]; x = [double]$Matches[2]; y = [double]$Matches[3] } } })
$kindsSeen = @($outs | ForEach-Object { $_.id } | Sort-Object -Unique)
Check 'standing still, packs pass by and come out where they have got to' ($outs.Count -ge 5) "$($outs.Count) came out ($($kindsSeen -join ', '))"
$costs = @($steps | ForEach-Object { if ($_ -match '([\d.]+) ms$') { [double]$Matches[1] } })
$costMean = ($costs | Measure-Object -Average).Average; $costMax = ($costs | Measure-Object -Maximum).Maximum
Check 'moving them all costs the server next to nothing' ($costMean -lt 3 -and $costMax -lt 25) ("{0:N2} ms a step on average, {1:N2} at most" -f $costMean, $costMax)

# ---- a region emptied out, with the player standing in it and a building 700 m or more away: it fills back up, a few
# packs at a check, none within 300 m of the player or (for the test) 600 m of the building.
$worldPath = Join-Path $db 'world.json'
$worldBefore = Get-Content -Raw $worldPath
$hut = $sites | Where-Object { $_.creature -in 'ashstrider', 'pumicehog', 'glowmoth' -and (Dist $_.x $_.y $stand.x $stand.y) -gt 700 -and (Dist $_.x $_.y $stand.x $stand.y) -lt 1500 } | Select-Object -First 1
$totem = "{`"piece`":`"claim_totem`",`"owner`":`"Stranger`",`"x`":$([int]$hut.x),`"y`":$([int]$hut.y),`"z`":$([int]$hut.z),`"yaw`":0,`"hp`":1500,`"code`":`"`",`"locked`":false,`"planted`":false,`"cropSecondsLeft`":0,`"storage`":[]}"
[IO.File]::WriteAllText($worldPath, ($worldBefore -replace '"pieces":\s*\[\s*\]', "`"pieces`":[$totem]"))
$wild = Get-Content -Raw (Join-Path $db 'wildlife.json') | ConvertFrom-Json
$emptied = 'volcanic_lowland'
$wild.packs = @($wild.packs | Where-Object { $_.region -ne $emptied })
[IO.File]::WriteAllText((Join-Path $db 'wildlife.json'), ($wild | ConvertTo-Json -Depth 4 -Compress))
$refill = @('WRBotPause 1', 'WRFlow quiet on', 'wait 2', "WRTeleport $([int]$stand.x) $([int]$stand.y)", 'wait 5')
foreach ($i in 1..8) { $refill += @('WRHeal', 'wait 10') }
$refill += @('WRSay refill-done')
$filled = Run 'refill' '-wrtune=wildlife.stepSeconds:2,wildlife.timeScale:30,wildlife.logSeconds:6,wildlife.spawnAwayFromBuildings:60000,clock.dayMinutes:2' $refill 'refill-done' 150
$hutLoaded = @($filled | Where-Object { $_ -match '^WORLD loaded [1-9]' }).Count -ge 1
$fillLines = @($filled | Where-Object { $_ -like 'WILDLIFE regions: *' } | ForEach-Object { (Regions $_)[$emptied] })
$target = $regions[$emptied].target
$partway = @($fillLines | Where-Object { $_ -and $_[0] -gt 0 -and $_[0] -lt $target - 0.5 }).Count
$end = if ($fillLines.Count) { $fillLines[$fillLines.Count - 1] } else { $null }
Check "an emptied region fills back up to its target, over several checks" ($fillLines.Count -ge 2 -and $fillLines[0][0] -eq 0 -and $end -and $end[0] -ge $target - 0.5 -and $partway -ge 3) ("{0}: {1}" -f $emptied, (($fillLines | ForEach-Object { "$($_[0])" }) -join ' > '))
# The same frame is the same check. (The log counts frames to 1000 and round again: the second goes with it.)
$frames = @(Select-String -Path "$logs\refill-server.log" -Pattern "WILDLIFE spawned \w+ x\d+ in $emptied " | ForEach-Object { if ($_.Line -match '^\[([^:\]]+):\d+\]\[\s*(\d+)\]') { "$($Matches[1]) $($Matches[2])" } } | Group-Object | ForEach-Object { $_.Count })
$most = ($frames | Measure-Object -Maximum).Maximum
Check 'no more than 2 new packs in a region at one check' ($frames.Count -ge 10 -and $most -le 2) "$($frames.Count) checks with new packs, at most $most at one"
$back = @(Spawned $filled | Where-Object { $_.region -eq $emptied })
# (The server checks from its start: the first few come before the player has joined.)
$withPlayer = @($back | Where-Object { $_.player -ge 0 })
$nearPlayer = ($withPlayer | ForEach-Object { $_.player } | Measure-Object -Minimum).Minimum
$nearHut = ($back | ForEach-Object { Dist $_.x $_.y $hut.x $hut.y } | Measure-Object -Minimum).Minimum
$tooClose = @($withPlayer | Where-Object { $_.player -lt 300 }) + @($back | Where-Object { $_.building -lt 600 })
Check 'new packs start out of the player''s sight and away from buildings' ($hutLoaded -and $withPlayer.Count -ge 15 -and $tooClose.Count -eq 0 -and $nearHut -ge 600) ("{0} packs, {1} with the player on: the nearest {2} m from the player, {3:N0} m from the building (loaded: {4})" -f $back.Count, $withPlayer.Count, $nearPlayer, $nearHut, $hutLoaded)
$foreign = @($back | Where-Object { -not ($regions[$emptied].creatures.PSObject.Properties.Name -contains $_.id) })
$mix = ($back | Group-Object id | Sort-Object Count -Descending | ForEach-Object { "$($_.Name) $($_.Count)" }) -join ', '
Check 'and are the region''s kinds, mixed by weight' ($foreign.Count -eq 0 -and @($back | Group-Object id).Count -ge 4) $mix
[IO.File]::WriteAllText($worldPath, $worldBefore)

# ---- at normal speed, a herd with a short stay so it sets off soon: its creatures walk with it.
$herdSite = $sites | Where-Object { $_.creature -eq 'ashstrider' } | Select-Object -Last 1
$wild = Get-Content -Raw (Join-Path $db 'wildlife.json') | ConvertFrom-Json
$wild.packs = @($wild.packs) + @([pscustomobject]@{ id = 'ashstrider'; region = 'volcanic_lowland'; x = [int]$herdSite.x; y = [int]$herdSite.y; z = [int]$herdSite.z + 100; n = 4; r = 650; hp = 1; doing = 'grazing'; tx = [int]$herdSite.x; ty = [int]$herdSite.y; wander = $false; next = 0; fed = 0 })
[IO.File]::WriteAllText((Join-Path $db 'wildlife.json'), ($wild | ConvertTo-Json -Depth 4 -Compress))
$watch = @('WRBotPause 1', 'WRFlow quiet on', 'wait 2', "WRTeleport $([int]$herdSite.x) $([int]$herdSite.y + 3000)", 'wait 10', 'WRPacks')
foreach ($i in 1..3) { $watch += @('wait 10', 'WRHeal', 'WRPacks') }
# Thin the Ashstrider herd and hurt the rest while it's still near (it sets off soon), watch it walk on, then walk off
# until it fades: out of sight, and in the world's next save.
$watch += @('WRHurtPack ashstrider 1 0.5', 'wait 1', 'WRPacks')
foreach ($i in 1..6) { $watch += @('wait 10', 'WRHeal', 'WRPacks') }
$watch += @('WRTeleport -131650 -56400', 'wait 75', 'WRSay roamnow-done')
$walked = Run 'watch' '-wrtune=wildlife.grazeMinMinutes:0.1,wildlife.grazeMaxMinutes:0.2,wildlife.despawnSeconds:5,wildlife.stepSeconds:2,wildlife.healPerHour:0' $watch 'roamnow-done' 300
$lines = @($walked | Where-Object { $_ -like 'PACK *' })
$rows = @($lines | ForEach-Object { if ($_ -match '^PACK (\w+) x(\d+) (\w+) at (-?\d+) (-?\d+); (\d+) bodies, ([\d.]+) m away on average, ([\d.]+) at most, health ([\d.]+)') { [pscustomobject]@{ id = $Matches[1]; n = [int]$Matches[2]; doing = $Matches[3]; x = [int]$Matches[4]; y = [int]$Matches[5]; bodies = [int]$Matches[6]; mean = [double]$Matches[7]; far = [double]$Matches[8]; hp = [double]$Matches[9] } } })
$grouped = $rows | Group-Object id | Sort-Object Count -Descending
$followed = $null
foreach ($g in $grouped) {
	$all = @($g.Group)
	$xs = @($all | ForEach-Object { $_.x }); $ys = @($all | ForEach-Object { $_.y })
	$span = [Math]::Sqrt([Math]::Pow((($xs | Measure-Object -Maximum).Maximum - ($xs | Measure-Object -Minimum).Minimum), 2) + [Math]::Pow((($ys | Measure-Object -Maximum).Maximum - ($ys | Measure-Object -Minimum).Minimum), 2)) / 100
	$walkingRows = @($all | Where-Object { $_.doing -eq 'travelling' -and $_.bodies -gt 0 })
	if ($span -ge 30 -and $walkingRows.Count -ge 3) { $followed = [pscustomobject]@{ id = $g.Name; span = $span; mean = (($walkingRows | ForEach-Object { $_.mean } | Measure-Object -Average).Average); far = (($walkingRows | ForEach-Object { $_.far } | Measure-Object -Maximum).Maximum) }; break }
}
Check 'a pack in view walks on, and its creatures walk with it' ($followed -and $followed.mean -lt 30) $(if ($followed) { "{0} walked {1:N0} m, its creatures on average {2:N0} m from the pack, {3:N0} at most" -f $followed.id, $followed.span, $followed.mean, $followed.far } else { "$($rows.Count) reports, none of a pack travelling far enough" })
# The thinned pack: the last PACK report after the hurt names it (health 0.50).
$hurtRow = @($rows | Where-Object { $_.hp -gt 0.45 -and $_.hp -lt 0.55 } | Select-Object -Last 1)
$fadedOk = @($walked | Where-Object { $_ -match 'went quiet' }).Count -ge 1
Check 'hurting a pack in view thins it and leaves the rest hurt' ($hurtRow.Count -eq 1) $(if ($hurtRow.Count) { "$($hurtRow[0].id) x$($hurtRow[0].n), health $($hurtRow[0].hp)" })
$record = $null
if ($hurtRow.Count) {
	Start-Sleep -Seconds 1
	$candidates = @(Packs | Where-Object { $_.id -eq $hurtRow[0].id -and [Math]::Abs($_.hp - 0.5) -lt 0.06 })
	if ($candidates.Count) { $record = $candidates[0] }
}
Check 'out of sight it is a record with its numbers and wounds' ($fadedOk -and $record -and $record.n -eq $hurtRow[0].n) $(if ($record) { "$($record.id) x$($record.n), hp $($record.hp) in wildlife.json" } else { 'not saved (the world saves each minute)' })

# ---- after a restart: the same world, the same pack, still thin and hurt. And one put inside a land claim comes out at its edge.
$claimAt = @(-131650, -56400)
$piece = "{`"piece`":`"claim_totem`",`"owner`":`"Stranger`",`"x`":$($claimAt[0]),`"y`":$($claimAt[1]),`"z`":4734,`"yaw`":0,`"hp`":1500,`"code`":`"`",`"locked`":false,`"planted`":false,`"cropSecondsLeft`":0,`"storage`":[]}"
$worldText = Get-Content -Raw $worldPath
$worldText = $worldText -replace '"pieces":\s*\[\s*\]', "`"pieces`":[$piece]"
[IO.File]::WriteAllText($worldPath, $worldText)
$wild = Get-Content -Raw (Join-Path $db 'wildlife.json') | ConvertFrom-Json
$wild.packs = @($wild.packs) + @([pscustomobject]@{ id = 'emberhorn'; x = $claimAt[0] + 500; y = $claimAt[1]; z = 4900; n = 2; r = 650; hp = 1; doing = 'grazing'; tx = $claimAt[0]; ty = $claimAt[1]; wander = $false; next = 1e9; fed = 0 })
[IO.File]::WriteAllText((Join-Path $db 'wildlife.json'), ($wild | ConvertTo-Json -Depth 4 -Compress))
# (A wait long enough for the saved character to be in the world: a teleport before that is lost.)
$back = @('WRBotPause 1', 'WRFlow quiet on', 'wait 8')
if ($record) { $back += @("WRTeleport $([int]$record.x) $([int]$record.y + 2000)", 'wait 10', 'WRPacks', 'wait 1') }
$back += @("WRTeleport $($claimAt[0] - 6000) $($claimAt[1])", 'wait 10', 'WRPacks', 'wait 1', 'WRSay roamback-done')
$again = Run 'again' '-wrtune=wildlife.grazeMinMinutes:30,wildlife.grazeMaxMinutes:30,wildlife.healPerHour:0' $back 'roamback-done' 120
Check 'a restart picks up the world''s packs as it left them' (@($again | Where-Object { $_ -match '^WILDLIFE \d+ packs, \d+ animals, as the world left them' }).Count -eq 1) (@($again | Where-Object { $_ -like 'WILDLIFE * packs,*' }) -join '')
$againRows = @($again | Where-Object { $_ -like 'PACK *' } | ForEach-Object { if ($_ -match '^PACK (\w+) x(\d+) (\w+) at (-?\d+) (-?\d+); (\d+) bodies, ([\d.]+) m away on average, ([\d.]+) at most, health ([\d.]+)') { [pscustomobject]@{ id = $Matches[1]; n = [int]$Matches[2]; x = [int]$Matches[4]; y = [int]$Matches[5]; bodies = [int]$Matches[6]; hp = [double]$Matches[9] } } })
if ($record) {
	$seen = @($againRows | Where-Object { $_.id -eq $record.id -and $_.n -eq $record.n -and [Math]::Abs($_.hp - 0.5) -lt 0.08 })
	Check 'the thinned pack comes out just as thin and hurt' ($seen.Count -ge 1 -and $seen[0].bodies -eq $record.n) $(if ($seen.Count) { "$($seen[0].id) x$($seen[0].n), $($seen[0].bodies) bodies, health $($seen[0].hp)" } else { 'not seen' })
}
$claimLoaded = @($again | Where-Object { $_ -match '^WORLD loaded [1-9]' }).Count -ge 1
$byClaim = @($againRows | Where-Object { $_.id -eq 'emberhorn' -and (Dist $_.x $_.y $claimAt[0] $claimAt[1]) -lt 120 } | Select-Object -First 1)
Check 'a pack inside a land claim comes out at the edge of it' ($claimLoaded -and $byClaim.Count -eq 1 -and (Dist $byClaim[0].x $byClaim[0].y $claimAt[0] $claimAt[1]) -gt 30) $(if ($byClaim.Count) { "emberhorn {0:N0} m from the totem" -f (Dist $byClaim[0].x $byClaim[0].y $claimAt[0] $claimAt[1]) } else { "claim loaded: $claimLoaded" })

Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$errors = @(Select-String -Path (Get-ChildItem $logs -Filter '*.log' | ForEach-Object { $_.FullName }) -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
