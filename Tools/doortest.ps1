# Code-lock doors by their keypad (first playable's "doors and code locks": until now only the console set or entered a
# code). Mason hosts L_Island (no window): two foundations at the survival test's hut site and a reinforced door on one,
# locked with 4821 (WRSetCode). Bravo joins in a 1440x900 window and plays by real keys and clicks; the two keep in step
# by chat ("waitchat"):
#  1. At Mason's door: the prompt says "Enter code"; E brings up the keypad; 1111 by the number keys and Enter is wrong
#     (the digits shake, "Wrong code"); 4821 by clicks and Unlock opens it and closes the keypad; a tap of E shuts it.
#  2. Bravo's own reinforced door on the other foundation: "Open · Hold: code lock"; E held brings up its keypad; 7390 by
#     keys and Enter sets the code and locks it; Mason (no code) is kept out; a tap of E still opens it for Bravo; held
#     again, Remove lock takes the lock off.
# -Shots keeps Bravo's pictures.
#   doortest.ps1 [-Port 7894] [-Shots] [-Out <folder>]
param([int]$Port = 7894, [switch]$Shots, [string]$Out = '')
$X = -132000; $Y = -56400; $Z = 4734; $T = $Z + 100   # (survivaltest's hut site: the foundation sunk 60 cm, floor at T)
function Shot { if ($Shots) { @('wait 0.8', 'shot showui', 'wait 1.2') } else { @() } }
function Screen([string]$tag) { @("WRSay $tag", "waitchat $tag", 'WRUIState', 'WRUIText') }
$mason = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'WRHour 11', 'wait 2', 'WRGive stone 200', 'WRGive wood 100', 'WRGive copper_ingot 20',
	"WRTeleport $($X + 600) $Y", 'wait 8', 'WRClearCreatures',
	"WRPlace wood_foundation $X $Y $Z 0", "WRPlace wood_foundation $X $($Y + 400) $Z 0", "WRPlace reinforced_door $($X + 200) $Y $T 90", 'wait 1',
	"WRTeleport $($X + 400) $Y", 'wait 2', 'WRSetCode 4821', 'wait 1', "WRTeleport $($X + 900) $($Y - 600)", 'wait 1', 'WRSay host-ready',
	# 2. Kept out of Bravo's door once it's locked.
	# (1.5 m beside Bravo, who stands at the door: on the same spot the teleport stood Mason on Bravo's head.)
	'waitchat bravo-locked-b', "WRTeleport $($X + 400) $($Y + 550)", 'wait 2', 'WRInteract', 'wait 1.5', "WRTeleport $($X + 900) $($Y - 600)", 'wait 1', 'WRSay mason-tried-b',
	'waitchat bravo-done', 'wait 1', 'WRSay door-done')
$bravo = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 8', 'WRGive stone 60', 'WRGive copper_ingot 10',
	# 1. Mason's locked door.
	"WRTeleport $($X + 400) $Y", 'wait 2', "WRLookAt $($X + 200) $Y $($T + 100)", 'wait 0.5') + (Screen 'ui-prompt-a') +
	@('WRKey E', 'wait 1.5') + (Shot) + (Screen 'ui-keypad-a') +
	@('WRKey One', 'WRKey One', 'WRKey One', 'WRKey One', 'wait 0.3', 'WRKey Enter', 'wait 0.25') + (Shot) + @('wait 1') + (Screen 'ui-wrong-a') +
	@('WRClick 4', 'WRClick 8', 'WRClick 2', 'WRClick 1', 'wait 0.5', 'WRClick Unlock', 'wait 2') + (Shot) + (Screen 'ui-open-a') +
	@('WRKey E', 'wait 1.5') + (Screen 'ui-shut-a') +
	# 2. Bravo's own door.
	@("WRPlace reinforced_door $($X + 200) $($Y + 400) $T 90", 'wait 1', "WRTeleport $($X + 400) $($Y + 400)", 'wait 2', "WRLookAt $($X + 200) $($Y + 400) $($T + 100)", 'wait 0.5') + (Shot) + (Screen 'ui-prompt-b') +
	@('WRKey E hold 0.6', 'wait 1.5') + (Shot) + (Screen 'ui-keypad-b') +
	@('WRKey Seven', 'WRKey Three', 'WRKey Nine', 'WRKey Zero', 'wait 0.3', 'WRKey Enter', 'wait 1.5') + (Shot) + (Screen 'ui-set-b') +
	@('WRKey Escape', 'wait 1', 'WRSay bravo-locked-b', 'waitchat mason-tried-b', 'WRKey E', 'wait 1.5') + (Screen 'ui-tap-b') +
	@('WRKey E hold 0.6', 'wait 1.5', 'WRClick Remove lock', 'wait 1.5') + (Shot) + (Screen 'ui-removed-b') +
	@('WRKey Escape', 'wait 1', 'WRSay bravo-done')
