# Launch Wildreach from the editor binaries (no packaging needed).
#   play.ps1 server           dedicated server on :7777 (console window)
#   play.ps1 client           windowed client that joins 127.0.0.1:7777
#   play.ps1 listen           windowed listen server (host + play)
#   play.ps1 local -Clients 2 dedicated server + N windowed clients
param(
	[ValidateSet('server', 'client', 'listen', 'local')] [string]$Mode = 'local',
	[int]$Clients = 2,
	[string]$Address = '127.0.0.1',
	[int]$Port = 7777
)

$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Wildreach.uproject')).Path
$map = '/Game/Maps/L_Wildreach'

function Start-Server { Start-Process $ue -ArgumentList "`"$project`" $map -server -log -port=$Port" }
function Start-Client([int]$i) {
	$x = 40 + ($i * 60); $y = 40 + ($i * 60)
	Start-Process $ue -ArgumentList "`"$project`" ${Address}:$Port -game -windowed -ResX=1280 -ResY=720 -WinX=$x -WinY=$y -log"
}

switch ($Mode) {
	'server' { Start-Server }
	'client' { Start-Client 0 }
	'listen' { Start-Process $ue -ArgumentList "`"$project`" ${map}?listen -game -windowed -ResX=1280 -ResY=720 -log" }
	'local'  { Start-Server; Start-Sleep -Seconds 8; for ($i = 0; $i -lt $Clients; $i++) { Start-Client $i } }
}
