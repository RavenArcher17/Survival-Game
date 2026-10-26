# Screenshots of every menu and HUD screen, for reviewing the UI: a rendered client on L_CombatArena with a stocked
# bag opens each menu (WRMenu), walks up to each crafting station, a chest and a land claim, a tame and a knocked-out
# creature, and saves one picture per screen, named in order, to -Out (default: a new folder in %TEMP%).
#   menushots.ps1 [-Out <folder>] [-Body f]
# The main menu and the chat box are Slate widgets, which in-game screenshots don't include.
param([string]$Out = '', [string]$Body = '')
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-menushots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$name = 'Snapper'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue

$shot = 'wait 0.6|HighResShot 1600x900|wait 1.2'
# One entry per screen: what it is, then the commands that bring it up (the shot follows).
$screens = [ordered]@{
	'hud'             = 'WRSay anyone up for the Vulkarn tonight?|wait 1'
	'hud_ranged'      = 'WRRanged 1|wait 1.5|WRAim 1|wait 1'
	'inventory'       = 'WRAim 0|WRRanged 0|wait 1|WRMenu inventory 3'
	'inventory_end'   = 'WRMenu inventory 40'
	'build_craft'     = 'WRMenu build 2'
	'build_pieces'    = 'WRMenu build 20'
	'build_more'      = 'WRMenu build 45'
	'placing'         = 'WRMenu none 0|WRTeleport 2400 1800|wait 1|WRMenu build 10|WRMenu confirm 0|WRLook 0 -25|wait 1'
	'skills'          = 'WRMenu build 0|WRMenu none 0|WRMenu skills 1'
	'paths'           = 'WRMenu paths 2'
	'map'             = 'WRMenu none 0|WRMenu map 0'
	'station_prompt'  = 'WRMenu map 0|WRGoTo workbench|wait 1'
	'workbench'       = 'WRInteract|wait 1'
	'forge'           = 'WRInteract|WRGoTo forge|wait 1|WRInteract|wait 1'
	'armorer'         = 'WRInteract|WRGoTo armorer|wait 1|WRInteract|wait 1'
	'alchemy'         = 'WRInteract|WRGoTo alchemy_table|wait 1|WRInteract|wait 1'
	'hearth'          = 'WRInteract|WRGoTo hearth|wait 1|WRInteract|wait 1'
	'chest'           = 'WRInteract|WRTeleport 2400 3000|wait 1|WRPlace claim_totem 2000 3000 0 0|WRPlace wood_foundation 2400 3600 0 0|WRPlace storage_chest 2500 3700 100 0|wait 1|WRTeleport 2500 3450|wait 1|WRInteract|wait 1'
	'claim'           = 'WRInteract|WRTeleport 2000 2800|wait 1|WRInteract|wait 1'
	'tame_order'      = 'WRInteract|WRTeleport 2500 1000|wait 1|WRSpawn ashstrider 1|wait 1|WRGoTo ashstrider|WRInteract|wait 7|WRInteract|wait 1'
	'taming'          = 'WRInteract|WRTeleport -1500 -5000|wait 1|WRSpawn cinderfang 1|WRHold 60|wait 1|WRGoTo cinderfang|WRTorpor 1000|wait 1|WRInteract|wait 1'
}
$cmds = @('WRBotPause 1', 'wait 2',
	'WRGive wood 80', 'WRGive stone 50', 'WRGive fiber 30', 'WRGive hide 8', 'WRGive raw_meat 6', 'WRGive berries 8',
	'WRGive copper_ore 12', 'WRGive tin_ore 4', 'WRGive kibble 3', 'WRGive narcotic 2', 'WRGive cooked_meat 2',
	'WRGive hunting_bow 1', 'WRGive stone_arrow 20', 'WRGive tranq_arrow 10', 'WRGive crude_hatchet 1', 'WRGive crude_pick 1',
	'WRGive copper_sword 1', 'WRGive hide_armor 1', 'WREquip hunting_bow', 'WREquip hide_armor', 'wait 2')
foreach ($s in $screens.GetEnumerator()) { $cmds += $s.Value; $cmds += $shot }
$cmds += 'wait 2'
$cmdline = $cmds -join '|'
$labels = @($screens.Keys)

$tune = '-wrtune=taming.easySeconds:4'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_CombatArena -server -nullrhi -unattended -nosound -port=7839 -wrworld=menushots $tune -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 25
$bodyOpt = if ($Body) { "?Body=$Body" } else { '' }
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:7839?Path=fire?WRName=$name$bodyOpt -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$cmdline`" -abslog=`"$logs\client.log`"" -PassThru
$deadline = (Get-Date).AddSeconds(300)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if ((Test-Path "$logs\client.log") -and @(Select-String -Path "$logs\client.log" -Pattern 'screenshot saved as').Count -ge $labels.Count) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_menushots.json') -ErrorAction SilentlyContinue

$files = @(if (Test-Path "$logs\client.log") { Select-String -Path "$logs\client.log" -Pattern 'screenshot saved as (.*\.png)' | ForEach-Object { $_.Matches[0].Groups[1].Value } })
for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
	$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
	Copy-Item $files[$i] $dest -Force
	$dest
}
"{0} of {1} screens captured (logs: {2})" -f [Math]::Min($files.Count, $labels.Count), $labels.Count, $logs
