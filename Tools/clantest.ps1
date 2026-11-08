# Clans by the journal's Clan tab (O): until now they only went through the console (WRClan ...). Mason hosts L_Island
# (no window); Bravo joins in a 1440x900 window and plays the tab by real keys and clicks; Mason answers by console. The
# two keep in step by chat ("waitchat").
#  1. Bravo founds "River Wolves" (Found a clan, the name typed, Enter), invites Mason from the players in the game,
#     Mason joins, Bravo removes him; invites "Ghost" (not on) by name and takes it back; leaves, and the clan ends.
#  2. Mason founds "Ash" and invites Bravo: the invite shows in Bravo's tab; Turn down; invited again, Accept: Bravo is in
#     Mason's clan.
# -Shots keeps Bravo's pictures.
#   clantest.ps1 [-Port 7893] [-Shots] [-Out <folder>]
param([int]$Port = 7893, [switch]$Shots, [string]$Out = '')
function Shot { if ($Shots) { @('wait 0.8', 'shot showui', 'wait 1.2') } else { @() } }
function Screen([string]$tag) { @("WRSay $tag", "waitchat $tag", 'WRUIState', 'WRUIText') }
$X = -132000; $Y = -56400
$mason = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'WRHour 11', 'wait 2', "WRTeleport $($X - 600) $Y", 'wait 8', 'WRSay host-ready',
	'waitchat bravo-invited-mason', 'wait 1', 'WRCmd clan join River Wolves', 'wait 1', 'WRSay mason-joined',
	'waitchat bravo-left', 'wait 1', 'WRClan create Ash', 'wait 0.5', 'WRClan invite Bravo', 'wait 1', 'WRSay mason-invited-1',
	'waitchat bravo-turned-down', 'wait 1', 'WRClan invite Bravo', 'wait 1', 'WRSay mason-invited-2',
	'waitchat bravo-done', 'wait 1', 'WRClan info', 'wait 1', 'WRSay clan-done')
$bravo = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 8', "WRTeleport $($X - 400) $($Y + 300)", 'wait 2',
	# 1. Found, invite, join, remove, invite by name, take back, leave.
	'WRKey O', 'wait 1.5') + (Shot) + (Screen 'ui-none') +
	@('WRClick Found a clan', 'wait 1', 'WRType River Wolves', 'wait 0.3', 'WRKey Enter', 'wait 1.5') + (Screen 'ui-founded') +
	@('WRClick Invite @ Mason', 'wait 1.5') + (Screen 'ui-invited') + @('WRSay bravo-invited-mason', 'waitchat mason-joined', 'wait 1.5') + (Shot) + (Screen 'ui-joined') +
	@('WRClick Remove @ Mason', 'wait 1.5') + (Screen 'ui-removed') +
	@('WRClick Invite by name', 'wait 1', 'WRType Ghost', 'wait 0.3', 'WRKey Enter', 'wait 1.5') + (Shot) + (Screen 'ui-ghost') +
	@('WRClick Take back', 'wait 1.5') + (Screen 'ui-taken-back') +
	@('WRClick Leave clan', 'wait 1') + (Shot) + (Screen 'ui-leave-ask') + @('WRClick Yes, leave', 'wait 1.5') + (Screen 'ui-left') + @('WRSay bravo-left',
	# 2. Mason's invites: turned down, then accepted.
	'waitchat mason-invited-1', 'wait 1.5') + (Shot) + (Screen 'ui-invite-1') + @('WRClick Turn down', 'wait 1.5') + (Screen 'ui-turned-down') + @('WRSay bravo-turned-down',
	'waitchat mason-invited-2', 'wait 1.5', 'WRClick Accept', 'wait 1.5') + (Shot) + (Screen 'ui-accepted') + @('WRKey O', 'wait 1', 'WRSay bravo-done')
$shotNames = @('no_clan', 'clan_with_mason', 'invite_by_name', 'leave_ask', 'invite_from_mason', 'in_masons_clan')

