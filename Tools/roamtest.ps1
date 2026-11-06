# Wildlife that roams (WRWildlife). First on fast time: a player stands still for a couple of island days and the packs
# live on around them: they travel and rest by the clock, predators hunt where nobody sees, numbers come back, packs pass
# by and come out where they've got to, all on ground their kind belongs to, cheaply. Then at normal speed: a herd walks
# on with its creatures following; a pack that's been thinned and hurt stays that way out of sight, and through a server
# restart; and a pack that has wandered into a land claim comes out at the edge of it.
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
$sites = (Get-Content -Raw (Join-Path $root 'Data\island_spawns.json') | ConvertFrom-Json).sites
$walk = Get-Content -Raw (Join-Path $root 'Data\island_walk.json') | ConvertFrom-Json
$kinds = @{}
foreach ($k in (Get-Content -Raw (Join-Path $root 'Data\creatures.json') | ConvertFrom-Json).creatures) { if ($k.id) { $kinds[$k.id] = $k } }

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

# ---- on fast time: 30 wildlife seconds to a second, and a 2-minute day to match. The player stands on open lowland.
$stand = $sites | Where-Object { $_.creature -eq 'ashstrider' } | Select-Object -First 1
$fast = @('WRBotPause 1', 'WRFlow quiet on', 'wait 2', "WRTeleport $([int]$stand.x) $([int]$stand.y)", 'wait 5')
foreach ($i in 1..24) { $fast += @('WRHeal', 'wait 10') }
$fast += @('WRSay roam-done')
$start = Run 'fast' '-wrtune=wildlife.stepSeconds:2,wildlife.timeScale:30,wildlife.logSeconds:6,clock.dayMinutes:2' $fast 'roam-done' 330
$newLine = @($start | Where-Object { $_ -match '^WILDLIFE (\d+) packs, (\d+) animals, new; the island.s share is (\d+)' })
$packsAtStart = 0; $share = 0
if ($newLine.Count) { $null = $newLine[0] -match '^WILDLIFE (\d+) packs, (\d+) animals, new; the island.s share is (\d+)'; $packsAtStart = [int]$Matches[1]; $animalsAtStart = [int]$Matches[2]; $share = [int]$Matches[3] }
Check 'a new world has a pack for every site, and about its share of animals' ($packsAtStart -eq $sites.Count -and [Math]::Abs($animalsAtStart - $share) -le 0.25 * $share) "$packsAtStart packs, $animalsAtStart animals, share $share"

$saved = @(Packs)
$sizes = @($saved | ForEach-Object { $_.n })
$lone = @($sizes | Where-Object { $_ -le 2 }).Count
$big = @($sizes | Where-Object { $_ -ge 6 }).Count
Check 'packs come in all sizes: lone animals and pairs, and herds of 6 or more' ($lone -ge 10 -and $big -ge 5) "$lone of 1 or 2, $big of 6 or more, largest $(($sizes | Measure-Object -Maximum).Maximum)"

function Moved($pack) {
	$own = @($sites | Where-Object { $_.creature -eq $pack.id })
	$best = [double]::MaxValue
	foreach ($q in $own) { $d = [Math]::Sqrt([Math]::Pow($q.x - $pack.x, 2) + [Math]::Pow($q.y - $pack.y, 2)); if ($d -lt $best) { $best = $d } }
	$best / 100
}
$walkers = @($saved | Where-Object { $kinds[$_.id].archetype -ne 'burrower' })
$homebodies = @($saved | Where-Object { $kinds[$_.id].archetype -eq 'burrower' })
$away = @($walkers | ForEach-Object { Moved $_ } | Sort-Object)
$median = if ($away.Count) { $away[[int]($away.Count / 2)] } else { 0 }
Check 'packs have left their sites' ($median -gt 150) ("half are over {0:N0} m from any site of their kind" -f $median)
$strayed = @($homebodies | Where-Object { (Moved $_) -gt 1 })
Check 'burrowers and colonies stay home' ($homebodies.Count -ge 10 -and $strayed.Count -eq 0) "$($homebodies.Count) at home, $($strayed.Count) moved"

# Everyone on ground their kind belongs to.
$bad = @()
foreach ($p in $walkers) {
	$k = $kinds[$p.id]
	$band = $walk.bands.($k.tier)
	$slack = 100   # metres: a pack can stop a cell's width over, and comes back to its centroid after being in view
	$d = [Math]::Abs($p.x) / 100
	$gap = $walk.campGap.($k.behavior)
	$campNear = @($walk.camps | Where-Object { [Math]::Sqrt([Math]::Pow($_.x - $p.x, 2) + [Math]::Pow($_.y - $p.y, 2)) / 100 -lt $gap - $slack }).Count
	if (($k.biome -eq 'volcanic') -ne ($p.x -lt 0)) { $bad += "$($p.id) on the wrong side" }
	elseif ($d -lt $band[0] - $slack -or $d -gt $band[1] + $slack) { $bad += "$($p.id) out of its band ($([int]$d) m)" }
	elseif ($campNear) { $bad += "$($p.id) by a camp" }
}
Check 'every pack is on its own side, in its own band, clear of the camps' ($bad.Count -eq 0) (($bad | Select-Object -First 4) -join '; ')

