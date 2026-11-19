# The round island's first slice in play (plan phase 3, slice 1): a dedicated server on L_Wildreach, a rendered walker
# and a second (headless) player joining. The walker starts on Hearthmoot's plaza, measures the frame rate looking over
# the city, walks out along the north-west avenue and through its gate (the houses stand beside the street: a body that
# walks through one, or sticks on one, shows here), walks on into the north-west fort through its gate, measures the
# frame rate out on the meadow, and swims in the lake. The visitor joins, walks to the market and says so.
# Checks: both are in, every walk arrives, the walker swims in the lake, the frame times, no errors in the logs.
#   roundtest.ps1 [-Port 7880]
param([int]$Port = 7880)
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$lock = New-Object System.Threading.Mutex($false, 'Global\WildreachUIShots')
try { [void]$lock.WaitOne() } catch [System.Threading.AbandonedMutexException] { }
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$root = Split-Path $project
$logs = Join-Path $env:TEMP ('wildreach-round-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $logs | Out-Null
$db = Join-Path $root 'Saved\Worlds\roundtest'
foreach ($n in 'Walker', 'Visitor') { Remove-Item (Join-Path $db "players\$n.json") -ErrorAction SilentlyContinue }
Remove-Item (Join-Path $db 'world.json') -ErrorAction SilentlyContinue

# Game cm (map metres x 100): the north-west avenue's own line (Tools/terrain/hearthmoot_plan.mjs) from the plaza out to
# the gate at bearing 326, then out of it onto the main road.
$avenue = @('-5244 -6976', '-10593 -14777', '-15668 -22766', '-23500 -34800')
$walker = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 25', 'WRHour 12', 'stat unit', 'wait 2',
	'WRLook 270 -5', 'wait 3', 'wr.FrameStats 6', 'wait 8') +
	@($avenue | ForEach-Object { "WRWalkTo $_"; 'wait 45' }) +
	@('WRTeleport -66000 -97000', 'wait 6', 'WRWalkTo -77000 -113900', 'wait 40',
	'WRLook 56 -3', 'wait 2', 'wr.FrameStats 6', 'wait 8',
	# Off the beach by the jetty, wading into the lake as a player would (a body dropped under the water holds its depth:
	# the underwater rules), then a look at the water from a swimmer's place.
	'WRTeleport 23600 72800', 'wait 6', 'WRWalkTo 25600 79000', 'wait 25', 'wr.Water lake2', 'WRCmd water stat lake', 'wait 2',
	'WRTeleport 0 3000', 'wait 5', 'WRSay walker-done', 'waitchat visitor-done', 'wait 2')
$visitor = @('WRBotPause 1', 'WRFlow quiet on', 'WRFlow hints off', 'wait 30', 'WRWalkTo 2600 -900', 'wait 25', 'WRSay visitor-here',
	'waitchat walker-done', 'wait 1', 'WRSay visitor-done', 'wait 3')

$s = Start-Process $ue -ArgumentList "`"$project`" /Game/Maps/L_Wildreach -server -nullrhi -unattended -nosound -port=$Port -wrworld=roundtest -wrstore=json -wrdefaultpreset=playtest -abslog=`"$logs\server.log`"" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 30
$w = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=fire?WRName=Walker -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -wrbot `"-ExecCmds=t.MaxFPS 120`" `"-wrbotcmds=$($walker -join '|')`" -abslog=`"$logs\walker.log`"" -PassThru
Start-Sleep -Seconds 12
$v = Start-Process $ue -ArgumentList "`"$project`" 127.0.0.1:${Port}?Path=frost?WRName=Visitor -game -nullrhi -unattended -nosound -wrbot `"-wrbotcmds=$($visitor -join '|')`" -abslog=`"$logs\visitor.log`"" -PassThru
$deadline = (Get-Date).AddSeconds(600)
while ((Get-Date) -lt $deadline -and -not ($w.HasExited -and $v.HasExited)) {
	Start-Sleep -Seconds 5
	if (Select-String -Path "$logs\walker.log" -Pattern 'chat Visitor: visitor-done' -Quiet -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 4; break }
}
foreach ($p in $w, $v, $s) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Start-Sleep -Seconds 2

$fail = 0
function Check([string]$what, [bool]$ok, [string]$detail) { if ($ok) { "  ok    $what  ($detail)" } else { "  FAIL  $what  ($detail)"; $script:fail++ } }
$arrived = @(Select-String -Path "$logs\walker.log" -Pattern 'WALK arrived after' | ForEach-Object { $_.Line -replace '.*WALK ', '' })
Check 'the walker walks the avenue, through the gate and into the fort, and wades into the lake' ($arrived.Count -ge 6) ("{0} of 6 walks arrived: {1}" -f $arrived.Count, ($arrived -join ' | '))
$visit = @(Select-String -Path "$logs\visitor.log" -Pattern 'WALK arrived after')
Check 'the visitor joins and walks to the market' ($visit.Count -ge 1) ("{0} walk arrived" -f $visit.Count)
$both = (Select-String -Path "$logs\server.log" -Pattern 'Walker' -Quiet) -and (Select-String -Path "$logs\server.log" -Pattern 'Visitor' -Quiet)
Check 'the server has both players' $both 'server log names both'
$lake = @(Select-String -Path "$logs\walker.log" -Pattern 'WATER client \[lake2\]' | Select-Object -Last 1)
$lakeZ = if ($lake.Count -and $lake[0].Line -match 'at V\(X=[-\d.]+, Y=[-\d.]+, Z=([-\d.]+)\)') { [double]$Matches[1] } else { 0 }
# The lake's surface is at 16 m (Data/lands/island.json lake.level): a swimmer floats with its middle just under it.
Check 'the walker swims in the lake, at its surface' ($lake.Count -and $lake[0].Line -match 'Swimming' -and $lakeZ -gt 1450 -and $lakeZ -lt 1700) $(if ($lake.Count) { $lake[0].Line -replace '.*WATER client ', '' } else { 'no water line' })
$frames = @(Select-String -Path "$logs\walker.log" -Pattern 'LogWildreach: FRAMESTATS' | ForEach-Object { $_.Line -replace '.*FRAMESTATS ', '' })
Check 'frame times measured in the city and on the meadow' ($frames.Count -ge 2) ($frames -join ' | ')
$errors = @(Select-String -Path "$logs\server.log", "$logs\walker.log", "$logs\visitor.log" -Pattern 'Error:|Ensure condition failed|Accessed None' -ErrorAction SilentlyContinue |
	Where-Object { $_.Line -notmatch 'FDataflowToolNodeSnapshot|UnifiedErrorTest|Error test|Error with|FError that' } | Select-Object -First 5)
Check 'no errors in the logs' ($errors.Count -eq 0) $(if ($errors.Count) { ($errors | ForEach-Object { $_.Line.Trim() }) -join ' | ' } else { 'none' })
"{0} failed (logs: {1})" -f $fail, $logs
$lock.ReleaseMutex()
exit $fail
