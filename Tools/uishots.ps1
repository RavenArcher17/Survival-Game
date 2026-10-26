# Screenshots of the stone-and-leather UI (Slate, so "shot showui": HighResShot leaves the UI out). A rendered client
# on L_Island with a stocked bag, learned abilities and a tame opens each screen, saving one numbered picture per
# screen into -Out. Add screens to $screens as they're built.
#   uishots.ps1 [-Out <folder>] [-Only hud,wheel]
param([string]$Out = '', [string[]]$Only = @(), [int]$Port = 7845)
# One run at a time on this machine: each starts a server and a rendered client, and worktrees can run it side by side.
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
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
	'paths'        = 'WRGivePoints fire 8|WRAutoSpend fire 5|wait 1.5|WRMenu paths 0|wait 1.5'
	'skills'       = 'WRSkill woodcutting 25|WRSkill mining 18|WRSkill smithing 12|WRSkill cooking 4|WRSkill taming 9|WRSkill sneaking 6|WRSkill tracking 3|WRSkillXP woodcutting 1|WRSkillXP mining 8|WRSkillXP smithing 4|WRSkillXP cooking 5|WRSkillXP taming 5|WRSkillXP sneaking 4|wait 1.5|WRMenu skills 0|wait 1.5'
	'map'          = 'WRMenu none 0|WRTeleport -130000 10000|wait 6|WRGive copper_ingot 5|WRLook 0 -35|WRGhost claim_totem|wait 1|WRGhostPlace|wait 1.5|WRLook 180 -35|WRGhost hearth|wait 1|WRGhostPlace|wait 1.5|WRTeleport -120000 15000|wait 5|WRSpawn ashstrider 1|wait 1|WRGoTo ashstrider|WRInteract|wait 6|WRMenu none 0|WRLook 30 -10|wait 0.5|WRMenu map 0|wait 1.5'
	'build'        = 'WRMenu map 0|WRMenu build 0|wait 1'
	'taming'       = 'WRMenu none 0|WRMenu build 0|WRMenu none 0|WRSpawn cinderfang 1|WRHold 60|wait 1|WRGoTo cinderfang|WRTorpor 1000|wait 1.5|WRInteract|wait 1'
	'taming_hud'   = 'WRInteract|wait 1'
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
$deadline = (Get-Date).AddSeconds(120 + 12 * $labels.Count)
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