$steps = @($start | Where-Object { $_ -match '^WILDLIFE step: ' })
function Field($line, $word) { if ($line -match "(\d+) $word") { [int]$Matches[1] } else { 0 } }
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
$bred = ($steps | ForEach-Object { Field $_ 'bred' } | Measure-Object -Sum).Sum
$arrived = ($steps | ForEach-Object { Field $_ 'wandered in' } | Measure-Object -Sum).Sum
Check 'and numbers come back' (($bred + $arrived) -ge 1) "$bred bred, $arrived wandered in"
$lastAnimals = if ($steps.Count) { if ($steps[$steps.Count - 1] -match '(\d+) animals of (\d+)') { [int]$Matches[1] } else { 0 } } else { 0 }
Check 'the island stays about as full as it should be' ($lastAnimals -ge 0.6 * $share -and $lastAnimals -le 1.25 * $share) "$lastAnimals animals, share $share"

$outs = @($start | Where-Object { $_ -match '^WILDLIFE (\w+) out at V\(X=(-?[\d.]+), Y=(-?[\d.]+)' } | ForEach-Object { if ($_ -match '^WILDLIFE (\w+) out at V\(X=(-?[\d.]+), Y=(-?[\d.]+)') { [pscustomobject]@{ id = $Matches[1]; x = [double]$Matches[2]; y = [double]$Matches[3] } } })
$offSite = @($outs | Where-Object { (Moved $_) -gt 50 })
$kindsSeen = @($outs | ForEach-Object { $_.id } | Sort-Object -Unique)
Check 'standing still, packs pass by and come out where they have got to' ($outs.Count -ge 5 -and $offSite.Count -ge 3) "$($outs.Count) came out ($($kindsSeen -join ', ')), $($offSite.Count) over 50 m from any site"
$costs = @($steps | ForEach-Object { if ($_ -match '([\d.]+) ms$') { [double]$Matches[1] } })
$costMean = ($costs | Measure-Object -Average).Average; $costMax = ($costs | Measure-Object -Maximum).Maximum
Check 'moving them all costs the server next to nothing' ($costMean -lt 3 -and $costMax -lt 25) ("{0:N2} ms a step on average, {1:N2} at most" -f $costMean, $costMax)

# ---- at normal speed, a herd with a short stay so it sets off soon: its creatures walk with it.
Remove-Item (Join-Path $db 'world.json') -ErrorAction SilentlyContinue   # (and so a new wildlife)
$herdSite = $sites | Where-Object { $_.creature -eq 'ashstrider' } | Select-Object -Last 1
$watch = @('WRBotPause 1', 'WRFlow quiet on', 'wait 2', "WRTeleport $([int]$herdSite.x) $([int]$herdSite.y + 3000)", 'wait 10', 'WRPacks')
foreach ($i in 1..10) { $watch += @('wait 10', 'WRHeal', 'WRPacks') }
# Thin an Ashstrider herd and hurt the rest, then walk off until it fades: out of sight, and in the world's next save.
$watch += @('WRHurtPack ashstrider 1 0.5', 'wait 1', 'WRPacks', 'wait 1')
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
$world = Get-Content -Raw (Join-Path $db 'world.json') | ConvertFrom-Json
$piece = "{`"piece`":`"claim_totem`",`"owner`":`"Stranger`",`"x`":$($claimAt[0]),`"y`":$($claimAt[1]),`"z`":4734,`"yaw`":0,`"hp`":1500,`"code`":`"`",`"locked`":false,`"planted`":false,`"cropSecondsLeft`":0,`"storage`":[]}"
$worldText = Get-Content -Raw (Join-Path $db 'world.json')
$worldText = $worldText -replace '"pieces":\s*\[\s*\]', "`"pieces`":[$piece]"
[IO.File]::WriteAllText((Join-Path $db 'world.json'), $worldText)
$wild = Get-Content -Raw (Join-Path $db 'wildlife.json') | ConvertFrom-Json
$wild.packs = @($wild.packs) + @([pscustomobject]@{ id = 'emberhorn'; x = $claimAt[0] + 500; y = $claimAt[1]; z = 4900; n = 2; r = 650; hp = 1; doing = 'grazing'; tx = $claimAt[0]; ty = $claimAt[1]; wander = $false; next = 1e9; fed = 0 })
[IO.File]::WriteAllText((Join-Path $db 'wildlife.json'), ($wild | ConvertTo-Json -Depth 4 -Compress))
$back = @('WRBotPause 1', 'WRFlow quiet on', 'wait 3')
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
$byClaim = @($againRows | Where-Object { $_.id -eq 'emberhorn' -and [Math]::Sqrt([Math]::Pow($_.x - $claimAt[0], 2) + [Math]::Pow($_.y - $claimAt[1], 2)) -lt 12000 } | Select-Object -First 1)
Check 'a pack inside a land claim comes out at the edge of it' ($claimLoaded -and $byClaim.Count -eq 1 -and [Math]::Sqrt([Math]::Pow($byClaim[0].x - $claimAt[0], 2) + [Math]::Pow($byClaim[0].y - $claimAt[1], 2)) -gt 3000) $(if ($byClaim.Count) { "emberhorn {0:N0} m from the totem" -f ([Math]::Sqrt([Math]::Pow($byClaim[0].x - $claimAt[0], 2) + [Math]::Pow($byClaim[0].y - $claimAt[1], 2)) / 100) } else { "claim loaded: $claimLoaded" })

Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$errors = @(Select-String -Path (Get-ChildItem $logs -Filter '*.log' | ForEach-Object { $_.FullName }) -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
