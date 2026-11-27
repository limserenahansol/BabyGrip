param(
  [string]$Port = 'COM4',
  # BGTEST adds and removes one test photo and plays sound at volume 0.
  [switch]$RunTests,
  [int]$Seconds = 6
)
# Talks to BabyGrip over USB without resetting it (DTR/RTS stay low).
$serial = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'One'
$serial.DtrEnable = $false; $serial.RtsEnable = $false; $serial.ReadTimeout = 300
function Send-Command([string]$command, [int]$wait, [string]$until) {
  $serial.Write("$command`n")
  $deadline = (Get-Date).AddSeconds($wait); $text = ''
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 200
    $text += $serial.ReadExisting()
    if ($until -and $text -match [regex]::Escape($until)) { break }
  }
  $text
}
try {
  $serial.Open()
  Start-Sleep -Milliseconds 300
  $null = $serial.ReadExisting()
  "== BGSTATUS"; Send-Command 'BGSTATUS' 3 '"ssid"'
  "== BGLIST"; Send-Command 'BGLIST' $Seconds '[LIST] END'
  if ($RunTests) { "== BGTEST"; Send-Command 'BGTEST' 240 '[CHECK] COMPLETE' }
} finally { if ($serial.IsOpen) { $serial.Close() } }