$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')   # one rendered UI run at a time
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ("wildreach-clan-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $Out) { $Out = Join-Path $logs 'shots' }
New-Item -ItemType Directory -Force $logs | Out-Null
$db = Join-Path $root 'Saved\Worlds\clantest'
Remove-Item -Recurse -Force $db -ErrorAction SilentlyContinue
$shotDir = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$started = Get-Date
$hostLog = Join-Path $logs 'host.log'; $friendLog = Join-Path $logs 'friend.log'
$h = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Island?listen?WRName=Mason?Path=fire?Body=m -game -nullrhi -unattended -nosound -port=$Port -wrworld=clantest -wrtune=wildlife.enabled:0 -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($mason -join '|')`" -abslog=`"$hostLog`"" -PassThru -WindowStyle Hidden
$f = $null
$deadline = (Get-Date).AddSeconds(360)
while ((Get-Date) -lt $deadline -and -not $h.HasExited) {
	Start-Sleep -Seconds 3
	if (-not $f -and (Select-String -Path $hostLog -Pattern 'chat Mason: host-ready' -Quiet -ErrorAction SilentlyContinue)) {
		$f = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=Bravo?Body=f -game -windowed -ResX=1440 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 45`" `"-wrbotcmds=$($bravo -join '|')`" -abslog=`"$friendLog`"" -PassThru
	}
	if (Select-String -Path $hostLog -Pattern 'chat Mason: clan-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 2; break }
}
foreach ($p in @($f, $h)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2
$clans = Join-Path $db 'clans.json'
$saved = if (Test-Path $clans) { Get-Content $clans -Raw | ConvertFrom-Json } else { $null }
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
$hl | Where-Object { $_ -match '^CLAN \S+: ' } | ForEach-Object { "  host:   $_" }
$fl | Where-Object { $_ -match '^NOTICE (?!chat)' } | ForEach-Object { "  friend: $_" }
function Index($lines, [string]$pattern, [int]$after = -1) { for ($i = $after + 1; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { return $i } }; return -1 }
function Screen([string]$marker) {
	$from = Index $fl "^NOTICE chat Bravo: $marker$"
	if ($from -lt 0) { return @() }
	$to = Index $fl '^NOTICE chat Bravo: ' $from
	if ($to -lt 0) { $to = $fl.Count }
	@($fl[$from..($to - 1)] | Where-Object { $_ -like 'UITEXT*' -or $_ -like 'UISTATE*' } | ForEach-Object { $_ -replace '^UITEXT\s+-?\d+,\s*-?\d+ ', '' })
}
function Has($texts, [string]$exact) { @($texts | Where-Object { $_ -eq $exact }).Count -ge 1 }
# (WRUIText cuts a line at about 60 characters: a long one is matched by its start.)
function Like($texts, [string]$start) { @($texts | Where-Object { $_ -like $start }).Count -ge 1 }
function Menu($texts) { $s = $texts | Where-Object { $_ -like 'UISTATE*' } | Select-Object -First 1; if ($s -match 'menu=(\S+)') { $Matches[1] } else { '?' } }
function Notice([string]$pattern) { (Index $fl "^NOTICE $pattern") -ge 0 }
$none = Screen 'ui-none'; $founded = Screen 'ui-founded'; $invited = Screen 'ui-invited'; $joined = Screen 'ui-joined'; $removed = Screen 'ui-removed'
$ghost = Screen 'ui-ghost'; $back = Screen 'ui-taken-back'; $ask = Screen 'ui-leave-ask'; $left = Screen 'ui-left'
$inv1 = Screen 'ui-invite-1'; $down = Screen 'ui-turned-down'; $acc = Screen 'ui-accepted'

Check 'O opens the Clan tab: no clan, Found a clan, no invites' ((Menu $none) -eq 'clan' -and (Has $none 'No clan yet') -and (Has $none 'Found a clan') -and (Like $none 'No invites. A clan*'))
Check 'a clan founded by its name typed and Enter' ((Notice 'Founded clan River Wolves') -and (Has $founded 'River Wolves') -and (Has $founded 'You lead it') -and (Has $founded '1 member'))
Check 'Invite on Mason''s row in the players here: he''s told how to accept' ((Index $hl '^NOTICE Bravo invited you to clan River Wolves: accept it in the Clan tab \(O\)') -ge 0 -and (Has $invited 'Invited, not joined yet') -and (Has $invited 'Take back'))
Check 'Mason joins: Bravo is told, and he''s on the members list' ((Notice 'Mason joined the clan') -and (Has $joined '2 members') -and (Has $joined 'Remove') -and (Has $joined 'your clan'))
Check 'Remove takes him out, and he''s told' ((Index $hl '^NOTICE Bravo removed you from clan River Wolves') -ge 0 -and (Has $removed '1 member') -and -not (Has $removed 'your clan'))
Check 'Invite by name reaches someone who isn''t on' ((Notice 'Invited Ghost to clan River Wolves') -and (Has $ghost 'Ghost') -and (Has $ghost 'invited'))
Check 'Take back withdraws it' ((Notice 'Took back the invite to Ghost') -and -not (Has $back 'Ghost'))
Check 'Leave asks first, then the clan ends with its last member' ((Has $ask 'Leave River Wolves? It ends with you.') -and (Notice 'Left clan River Wolves') -and (Has $left 'No clan yet'))
Check 'Mason''s invite shows in the tab, from him' ((Notice 'Mason invited you to clan Ash: accept it in the Clan tab \(O\)') -and (Has $inv1 'Ash') -and (Has $inv1 'from Mason') -and (Has $inv1 'Accept') -and (Has $inv1 'Turn down'))
Check 'Turn down declines it' ((Notice 'Turned down clan Ash') -and (Like $down 'No invites. A clan*'))
Check 'invited again, Accept joins Mason''s clan' ((Notice 'Joined clan Ash') -and (Has $acc 'Ash') -and (Has $acc 'Leader: Mason') -and (Has $acc 'Mason invites new members.') -and (Index $hl '^NOTICE Bravo joined the clan') -ge 0)
Check 'the clans are saved: Ash with Mason leading and Bravo' ($saved -and $saved.Ash -and $saved.Ash.leader -eq 'Mason' -and @($saved.Ash.members) -contains 'Bravo' -and -not $saved.'River Wolves') $(if ($saved) { $saved | ConvertTo-Json -Compress } else { 'no clans.json' })
$missed = @($fl | Where-Object { $_ -match '^CLICK .*(not on screen|reaches=0)' })
Check 'every click lands' ($missed.Count -eq 0) (($missed | Select-Object -First 2) -join ' | ')
$errors = @(Select-String -Path $hostLog, $friendLog -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue | Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot' })
Check 'no errors in either log' ($errors.Count -eq 0) (($errors | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() }) -join ' | ')
foreach ($r in $results) { "{0,-5} {1}{2}" -f $(if ($r.ok) { 'PASS' } else { 'FAIL' }), $r.what, $(if ($r.detail) { "  ($($r.detail))" } else { '' }) }
$failed = @($results | Where-Object { -not $_.ok }).Count
"{0} checks, {1} failed (logs: {2})" -f $results.Count, $failed, $logs
if ($failed -gt 0) { exit 1 }