$shotNames = @('keypad_enter', 'wrong_code', 'opened', 'own_door_prompt', 'keypad_own', 'code_set', 'lock_removed')

$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-door-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs | Out-Null
$db = Join-Path $root 'Saved\Worlds\doortest'
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date
$hostLog = Join-Path $logs 'host.log'; $friendLog = Join-Path $logs 'friend.log'
$h = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=Mason?Path=fire?Body=m -game -nullrhi -unattended -nosound -port=$Port -wrworld=doortest -wrtune=wildlife.enabled:0 -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($mason -join '|')`" -abslog=`"$hostLog`"" -PassThru -WindowStyle Hidden
$f = $null
$deadline = (Get-Date).AddSeconds(360)
while ((Get-Date) -lt $deadline -and -not $h.HasExited) {
	Start-Sleep -Seconds 3
	if (-not $f -and (Select-String -Path $hostLog -Pattern 'chat Mason: host-ready' -Quiet -ErrorAction SilentlyContinue)) {
		$f = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=Bravo?Body=f -game -windowed -ResX=1440 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($bravo -join '|')`" -abslog=`"$friendLog`"" -PassThru
	}
	if (Select-String -Path $hostLog -Pattern 'chat Mason: door-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
}
foreach ($p in @($f, $h)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$lock.ReleaseMutex()
if ($Shots) {
	New-Item -ItemType Directory -Force $Out | Out-Null
	$files = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
	for ($i = 0; $i -lt $files.Count; $i++) {
		$label = if ($i -lt $shotNames.Count) { $shotNames[$i] } else { "extra$i" }
		$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $label)
		Move-Item $files[$i].FullName $dest -Force
		$dest
	}
}

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
function Lines([string]$file) { if (Test-Path $file) { @(Get-Content $file -Encoding UTF8 | Where-Object { $_ -match 'LogWildreach: ' } | ForEach-Object { ($_ -replace '^.*LogWildreach: (Warning: )?', '').Trim() }) } else { @() } }
$hl = Lines $hostLog
$fl = Lines $friendLog
$hl | Where-Object { $_ -match '^(DOOR |NOTICE Locked)' } | ForEach-Object { "  host:   $_" }
$fl | Where-Object { $_ -match '^(DOOR |NOTICE (Opened|Closed|Code|Wrong|Lock))' } | ForEach-Object { "  friend: $_" }
function Index($lines, [string]$pattern, [int]$after = -1) { for ($i = $after + 1; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { return $i } }; return -1 }
# What Bravo's screen had after each marker: its texts, and the menu (UISTATE).
function Screen([string]$marker) {
	$from = Index $fl "^NOTICE chat Bravo: $marker$"
	if ($from -lt 0) { return @() }
	$to = Index $fl '^NOTICE chat Bravo: ' $from
	if ($to -lt 0) { $to = $fl.Count }
	@($fl[$from..($to - 1)] | Where-Object { $_ -like 'UITEXT*' -or $_ -like 'UISTATE*' } | ForEach-Object { $_ -replace '^UITEXT\s+-?\d+,\s*-?\d+ ', '' })
}
function Has($texts, [string]$exact) { @($texts | Where-Object { $_ -eq $exact }).Count -ge 1 }
function Menu($texts) { $s = $texts | Where-Object { $_ -like 'UISTATE*' } | Select-Object -First 1; if ($s -match 'menu=(\S+)') { $Matches[1] } else { '?' } }
$pa = Screen 'ui-prompt-a'; $ka = Screen 'ui-keypad-a'; $wa = Screen 'ui-wrong-a'; $oa = Screen 'ui-open-a'; $sa = Screen 'ui-shut-a'
$pb = Screen 'ui-prompt-b'; $kb = Screen 'ui-keypad-b'; $sb = Screen 'ui-set-b'; $tb = Screen 'ui-tap-b'; $rb = Screen 'ui-removed-b'

Check 'Mason''s door is locked with a code' ((Index $hl '^DOOR Mason setcode .*: Code set; door locked') -ge 0)
Check 'at Mason''s locked door, Bravo''s prompt says "Enter code"' ((Has $pa 'Enter code') -and (Menu $pa) -eq 'none')
Check 'E brings up the keypad, for entering the code' ((Menu $ka) -eq 'door' -and (Has $ka "Mason's door") -and (Has $ka 'Locked') -and (Has $ka 'Enter its 4-digit code to open it.') -and (Has $ka 'Unlock') -and (Index $fl "^DOOR keypad for Mason's door \(enter the code\)") -ge 0)
Check 'a wrong code by the number keys and Enter is refused' ((Index $hl '^DOOR Bravo code .*: Wrong code') -ge 0 -and (Has $wa 'Wrong code') -and (Menu $wa) -eq 'door')
Check 'the right code by clicks opens the door and closes the keypad' ((Index $hl '^DOOR Bravo code .*: Code accepted') -ge 0 -and (Menu $oa) -eq 'none' -and (Has $oa 'Close'))
Check 'then E shuts it for Bravo like any door' ((Index $fl '^NOTICE Closed') -ge 0 -and (Has $sa 'Open'))
Check 'on Bravo''s own door: "Open · Hold: code lock"' (Has $pb 'Open  ·  Hold: code lock') (($pb | Where-Object { $_ -match 'Hold|Open|Enter' }) -join ' | ')
Check 'E held brings up its keypad, to set a code' ((Menu $kb) -eq 'door' -and (Has $kb 'Your door · code lock') -and (Has $kb 'No lock: anyone can open it') -and (Has $kb 'Set code and lock') -and (Index $fl "^DOOR keypad for Bravo's door \(theirs") -ge 0)
Check 'a code by keys and Enter locks it' ((Index $hl '^DOOR Bravo setcode .*: Code set; door locked') -ge 0 -and (Has $sb 'Locked with a code') -and (Has $sb 'Remove lock') -and (Has $sb 'Code set; door locked'))
# (Mason's game has the UI too, with no window: E brings up the keypad; without it the server says it's locked.)
$tried = Index $hl '^NOTICE chat Bravo: bravo-locked-b'
Check 'Mason, without the code, is kept out: E brings up the keypad for him' ($tried -ge 0 -and ((Index $hl "^DOOR keypad for Bravo's door \(enter the code\)" $tried) -ge 0 -or (Index $hl '^NOTICE Locked\. Enter its code on the keypad' $tried) -ge 0) -and (Index $hl '^CMD WRPlayerState_0: interact -> Opened' $tried) -lt 0)
Check 'a tap of E still opens it for its owner' ((Index $fl '^NOTICE Opened' (Index $fl '^NOTICE chat Mason: mason-tried-b')) -ge 0 -and (Menu $tb) -eq 'none')
Check 'held again, Remove lock takes the lock off' ((Index $hl '^DOOR Bravo clearcode .*: Lock removed') -ge 0 -and (Has $rb 'No lock: anyone can open it'))
$missed = @($fl | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
Check 'every click lands' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
$errors = @(Select-String -Path $hostLog, $friendLog -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in either log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
