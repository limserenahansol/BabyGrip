param(
  # put: copy MP3 files into BabyGrip's sound list (fx\*.mp3 go to the effect folder)
  # shot: save what the screen shows as a PNG
  # cmd: send one serial command (e.g. "BGPAGE feed", "BGSOUNDS", "BGSTATUS") and print the reply
  [Parameter(Mandatory = $true)][ValidateSet('put', 'shot', 'cmd')][string]$Action,
  [string]$Port = 'COM4',
  [string[]]$Files,
  [string]$Out,
  [string]$Command,
  [int]$Seconds = 4
)
# Talks to BabyGrip over USB without resetting it (DTR/RTS stay low).
$ErrorActionPreference = 'Stop'
# Python with Pillow, used only by 'shot': BABYGRIP_PYTHON, else the per-user Python 3.13, else python on PATH.
$python = @($env:BABYGRIP_PYTHON, (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\python.exe')) |
  Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $python) { $python = (Get-Command python -ErrorAction SilentlyContinue | Select-Object -First 1).Source }
if ($Action -eq 'shot' -and -not $python) { throw 'shot needs Python 3 with Pillow (pip install pillow), or set BABYGRIP_PYTHON.' }
if (-not ('BabyGripCrc' -as [type])) {
  Add-Type -TypeDefinition @'
public static class BabyGripCrc {
  public static uint Crc32(byte[] data) {
    uint crc = 0xFFFFFFFF;
    foreach (byte b in data) { crc ^= b; for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (uint)(-(int)(crc & 1))); }
    return ~crc;
  }
}
'@
}
$serial = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'One'
$serial.DtrEnable = $false; $serial.RtsEnable = $false
$serial.ReadBufferSize = 2097152; $serial.WriteBufferSize = 65536
$serial.NewLine = "`n"; $serial.ReadTimeout = 8000
function Read-Until([string[]]$prefixes, [int]$timeoutMs = 8000) {
  $deadline = (Get-Date).AddMilliseconds($timeoutMs)
  $seen = @()
  while ((Get-Date) -lt $deadline) {
    try { $line = $serial.ReadLine().TrimEnd("`r") } catch [System.TimeoutException] { continue }
    foreach ($p in $prefixes) { if ($line.StartsWith($p)) { return $line } }
    $seen = @($seen + $line) | Select-Object -Last 3
  }
  throw "No reply starting with $($prefixes -join ' / ') (last lines: $($seen -join ' | '))"
}
try {
  $serial.Open()
  Start-Sleep -Milliseconds 300
  $null = $serial.ReadExisting()
  switch ($Action) {
    'put' {
      foreach ($path in $Files) {
        $item = Get-Item $path
        $bytes = [IO.File]::ReadAllBytes($item.FullName)
        $name = if ($item.Directory.Name -eq 'fx') { "fx:$($item.Name)" } else { $item.Name }
        $crc = '{0:x8}' -f [BabyGripCrc]::Crc32($bytes)
        for ($attempt = 1; ; $attempt++) {
          $serial.Write("`nBGPUT $name $($bytes.Length) $crc`n")
          $reply = Read-Until @('[PUT] READY', '[PUT] ERR')
          if ($reply -ne '[PUT] READY') { throw "$name : $reply" }
          $started = Get-Date
          $at = 0; $resends = 0; $reply = ''
          while (-not $reply.StartsWith('[PUT] DONE') -and -not $reply.StartsWith('[PUT] ERR')) {
            if ($resends -gt 200) { throw "$name : too many resends" }
            $count = [Math]::Min(3072, $bytes.Length - $at)
            if ($count -le 0) { $reply = '[PUT] DONE (reply not received; check BGSOUNDS)'; break }
            $serial.Write("$at " + [Convert]::ToBase64String($bytes, $at, $count) + "`n")
            try { $reply = Read-Until @('[PUT] OK', '[PUT] RETRY', '[PUT] DONE', '[PUT] ERR') 1500 }
            catch { $resends++; $serial.Write("`n"); $reply = ''; continue }  # lost line: resend the same piece
            if ($reply.StartsWith('[PUT] RETRY')) { $resends++ }
            if ($reply.StartsWith('[PUT] OK') -or $reply.StartsWith('[PUT] RETRY')) { $at = [int]($reply.Split(' ')[2]) }
          }
          if ($reply.StartsWith('[PUT] DONE')) { break }
          if ($attempt -ge 3) { throw "$name : $reply" }
          "$name : $reply, sending again"
          Start-Sleep -Milliseconds 500; $null = $serial.ReadExisting()
        }
        '{0,-28} {1,8} bytes  crc {2}  {3:n1} s  resends {4}  {5}' -f $name, $bytes.Length, $crc, ((Get-Date) - $started).TotalSeconds, $resends, $reply
      }
    }
    'shot' {
      for ($attempt = 1; ; $attempt++) {
        try {
          $serial.Write("BGSHOT`n")
          $begin = Read-Until @('[SHOT] BEGIN', '[SHOT] ERR') 10000
          if (-not $begin.StartsWith('[SHOT] BEGIN')) { throw $begin }
          $size = [int]($begin.Split(' ')[4])
          $buffer = New-Object System.IO.MemoryStream
          while ($true) {
            $line = $serial.ReadLine().TrimEnd("`r")
            if ($line.StartsWith('[SHOT] END')) { $end = $line; break }
            if ($line.Length -gt 0 -and -not $line.StartsWith('[')) { $chunk = [Convert]::FromBase64String($line); $buffer.Write($chunk, 0, $chunk.Length) }
          }
          $raw = $buffer.ToArray()
          $crc = '{0:x8}' -f [BabyGripCrc]::Crc32($raw)
          if ($raw.Length -ne $size -or -not $end.EndsWith($crc)) { throw "Screenshot incomplete: $($raw.Length) of $size bytes" }
          break
        } catch {
          if ($attempt -ge 4) { throw }
          Start-Sleep -Milliseconds 800; $null = $serial.ReadExisting()
        }
      }
      $rawPath = [IO.Path]::ChangeExtension($Out, '.rgb565')
      [IO.File]::WriteAllBytes($rawPath, $raw)
      & $python -I -c "import sys; from PIL import Image; d=open(sys.argv[1],'rb').read(); px=[((v>>11&31)*255//31,(v>>5&63)*255//63,(v&31)*255//31) for v in (d[i]|d[i+1]<<8 for i in range(0,len(d),2))]; im=Image.new('RGB',(412,412)); im.putdata(px); im.save(sys.argv[2])" $rawPath $Out
      Remove-Item $rawPath
      "saved $Out"
    }
    'cmd' {
      $serial.Write("$Command`n")
      $deadline = (Get-Date).AddSeconds($Seconds); $text = ''
      while ((Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200; $text += $serial.ReadExisting() }
      $text
    }
  }
} finally { if ($serial.IsOpen) { $serial.Close() } }
