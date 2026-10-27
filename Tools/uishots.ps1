# Screenshots of the stone-and-leather UI (Slate, so "shot showui": HighResShot leaves the UI out). A rendered client
# on L_Island with a stocked bag, learned abilities and a tame opens each screen, saving one numbered picture per
# screen into -Out. Add screens to $screens as they're built. -MainMenu shoots the front end instead ($menuScreens):
# what the game shows on its own, so a standalone client and no server.
# -Flow shoots only the moments between play ($flowScreens: death and respawn, loading, banners, toasts, hints) on a
# souls-penalty server, so the death screen shows a belongings bag; the full run ends with them too.
#   uishots.ps1 [-Out <folder>] [-Only hud,wheel] [-MainMenu] [-Flow]
param([string]$Out = '', [string[]]$Only = @(), [int]$Port = 7845, [switch]$MainMenu, [switch]$Flow)
# One run at a time on this machine: each starts a server and a rendered client, and worktrees can run it side by side.
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
# Unreal otherwise starts with UnrealBuildTool's platform check, which waits for any build running on the machine.
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-uishots-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs, $Out | Out-Null
$name = 'Snapper'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date

$shot = 'wait 0.8|shot showui|wait 1.5'
# Each screen: name -> commands that bring it up (the shot follows). They run in order; later ones build on earlier.
$screens = [ordered]@{
	'hud'          = 'WRTeleport -51623 54155|wait 6|WRLook 270 -8|WRSay anyone up for the Vulkarn tonight?|wait 1'
	'hud_ranged'   = 'WREquip hunting_bow|wait 1|WRRanged 1|wait 1.5|WRAim 1|wait 1'
	'wheel'        = 'WRAim 0|WRRanged 0|wait 1|WRWheel 0 0'
	'wheel2'       = 'WRWheel 0 2'
	'tame_wheel'   = 'WRWheel -1 0|WRSpawn ashstrider 1|wait 1|WRGoTo ashstrider|WRInteract|wait 6|WRWheel 2 0'
	'help'         = 'WRWheel -1 0|wait 0.5|WRHelp'
	'inventory'    = 'WRHelp|WRGive gem_ruby 1|WREquip gem_ruby|wait 0.5|WRMenu inventory 0|wait 1|WRUISelect ranged'
	'inventory_split' = 'WRUIInvPicker split stone'
	'crafting'     = 'WRMenu none 0|WRTeleport -51623 54155|wait 3|WRGive wood 30|WRGive ember_essence 1|WRLook 270 -35|WRGhost workbench|wait 1|WRGhostPlace|wait 1.5|WRGoTo workbench|wait 1.5|WRMenu crafting 0|wait 1.5'
	'paths'        = 'WRGivePoints fire 8|WRAutoSpend fire 5|wait 1.5|WRMenu paths 0|wait 1.5'
	'skills'       = 'WRSkill woodcutting 25|WRSkill mining 18|WRSkill smithing 12|WRSkill cooking 4|WRSkill taming 9|WRSkill sneaking 6|WRSkill tracking 3|WRSkillXP woodcutting 1|WRSkillXP mining 8|WRSkillXP smithing 4|WRSkillXP cooking 5|WRSkillXP taming 5|WRSkillXP sneaking 4|wait 1.5|WRMenu skills 0|wait 1.5'
	'map'          = 'WRMenu none 0|WRTeleport -130000 10000|wait 6|WRGive copper_ingot 5|WRLook 0 -35|WRGhost claim_totem|wait 1|WRGhostPlace|wait 1.5|WRLook 180 -35|WRGhost hearth|wait 1|WRGhostPlace|wait 1.5|WRTeleport -120000 15000|wait 5|WRSpawn ashstrider 1|wait 1|WRGoTo ashstrider|WRInteract|wait 6|WRMenu none 0|WRLook 30 -10|wait 0.5|WRMenu map 0|wait 1.5'
	'build'        = 'WRMenu map 0|WRTeleport -131300 -56400|wait 4|WRLook 180 -18|WRMenu build 0|wait 1'
	'build_placing' = 'WRLook 180 -18|WRGhost wood_foundation|wait 1.5'
	# The Ashstrider tame stays behind (E would pick it over the knocked-out Cinderfang); the Taming screen, fed meat.
	'taming'       = 'WRMenu none 0|WRMenu build 0|WRMenu none 0|WRTameAll stay|WRTeleport -51623 57155|wait 9|WRLook 270 -8|WRSpawn cinderfang 1|WRHold 60|wait 1|WRGoTo cinderfang|WRTorpor 1000|wait 1.5|WRInteract|wait 1|WRTameGive raw_meat|wait 3'
	'taming_hud'   = 'WRInteract|wait 1'
	# A base by the survival test's hut site (as skytest builds it): a hut of wood and stone, four stations, a claim
	# stocked from the bag, and a Hearth fed Vulkarn's core; then E on the totem, and E on the Hearth.
	'claim'        = 'WRMenu none 0|WRTeleport -131300 -56400|wait 4|WRGive wood 300|WRGive stone 200|WRGive copper_ingot 5|WRGive copper_ore 6|WRGive core_vulkarn 1|' +
	                 'WRPlace wood_foundation -132000 -56400 4734 0|WRPlace wood_wall -132000 -56200 4834 0|WRPlace wood_wall -132000 -56600 4834 0|WRPlace stone_wall -132200 -56400 4834 90|' +
	                 'WRPlace wood_door -131800 -56400 4834 90|WRPlace wood_roof -132000 -56400 5134 0|WRPlace stone_gable -132000 -56200 5134 0|WRPlace stone_gable -132000 -56600 5134 0|' +
	                 'WRPlace wood_upper_wall -131800 -56400 5134 90|WRPlace workbench -132060 -56290 4834 0|WRPlace forge -132060 -56510 4834 0|WRPlace armorer -131670 -56250 4803 0|' +
	                 'WRPlace alchemy_table -131670 -56550 4806 0|WRPlace claim_totem -131100 -56000 4847 0|wait 1|WRPlace hearth -131300 -56800 4838 0|wait 1|' +
	                 'WRCmd claimdeposit|WRGive wood 80|WRGive stone 50|WRGoTo hearth|wait 1|WRCmd hearthfeed|wait 1|WRTeleport -131100 -55800|wait 3|WRInteract|wait 1'
	'hearth'       = 'WRMenu none 0|WRGoTo hearth|wait 1|WRInteract|wait 1'
	'settings'     = 'WRMenu none 0|WRMenu settings 0|wait 1.5'
	'settings_listen' = 'WRSettings listen|wait 0.5'
	'settings_conflict' = 'WRSettings key G|wait 0.5'
	'settings_keys' = 'WRSettings cancel|WRSettings scroll 2000|wait 0.5'
	'settings_video' = 'WRSettings video|wait 1'
	# A chest on flat ground far from the tame (E would pick your tame over the chest), wild creatures cleared; the
	# resources stored, some taken back; then the "how many?" picker.
	'storage'      = 'WRMenu none 0|WRTeleport -124800 -51600|wait 5|WRClearCreatures|WRLook 270 -8|WRGive copper_ingot 12|WRPlace storage_chest -124800 -51800 6525 0|wait 2|WRInteract|wait 1|WRUICmd storeall|wait 1|WRUICmd take 1 20|WRUICmd take 0 30|wait 1.5'
	'storage_picker' = 'WRUIStoragePicker store wood'
}
# The moments between play (WRUIFlow). The rest of the run keeps them quiet (WRFlow quiet on) so they don't land in
# other screens; here they come back on, with this player's discoveries and hints forgotten so each fires again.
# flow_death and flow_respawn take their own pictures (the bot pauses while dead): 3.5 s into the death screen, and
# just after the new body arrives (the island loading behind the card).
$flowScreens = [ordered]@{
	'flow_hint'      = 'WRMenu none 0|WRFlow quiet off|WRFlow hints on|WRFlow forget|WRTeleport -51623 54155|wait 8'
	'flow_levelup'   = 'WRGivePoints fire 4|WRAutoSpend fire 3|wait 1.5'
	'flow_gather'    = 'WRFlow near tree|wait 3|WRBotPause 0|WRGather tree|wait 4.5'
	'flow_discover'  = 'WRGather off|WRBotPause 1|WRTeleport -1300 -126600|wait 2.5'
	'flow_boss'      = 'WRTeleport -40000 -60000|wait 6|WRHeal|WRSpawn vulkarn 1|wait 1.2'
	'flow_tame'      = 'WRTeleport -51623 59155|wait 7|WRHeal|WRClearCreatures|WRSpawn ashstrider 1|wait 1|WRGoTo ashstrider|WRInteract|wait 6'
	'flow_recipe'    = 'WRTameAll stay|WRTeleport -124800 -51600|wait 6|WRClearCreatures|WRGive copper_ingot 5|WRGive core_vulkarn 1|WRLook 0 -35|WRGhost claim_totem|wait 1|WRGhostPlace|wait 1.5|WRLook 180 -35|WRGhost hearth|wait 1|WRGhostPlace|wait 1.5|WRGoTo hearth|wait 1|WRCmd hearthfeed|wait 1.5'
	'flow_island'    = 'WRFlow preview island 4|wait 0.6'
	'flow_loading'   = 'WRFlow preview loading 4|wait 0.6'
	# Killed by Magmaws: a hard knock first, so they finish it quickly (tames nearby would otherwise fight them off).
	'flow_death'     = 'wait 4|WRTeleport -51623 64155|wait 6|WRClearCreatures|WRHeal|WRHitPlayer Snapper 105|WRFlow shotondeath 3.5|WRFlow shotonback 0.15|WRSpawn magmaw 2|wait 45'
	'flow_respawn'   = 'wait 0.1'
	'flow_after'     = 'wait 2'
	'flow_settings'  = 'WRMenu none 0|WRMenu settings 0|wait 1.5|WRSettings video|wait 1'
}
$selfShots = @('flow_death', 'flow_respawn')
# The front end (-MainMenu). The bot only runs the commands: WRMainMenu opens the menu as F10 does on your own.
$menuScreens = [ordered]@{
	'mainmenu'          = 'WRMainMenu|wait 4'
	'mainmenu_settings' = 'WRMainMenu settings|wait 1.5'
}
if ($MainMenu) { $screens = $menuScreens }
elseif ($Flow) { $screens = $flowScreens }
else { foreach ($f in $flowScreens.GetEnumerator()) { $screens[$f.Key] = $f.Value } }
if ($Only.Count -gt 0) { $Only = $Only | ForEach-Object { $_ -split ',' } }
$cmds = if ($MainMenu) { @('WRBotPause 1', 'wait 3') } else { @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 3',
	'WRGive wood 80', 'WRGive stone 50', 'WRGive fiber 30', 'WRGive hide 8', 'WRGive raw_meat 6', 'WRGive berries 8',
	'WRGive copper_ore 12', 'WRGive tin_ore 4', 'WRGive kibble 3', 'WRGive narcotic 2', 'WRGive cooked_meat 2',
	'WRGive hunting_bow 1', 'WRGive stone_arrow 20', 'WRGive tranq_arrow 10', 'WRGive stone_hatchet 1', 'WRGive stone_pick 1',
	'WRGive hide_armor 1', 'WREquip hide_armor', 'WRGivePoints fire 10', 'WRAutoSpend fire 10', 'wait 2') }
