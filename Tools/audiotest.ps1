# The sound pass, played through: a server and a rendered client WITH sound on L_Island, the bot walking through day
# and night, gathering, eating, crafting, building (a door), a fight, a bow, a boss, the tundra, the coast, a cave and
# the menus, with every sound logged (-wrsoundlog: event, file, volume, where) and the mixed output recorded to a WAV.
# Then it checks each expected event played, nothing was spammed, the ambient beds and music followed the place and the
# fight, and the recording isn't silent where sounds played. You can't hear a log: this is the proof it plays.
#   audiotest.ps1 [-Port 7863] [-Out <folder>] [-MainMenu]
param([int]$Port = 7863, [string]$Out = '', [switch]$MainMenu)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
if (-not $Out) { $Out = Join-Path $env:TEMP ("wildreach-audiotest-" + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Force $Out | Out-Null
$name = 'Earwig'
Remove-Item (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
$bounce = Join-Path $root 'Saved\BouncedWavFiles'
Remove-Item (Join-Path $bounce 'audiotest*.wav') -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
# The engine mutes a window without focus (UnfocusedVolumeMultiplier 0): the test client runs behind other windows.
$focus = '"-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0"'
$started = Get-Date

if ($MainMenu) {
	# The front end on its own: the menu theme over the island's ambience, and the settings' Audio page.
	# The settings window clicked with real Slate input: Video, Audio, a volume slider dragged, Done.
	$cmds = @('WRBotPause 1', 'wait 2', 'WRSoundRecord start 120', 'WRMainMenu', 'wait 14', 'WRSoundStats', 'WRMainMenu settings', 'wait 2', 'WRSettings audio', 'wait 1.5',
		'shot showui', 'wait 2', 'WRUIMouse click 203 273', 'wait 1', 'WRUIMouse click 204 235', 'wait 1', 'WRUIMouse drag 1025 219 900 219', 'wait 1',
		'WRUIMouse click 1080 641', 'wait 2', 'WRSoundStats', 'WRSoundRecord stop audiotest_menu', 'wait 3')
	$c = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?Path=fire?WRName=${name} -game -windowed -ResX=1280 -ResY=720 -unattended -wrbot -wrsoundlog -wrworld=audiotestmenu $focus `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$Out\client.log`"" -PassThru
	$s = $null
} else {
	$cmds = @(
		'WRBotPause 1', 'wait 3', 'WRSoundStats reset', 'WRSoundRecord start 900',
		# Day on the volcanic lowlands: ash underfoot, rumble and crackle, birds.
		'WRTeleport -51623 54155', 'WRHour 10', 'wait 12', 'WRSoundFootstep', 'wait 1', 'WRSoundStats',
		# Gathering: swings at trees and rocks.
		'WRGather tree', 'wait 14', 'WRGather rock', 'wait 12', 'WRGather off', 'wait 1',
		# Eating, drinking, a potion, equipping.
		'WRGive cooked_meat 2', 'WRGive waterskin 2', 'WRGive minor_healing_potion 4', 'WRHitPlayer Earwig 30', 'wait 1',
		'WRCmd use food', 'wait 2', 'WRCmd use drink', 'wait 2', 'WRCmd use potion', 'wait 2',
		'WRGive copper_sword 1', 'WREquip copper_sword', 'wait 1.5', 'WRUnequip weapon', 'wait 1.5',
		# Crafting by hand.
		'WRGive wood 300', 'WRGive stone 200', 'WRGive fiber 20', 'WRGive copper_ingot 10', 'WRCraft stone_arrow none', 'wait 3',
		# A floor and a door (the survival test's hut site), opened and shut from 3 m away; footsteps on wood.
		'WRTeleport -131300 -56400', 'wait 5', 'WRPlace wood_foundation -132000 -56400 4734 0', 'WRPlace wood_door -131800 -56400 4834 90', 'wait 2',
		'WRTeleport -131550 -56400', 'wait 3', 'WRInteract', 'wait 2', 'WRInteract', 'wait 2', 'WRTeleport -132000 -56400', 'wait 3', 'WRSoundFootstep', 'wait 1',
		# A claim, then a core into a Hearth inside it.
		'WRGive core_vulkarn 1', 'WRPlace claim_totem -131100 -56000 4847 0', 'wait 1', 'WRPlace hearth -131300 -56800 4838 0', 'wait 1', 'WRGoTo hearth', 'wait 1', 'WRCmd hearthfeed', 'wait 2',
		# A fight: a Cinderfang and the bot (kept alive: the music should follow the fight, not a respawn).
		'WRTeleport -51623 54155', 'wait 6', 'WRHeal', 'WRSpawn cinderfang 1', 'wait 3', 'WRCreatureHP cinderfang 0.3', 'WRBotPause 0', 'wait 7', 'WRHeal', 'wait 7', 'WRHeal', 'wait 7', 'WRHeal', 'wait 5',
		'WRBotPause 1', 'WRClearCreatures', 'WRHeal', 'wait 12', 'WRSoundStats',
		# The bow.
		'WRGive hunting_bow 1', 'WRGive stone_arrow 10', 'WREquip hunting_bow', 'wait 1', 'WRRanged 1', 'wait 1.5', 'WRAim 1', 'wait 1', 'WRFire', 'wait 2', 'WRFire', 'wait 2', 'WRAim 0', 'WRRanged 0', 'wait 1',
		# A boss: Skaldrith in his arena (the boss the dev damage command reaches), healed through it, then killed.
		'WRTeleport 77000 0', 'wait 6', 'WRHeal', 'WRBossDmgPct 5 none', 'wait 5', 'WRHeal', 'wait 5', 'WRHeal', 'WRSoundStats',
		'WRBossDmg 9999999 none', 'wait 3', 'WRHeal', 'wait 9',
		# Night on the volcanic side, then the tundra, the coast and a cave.
		'WRTeleport -51623 54155', 'wait 4', 'WRHour 23', 'wait 16', 'WRSoundStats',
		'WRTeleport 90000 40000', 'wait 12', 'WRSoundFootstep', 'wait 1', 'WRSoundStats',
		'WRHour 10', 'WRTeleport -163000 -20000', 'wait 12', 'WRSoundFootstep', 'wait 1', 'WRSoundStats',
		'WRTeleport -59600 71673', 'wait 5', 'WRWalkTo -57104 71673', 'wait 12', 'WRSoundStats',
		# The menus with real Slate input: the inventory (a screenshot), the settings' Audio page (a screenshot), its nav,
		# a volume slider dragged, Reset to defaults, Resume.
		'WRTeleport -51623 54155', 'wait 5', 'WRMenu inventory 0', 'wait 1.5', 'shot showui', 'wait 2', 'WRMenu none 0', 'wait 1.5',
		'WRMenu settings 0', 'wait 1.5', 'WRSettings audio', 'wait 1.5', 'shot showui', 'wait 2',
		'WRUIMouse click 218 197', 'wait 1', 'WRUIMouse click 204 235', 'wait 1', 'WRUIMouse drag 1025 219 900 219', 'wait 1', 'WRSoundStats',
		'WRUIMouse click 210 641', 'wait 1', 'WRUIMouse click 1080 641', 'wait 1.5',
		# Music modes by hand, then the moments and UI events from the table.
		'WRMusic combat', 'wait 6', 'WRMusic boss', 'wait 6', 'au.DumpActiveSounds', 'WRMusic explore', 'wait 8', 'WRMusic auto', 'wait 2',
		'WRSoundPlay levelup 2d', 'wait 2', 'WRSoundPlay recipe.unlock 2d', 'wait 2', 'WRSoundPlay discover 2d', 'wait 2', 'WRSoundPlay tame.success 2d', 'wait 2',
		'WRSoundPlay boss.intro 2d', 'wait 8', 'WRSoundPlay respawn 2d', 'wait 2',
		'WRSoundPlay ui.hover 2d', 'wait 0.5', 'WRSoundPlay ui.drag 2d', 'wait 0.5', 'WRSoundPlay ui.drop 2d', 'wait 0.5', 'WRSoundPlay ui.error 2d', 'wait 1',
		# Spam check: ten of one event at once: the minimum gap lets one through.
		'WRSoundPlay ui.click 2d 10', 'wait 1', 'WRSoundPlay no.such.event', 'WRSoundPlay no.such.event', 'wait 1',
		'WRSoundStats', 'WRSoundRecord stop audiotest', 'wait 4')
	$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=audiotest -abslog=`"$Out\server.log`"" -PassThru -WindowStyle Hidden
	Start-Sleep -Seconds 30
	$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=f -game -windowed -ResX=1280 -ResY=720 -unattended -wrbot -wrsoundlog $focus `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$Out\client.log`"" -PassThru
}
$timeout = if ($MainMenu) { 200 } else { 900 }
$deadline = (Get-Date).AddSeconds($timeout)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	if (Test-Path "$Out\client.log") {
		if (Select-String -Path "$Out\client.log" -Pattern 'SOUND record stop' -Quiet) { Start-Sleep -Seconds 6; break }
	}
}
foreach ($p in @($c, $s)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_audiotest.json'), (Join-Path $root 'Saved\ServerDB\world_audiotestmenu.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
foreach ($f in @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started })) { Copy-Item $f.FullName (Join-Path $Out ('audio_page_' + $f.Name)) }
foreach ($f in @(Get-ChildItem $bounce -Filter 'audiotest*.wav' -ErrorAction SilentlyContinue)) { Move-Item $f.FullName (Join-Path $Out $f.Name) -Force }
"logs and recording: $Out"
# The checks (Tools/audio/check_log.py reads the log and the WAV; Python with numpy, or just the log without it).
$py = Get-Command python -ErrorAction SilentlyContinue
if ($py) { & python -I (Join-Path $PSScriptRoot 'audio\check_log.py') "$Out\client.log" $(if ($MainMenu) { 'menu' } else { 'game' }) (Get-ChildItem $Out -Filter '*.wav' | Select-Object -First 1 -ExpandProperty FullName) }
