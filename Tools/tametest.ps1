# End-to-end taming test on the arena: offer food (Ashstrider), ride it; ARK-style knockout taming (Cinderfang:
# two tranq arrows, a stone arrow that costs effectiveness, a narcotic, kibble in its stash, tamed with a bonus);
# steal and hatch a Kilnmite egg, give orders; a Rimeclaw left to wake up loses its progress; then restart the server
# and check tames were saved (with their bonus). Taming times and the unconscious torpor drain are shortened with
# -wrtune. -Shots runs a windowed client and takes screenshots of the taming panel and the Taming menu.
param([switch]$Shots)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$out = Join-Path $env:TEMP ("wildreach-tame-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$db = Join-Path (Split-Path $project) 'Saved\ServerDB'
Remove-Item (Join-Path $db 'players\Tamer.json'), (Join-Path $db 'world_tametest.json') -ErrorAction SilentlyContinue
$tune = '-wrtune=taming.easySeconds:4,taming.mediumSeconds:6,taming.hardSeconds:600,taming.tameLimit:5,taming.unconsciousTorporDecay:0.05'
$before = Get-Date
$shot = if ($Shots) { 'HighResShot 1600x900|wait 1' } else { 'wait 0.1' }
$aim = 'WRAimAt cinderfang|wait 0.1'   # a frame for the camera to swing round before the shot

$cmds = @(
	'WRBotPause 1', 'WRGive raw_meat 5', 'WRGive berries 5',
	'WRTeleport 2500 3000', 'wait 1', 'WRSpawn ashstrider 1', 'wait 1', 'WRGoTo ashstrider', 'WRInteract', 'wait 7', 'WRTameStat',
	'WRTame ride', 'wait 2', 'WRTameStat', 'WRDismount', 'wait 1', 'WRTameStat', 'wait 1', 'WRTame stay', 'wait 1',
	# Knockout taming: tranq arrows put it down, a stone arrow while it's out costs effectiveness, a narcotic keeps it
	# under, kibble in its stash tames it.
	'WRGive hunting_bow 1', 'WRGive tranq_arrow 4', 'WRGive narcotic 2', 'WRGive kibble 3',
	'WRTeleport -1500 -5000', 'wait 1', 'WRSpawn cinderfang 1', 'WRHold 30', 'wait 1',   # rooted as it lands (a fresh one lunges at you)
	'WREquip hunting_bow', 'wait 1', 'WRRanged 1', 'wait 1', 'WRAim 1', 'wait 1',
	$aim, 'WRFire', 'wait 1.3', $aim, 'WRFire', 'wait 1.3', 'WRTameStat',
	'WRGive stone_arrow 1', 'WRAmmo 1', 'wait 0.5', $aim, 'WRFire', 'wait 1', 'WRAim 0', 'WRRanged 0', 'WRTameStat',
	'WRGoTo cinderfang', 'wait 0.5', 'WRNarcotic', 'wait 0.5', 'WRInteract', 'wait 1', $shot, 'WRInteract', 'wait 0.5',
	'WRTameGive kibble', 'wait 1', $shot, 'WRTameStat', 'wait 7', 'WRTameStat',
	'WRGoTo nest kilnmite', 'wait 1', 'WRInteract', 'wait 1', 'WREquip egg_kilnmite', 'wait 2', 'WRTameStat', 'WRTeleport 900 -3300', 'wait 5', 'WRTameStat', 'WRTame aggressive',
	# Park every tame well away from the nest's angry guardians and the Rimeclaw (tames walk after you; give them time).
	'WRTeleport 2500 -1000', 'wait 6', 'WRTameAll stay', 'wait 0.5',
	# Left to wake up: a Rimeclaw (hard: 600 s of food, 60 s on the playtest preset) part-tamed on raw meat loses its progress when it wakes.
	'WRTeleport -1500 -5000', 'wait 1', 'WRSpawn rimeclaw 1', 'wait 0.5', 'WRGoTo rimeclaw', 'WRTorpor 1000', 'WRHold 90', 'wait 0.5',   # rooted: awake, it stays put
	'WRTameGive raw_meat', 'wait 8', 'WRTameStat', 'WRTeleport 2500 3800', 'wait 12'   # away: it wakes alone
) -join '|'

function Start-Server($log) {
	Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7811 -wrworld=tametest $tune -abslog=`"$log`"" -PassThru -WindowStyle Hidden
}
$s = Start-Server "$out\server.log"
Start-Sleep -Seconds 20
$view = if ($Shots) { '-windowed -ResX=1600 -ResY=900' } else { '-nullrhi' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7811?Path=fire?WRName=Tamer -game $view -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmds`" -abslog=`"$out\client.log`"" -PassThru -WindowStyle $(if ($Shots) { 'Normal' } else { 'Hidden' })
Start-Sleep -Seconds 130   # past the 60 s and 120 s world autosaves
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }

# Restart: the tames should come back.
$s2 = Start-Server "$out\server2.log"
Start-Sleep -Seconds 30
if (-not $s2.HasExited) { Stop-Process -Id $s2.Id -Force }

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Count($path, $pattern) { @(Select-String -Path $path -Pattern $pattern).Count }
$log = "$out\server.log"
$notices = @(Select-String -Path "$out\client.log" -Pattern 'NOTICE (.*)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
function Said($pattern) { @($notices | Where-Object { $_ -match $pattern }).Count }
Check 'an Ashstrider tames on offered food' ((Said "^Tamer's Ashstrider torpor .* tame 100%") -ge 1)
Check 'ride it and get off' ((Said '^Riding \(E to dismount\)') -ge 1 -and (Said "^Tamer's Ashstrider .* rider WRCharacter") -ge 1)
Check 'two tranq arrows knock a Cinderfang out' ((Count $log 'fires hunting_bow \(tranq_arrow\)') -eq 2 -and (Said '^Cinderfang torpor .* unconscious') -ge 1)
Check 'a hit while it is out costs effectiveness' ((Said '^Cinderfang torpor .* eff 9\d% .*unconscious') -ge 1)
Check 'a narcotic tops its torpor up' ((Said '^Narcotic: torpor') -eq 1)
Check 'kibble tames it with a bonus (from 94%)' ((Count $log 'tamed by Tamer \(effectiveness 9\d%: \+4\d% health') -ge 1)
Check 'a stolen egg hatches a tame' ((Said 'Kilnmite egg hatched') -eq 1)
Check 'waking up loses the progress' ((Count $log 'woke up \([1-9]\d*% tamed, 0% kept\)') -ge 1)
Check 'the tames survive a restart' ((Count "$out\server2.log" 'WORLD loaded .* 3 tames') -eq 1) ((Select-String -Path "$out\server2.log" -Pattern 'WORLD loaded.*' | Select-Object -First 1).Matches.Value)
$errors = @(Select-String -Path "$out\server.log", "$out\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None')
Check 'no errors in the logs' ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')

foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
if ($Shots) { Select-String -Path "$out\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value } }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $out
if ($failed -gt 0) { exit 1 }
