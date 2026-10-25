# Biome gear-check hazards on L_Island: a frost-main player in the lava river takes damage, a fire-main
# player (Burning blood = lava resistance) doesn't; on the glacier ice the fire player loses grip and
# the frost player (Permafrost = ice resistance) keeps it. Cell positions come from Data/island_dressing.json.
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-hazard-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path (Split-Path $project) 'Saved\ServerDB\world_hazardtest.json') -ErrorAction SilentlyContinue
$bots = @(
	@{ N = 'Frosty'; P = 'frost'; Lava = 'WRTeleport -40400 85000'; Ice = 'WRTeleport 48400 4000' },
	@{ N = 'Blaze';  P = 'fire';  Lava = 'WRTeleport -48800 85600'; Ice = 'WRTeleport 109000 4000' }
)
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=7822 -wrworld=hazardtest -abslog=`"$out\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$clients = foreach ($b in $bots) {
	# Two readings in the lava 4 s apart, then one on the ice.
	$steps = @('WRBotPause 1', 'wait 2', $b.Lava, 'wait 3', 'WRHazard', 'wait 4', 'WRHazard', 'WRHeal', $b.Ice, 'wait 3', 'WRHazard') -join '|'
	Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7822?Path=$($b.P)?WRName=$($b.N) -game -nullrhi -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$steps`" -abslog=`"$out\$($b.N).log`"" -PassThru -WindowStyle Hidden
}
function Readings([string]$name) {
	if (-not (Test-Path "$out\$name.log")) { return @() }
	@(Select-String -Path "$out\$name.log" -Pattern 'NOTICE hazard (\S+) resist (yes|no) onIce (yes|no) friction ([\d.]+) hp (\d+)' -ErrorAction SilentlyContinue | ForEach-Object {
		$m = $_.Matches[0].Groups; [pscustomobject]@{ kind = $m[1].Value; resist = $m[2].Value -eq 'yes'; onIce = $m[3].Value -eq 'yes'; friction = [double]$m[4].Value; hp = [int]$m[5].Value } })
}
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline -and ((Readings 'Frosty').Count -lt 3 -or (Readings 'Blaze').Count -lt 3)) { Start-Sleep -Seconds 3 }
foreach ($p in @($clients) + $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# ---- results
$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Show($r) { if ($r) { "{0} resist {1} slipping {2} friction {3} hp {4}" -f $r.kind, $r.resist, $r.onIce, $r.friction, $r.hp } else { 'no reading' } }
$cells = Select-String -Path "$out\server.log" -Pattern 'HAZARDS loaded (\d+) lava and (\d+) ice cells' | Select-Object -First 1
Check 'the island''s lava and ice cells load' ($cells -and [int]$cells.Matches[0].Groups[1].Value -gt 0 -and [int]$cells.Matches[0].Groups[2].Value -gt 0) $(if ($cells) { $cells.Line -replace '^.*HAZARDS ', '' })
$frost = Readings 'Frosty'; $fire = Readings 'Blaze'
Check 'lava burns a frost main' ($frost.Count -ge 2 -and $frost[0].kind -eq 'lava' -and -not $frost[0].resist -and $frost[0].hp -lt 100 -and $frost[1].hp -lt $frost[0].hp) "$(Show $frost[0]) / $(Show $frost[1])"
Check 'a fire main walks through lava unhurt (Burning blood)' ($fire.Count -ge 2 -and $fire[0].kind -eq 'lava' -and $fire[0].resist -and $fire[0].hp -eq 100 -and $fire[1].hp -eq 100) "$(Show $fire[0]) / $(Show $fire[1])"
Check 'a fire main slips on the glacier ice' ($fire.Count -ge 3 -and $fire[2].kind -eq 'ice' -and -not $fire[2].resist -and $fire[2].onIce -and $fire[2].friction -lt 1) (Show $fire[2])
Check 'a frost main keeps its grip on ice (Permafrost)' ($frost.Count -ge 3 -and $frost[2].kind -eq 'ice' -and $frost[2].resist -and -not $frost[2].onIce -and $frost[2].friction -ge 4) (Show $frost[2])
$errors = @(Select-String -Path "$out\server.log", "$out\Frosty.log", "$out\Blaze.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
