# The menus played with a mouse and keyboard, the way players use them: a host, and a friend who starts from the main
# menu and clicks "Join". Each opens the journal with I, clicks
# through its tabs, crafts a crude hatchet by clicking the recipe and its button, closes it with Esc, opens Settings
# with Esc, clicks between its pages, flips Vertical sync off and back, clicks Resume, then opens and closes the build
# bar. Clicks and keys go through Slate as real ones do (WRClick / WRKey), not straight into the menus' code.
#   menutest.ps1 [-Port 7855] [-Only host|friend] [-MenuHost]
param([int]$Port = 7855, [ValidateSet('', 'host', 'friend')] [string]$Only = '', [switch]$MenuHost)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-menutest-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $logs | Out-Null
$players = @('Hosty', 'Friendo')
foreach ($n in $players) { Remove-Item (Join-Path $root "Saved\ServerDB\players\$n.json") -ErrorAction SilentlyContinue }
Remove-Item (Join-Path $root 'Saved\ServerDB\world_menutest.json') -ErrorAction SilentlyContinue

# What each player does once in the game, and the menu (and cursor) each WRUIState should then show.
$play = @(
	'net:wait 8', 'net:WRUIState',
	'net:WRGive wood 20', 'net:WRGive stone 20', 'net:WRGive fiber 20', 'net:wait 2',
	'net:WRKey I', 'net:wait 1', 'net:WRUIState',
	'net:WRClick Crafting', 'net:wait 1', 'net:WRUIState',
	'net:WRClick Crude hatchet', 'net:wait 0.5', 'net:WRClick Craft Crude hatchet', 'net:wait 4', 'net:WRUIState',
	'net:WRClick Skills', 'net:wait 1', 'net:WRUIState',
	'net:WRClick Paths', 'net:wait 1', 'net:WRUIState',
	'net:WRClick Map', 'net:wait 1', 'net:WRUIState',
	'net:WRClick Inventory', 'net:wait 1', 'net:WRUIState',
	'net:WRKey Escape', 'net:wait 1', 'net:WRUIState',
	'net:WRKey Escape', 'net:wait 1.5', 'net:WRUIState',
	'net:WRClick Video', 'net:wait 1', 'net:WRClick On/Off @ Vertical sync', 'net:wait 1', 'net:WRUIState',
	'net:WRClick On/Off @ Vertical sync', 'net:wait 1', 'net:WRUIState',
	'net:WRClick Audio', 'net:wait 1', 'net:WRClick Controls', 'net:wait 1', 'net:WRClick Resume', 'net:wait 1', 'net:WRUIState',
	'net:WRKey B', 'net:wait 1', 'net:WRUIState',
	'net:WRKey Escape', 'net:wait 1', 'net:WRUIState', 'net:wait 600'
)
$expect = @(
	@('none', 0), @('inventory', 1), @('crafting', 1), @('crafting', 1), @('skills', 1), @('paths', 1), @('map', 1), @('inventory', 1),
	@('none', 0), @('settings', 1), @('settings', 1), @('settings', 1), @('none', 0), @('build', 1), @('none', 0)
)
function Cmds([string]$menuClick) {
	$front = if ($menuClick) { @('solo:wait 3', 'solo:WRMainMenu', 'solo:wait 3', 'solo:WRUIState', "solo:WRClick $menuClick", 'solo:wait 600') } else { @() }
	@('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off') + $front + $play
}
# The host opens the island hosting straight away (what "Host a game" does: L_Island?listen with the menu's name and
# path); the friend starts on the main menu and clicks Join. -MenuHost has the host click "Host a game" too (a
# synthetic click there doesn't always fire in this two-window run, though hosting from the menu works on its own).
function Start-Player([string]$name, [string]$menuClick, [string]$extraIni, [string]$map = '/Game/Maps/L_Island') {
	$log = Join-Path $logs "$name.log"
	$ini = "[Wildreach.Menu]:Name=$name,[Wildreach.Menu]:Body=m,[Wildreach.Menu]:Path=fire$extraIni"
	$args = "`"$project`" $map -game -windowed -ResX=1280 -ResY=720 -unattended -nosound -wrbot -wrworld=menutest -port=$Port " +
		"`"-ini:Engine:[URL]:Port=$Port`" `"-ini:GameUserSettings:$ini`" `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$((Cmds $menuClick) -join '|')`" -abslog=`"$log`""
	Start-Process $ue -ArgumentList $args -PassThru
}

