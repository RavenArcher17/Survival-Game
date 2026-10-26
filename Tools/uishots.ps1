# Screenshots of the stone-and-leather UI (Slate, so "shot showui": HighResShot leaves the UI out). A rendered client
# on L_Island with a stocked bag, learned abilities and a tame opens each screen, saving one numbered picture per
# screen into -Out. Add screens to $screens as they're built.
#   uishots.ps1 [-Out <folder>] [-Only hud,wheel]
param([string]$Out = '', [string[]]$Only = @(), [int]$Port = 7845)
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
	'inventory'    = 'WRHelp|WRMenu inventory 0|wait 1'
	'crafting'     = 'WRMenu none 0|WRMenu crafting 0|wait 1'
	'paths'        = 'WRMenu paths 0|wait 1'
	'skills'       = 'WRMenu skills 0|wait 1'
	'map'          = 'WRMenu none 0|WRMenu map 0|wait 1'
	'build'        = 'WRMenu map 0|WRMenu build 0|wait 1'
	# The Ashstrider tame stays behind (E would pick it over the knocked-out Cinderfang); the Taming screen, fed meat.
	'taming'       = 'WRMenu none 0|WRMenu build 0|WRMenu none 0|WRTameAll stay|WRTeleport -51623 57155|wait 3|WRLook 270 -8|WRSpawn cinderfang 1|WRHold 60|wait 1|WRGoTo cinderfang|WRTorpor 1000|wait 1.5|WRInteract|wait 1|WRTameGive raw_meat|wait 3'
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
}
if ($Only.Count -gt 0) { $Only = $Only | ForEach-Object { $_ -split ',' } }
$cmds = @('WRBotPause 1', 'wait 3',
	'WRGive wood 80', 'WRGive stone 50', 'WRGive fiber 30', 'WRGive hide 8', 'WRGive raw_meat 6', 'WRGive berries 8',
	'WRGive copper_ore 12', 'WRGive tin_ore 4', 'WRGive kibble 3', 'WRGive narcotic 2', 'WRGive cooked_meat 2',
	'WRGive hunting_bow 1', 'WRGive stone_arrow 20', 'WRGive tranq_arrow 10', 'WRGive stone_hatchet 1', 'WRGive stone_pick 1',
	'WRGive hide_armor 1', 'WREquip hide_armor', 'WRGivePoints fire 10', 'WRAutoSpend fire 10', 'wait 2')
$labels = @()
foreach ($s in $screens.GetEnumerator()) {
	$cmds += $s.Value
	if ($Only.Count -eq 0 -or $Only -contains $s.Key) { $cmds += $shot; $labels += $s.Key }
}
$cmds += 'wait 2'
$tune = '-wrtune=taming.easySeconds:4'
$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island -server -nullrhi -unattended -nosound -port=$Port -wrworld=uishots $tune -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$c = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=${name}?Body=f -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 60`" `"-wrbotcmds=$($cmds -join '|')`" -abslog=`"$logs\client.log`"" -PassThru
# Every screen's commands run (even with -Only), so the time allowed grows with the whole list.
$deadline = (Get-Date).AddSeconds(120 + 12 * $screens.Count)
while ((Get-Date) -lt $deadline -and -not $c.HasExited) {
	Start-Sleep -Seconds 5
	$n = @(Get-ChildItem $shotDir -Filter 'ScreenShot*.png' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $started }).Count
	if ($n -ge $labels.Count) { Start-Sleep -Seconds 3; break }
}
foreach ($p in $c, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
Remove-Item (Join-Path $root 'Saved\ServerDB\world_uishots.json'), (Join-Path $root "Saved\ServerDB\players\$name.json") -ErrorAction SilentlyContinue
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