$labels = @()
foreach ($s in $screens.GetEnumerator()) {
	$cmds += $s.Value
	if ($Only.Count -eq 0 -or $Only -contains $s.Key -or $selfShots -contains $s.Key) {
		if ($selfShots -notcontains $s.Key) { $cmds += $shot }
		$labels += $s.Key
	}
}
$cmds += 'wait 2'
$tune = '-wrtune=taming.easySeconds:4'
if ($Flow) { $tune += ' -wrpreset=pve' }
if ($MainMenu) {
	# No server: the island on its own, as the game starts. A local stand-in for GitHub says the next version is out,
	# so the update banner shows too.
	$version = ((Get-Content (Join-Path $root 'Config\DefaultGame.ini') | Where-Object { $_ -match '^ProjectVersion=' } | Select-Object -First 1) -replace '^ProjectVersion=', '').Trim()
	$parts = @($version -split '\.'); $parts[-1] = [string]([int]$parts[-1] + 1)
	$feedPort = $Port + 1000
	[System.IO.File]::WriteAllText("$logs\feed.mjs", "import http from 'http'; http.createServer((q, r) => { r.writeHead(200, { 'Content-Type': 'application/json' }); r.end(JSON.stringify({ tag_name: 'v$($parts -join '.')' })); }).listen($feedPort, '127.0.0.1');")
	$s = Start-Process node -ArgumentList "`"$logs\feed.mjs`"" -PassThru -WindowStyle Hidden
	$c = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?Path=fire?WRName=${name}?Body=f -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot -wrworld=uishotsmenu -wrupdatefeed=http://127.0.0.1:$feedPort/latest `"-ini:GameUserSettings:[Wildreach.Menu]:Name=$name,[Wildreach.Menu]:Body=f,[Wildreach.Menu]:Path=fire`" `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
} else {
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=uishots $tune -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 30
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=f -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
}
# Every screen's commands run (even with -Only), so the time allowed grows with the whole list.
$deadline = (Get-Date).AddSeconds(240 + 12 * $screens.Count + $(if ($screens.Contains('flow_death')) { 120 } else { 0 }))
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	$n = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started }).Count
	if ($n -ge $labels.Count) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_uishots.json'), (Join-Path $root 'Saved\ServerDB\world_uishotsmenu.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$files = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' | Where-Object { $_.LastWriteTime -gt $started } | Sort-Object LastWriteTime)
for ($i = 0; $i -lt $files.Count -and $i -lt $labels.Count; $i++) {
	$dest = Join-Path $Out ('{0:D2}_{1}.png' -f ($i + 1), $labels[$i])
	Copy-Item $files[$i].FullName $dest -Force
	$dest
}
$errors = @(Select-String -Path "$logs\client.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Select-Object -First 5)
if ($errors) { "client errors:"; $errors | ForEach-Object { '  ' + $_.Line.Trim() } }
"{0} of {1} screens captured (logs: {2})" -f [Math]::Min($files.Count, $labels.Count), $labels.Count, $logs
$lock.ReleaseMutex()