$procs = @()
$started = Get-Date
if ($MenuHost) { $procs += Start-Player 'Hosty' 'Host a game' '' }
else { $procs += Start-Player 'Hosty' '' '' '/Game/Maps/L_Island?listen?WRName=Hosty?Path=fire?Body=m' }
$hostLog = Join-Path $logs 'Hosty.log'
# The friend starts once the host's game is up (its first UISTATE in the hosted world).
function States([string]$log) { if (Test-Path $log) { @(Select-String -Path $log -Pattern 'UISTATE').Count } else { 0 } }
$deadline = (Get-Date).AddSeconds(240)
$hostFront = if ($MenuHost) { 1 } else { 0 }   # the main menu's UISTATE before the game's
while ((Get-Date) -lt $deadline -and (States $hostLog) -lt 1 + $hostFront) { Start-Sleep -Seconds 3 }
$friendLog = Join-Path $logs 'Friendo.log'
if ($Only -ne 'host') { $procs += Start-Player 'Friendo' 'Join' ",[Wildreach.Menu]:Address=127.0.0.1:$Port" }
# Both finish the list (15 states in the game, after the main menu's) or time out.
$deadline = (Get-Date).AddSeconds(300)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 5
	$done = @(@($hostLog, $hostFront), @($friendLog, 1) | Where-Object { (States $_[0]) -ge $_[1] + $expect.Count }).Count
	if ($done -eq $(if ($Only -eq 'host') { 1 } else { 2 })) { Start-Sleep -Seconds 2; break }
}
foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
foreach ($n in $players) { Remove-Item (Join-Path $root "Saved\ServerDB\players\$n.json") -ErrorAction SilentlyContinue }
Remove-Item (Join-Path $root 'Saved\ServerDB\world_menutest.json') -ErrorAction SilentlyContinue

$results = @()
function Check([string]$what, [bool]$ok, [string]$detail = '') { $script:results += [pscustomobject]@{ ok = $ok; what = $what; detail = $detail } }
foreach ($who in @(@{ Name = 'Hosty'; Log = $hostLog; Role = 'host'; Front = $hostFront }, @{ Name = 'Friendo'; Log = $friendLog; Role = 'friend'; Front = 1 })) {
	if ($Only -and $Only -ne $who.Role) { continue }
	$lines = @(Select-String -Path $who.Log -Pattern 'LogWildreach: .*(UISTATE|CLICK|KEY |MENU |NOTICE Crafted)' -ErrorAction SilentlyContinue | ForEach-Object { ($_.Line -replace '^.*LogWildreach: (Warning: )?', '').Trim() })
	"--- $($who.Name) ($($who.Role))"
	$lines | ForEach-Object { "  $_" }
	$states = @($lines | Where-Object { $_ -like 'UISTATE*' })
	$inGame = @($states | Select-Object -Skip $who.Front)
	Check "$($who.Role): reaches the game$(if ($who.Front) { ' from the main menu' })" ($inGame.Count -ge 1) "$($states.Count) states"
	$bad = @()
	for ($i = 0; $i -lt $expect.Count; $i++) {
		$s = if ($i -lt $inGame.Count) { $inGame[$i] } else { '' }
		if ($s -notmatch "menu=$($expect[$i][0]) cursor=$($expect[$i][1]) ") { $bad += "step $($i + 1): wanted $($expect[$i][0]), got " + $(if ($s) { ($s -split ' ')[1] } else { 'nothing' }) }
	}
	Check "$($who.Role): every menu opens, switches and closes as clicked" ($bad.Count -eq 0) (($bad | Select-Object -First 3) -join '; ')
	$missed = @($lines | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
	Check "$($who.Role): every click lands on its button" ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
	$v = @($inGame | ForEach-Object { if ($_ -match 'vsync=(\d)') { $Matches[1] } })
	Check "$($who.Role): Vertical sync flips off and back by clicking" ($v.Count -ge 12 -and $v[10] -ne $v[9] -and $v[11] -eq $v[9]) "vsync $($v[9]) -> $($v[10]) -> $($v[11])"
	Check "$($who.Role): crafting by clicking makes the hatchet" (@($lines | Where-Object { $_ -like 'NOTICE Crafted 1 Crude hatchet*' }).Count -ge 1)
}
$crafts = @(Select-String -Path $hostLog -Pattern 'CRAFT .*Crafted 1 Crude hatchet' -ErrorAction SilentlyContinue).Count
Check "the host's server crafts for both players" ($crafts -ge $(if ($Only) { 1 } else { 2 })) "$crafts crafts"
$errors = @(Select-String -Path @($hostLog, $friendLog | Where-Object { Test-Path $_ }) -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue)
# The engine's Dataflow plugin logs a struct warning as an error on some starts: not the game's.
$errors = @($errors | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check "no errors in the logs" ($errors.Count -eq 0) (($errors | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
