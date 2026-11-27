param(
  # Where the toolchain goes. Build_BabyGrip_Album.ps1 and Flash_BabyGrip_Album.ps1
  # look here by default (or wherever BABYGRIP_SETUP points).
  [string]$Setup = $(if ($env:BABYGRIP_SETUP) { $env:BABYGRIP_SETUP } else { Join-Path $HOME 'Downloads\BabyGrip_Setup' })
)
# One-time setup of a new Windows PC for BabyGrip, without Arduino IDE:
#   arduino-cli 1.6.0 and the Espressif ESP32 Arduino core 3.1.1
#   (about 1.1 GB to download and 5 GB on disk).
# The two libraries (LVGL 8.3.10, ESP32-audioI2S 2.0.0) come with the
# repository in libraries\, so nothing else is installed. Safe to run again.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # the progress bar makes downloads very slow in Windows PowerShell 5.1
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$cliVersion = '1.6.0'
$coreVersion = '3.1.1'
$indexUrl = 'https://espressif.github.io/arduino-esp32/package_esp32_index.json'

New-Item -ItemType Directory -Force (Join-Path $Setup 'bin') | Out-Null
$cli = Join-Path $Setup 'bin\arduino-cli.exe'
if (-not (Test-Path $cli)) {
  $zip = Join-Path $Setup "arduino-cli_${cliVersion}_Windows_64bit.zip"
  "== Downloading arduino-cli $cliVersion"
  Invoke-WebRequest -UseBasicParsing -OutFile $zip `
    -Uri "https://downloads.arduino.cc/arduino-cli/arduino-cli_${cliVersion}_Windows_64bit.zip"
  Expand-Archive -Path $zip -DestinationPath (Join-Path $Setup 'bin') -Force
  Remove-Item $zip
}
& $cli version
if ($LASTEXITCODE -ne 0) { throw "arduino-cli does not run ($cli)." }

$config = Join-Path $Setup 'arduino-cli.yaml'
if (-not (Test-Path $config)) {
  $s = $Setup -replace '\\', '/'
  $yaml = @"
board_manager:
  additional_urls:
    - $indexUrl
directories:
  data: $s/toolchain
  downloads: $s/cache
  user: $s/vendor
updater:
  enable_notification: false
"@
  # UTF-8 without BOM, so a user folder with Korean letters still works
  [IO.File]::WriteAllText($config, $yaml, (New-Object Text.UTF8Encoding $false))
  "== Wrote $config"
}

"== Installing the ESP32 core $coreVersion (about 1.1 GB; this takes a while)"
& $cli --config-file $config core update-index
if ($LASTEXITCODE -ne 0) { throw 'arduino-cli core update-index failed (internet connection?).' }
& $cli --config-file $config core install "esp32:esp32@$coreVersion"
if ($LASTEXITCODE -ne 0) { throw "arduino-cli core install esp32:esp32@$coreVersion failed." }

# The files the build and flash scripts use
foreach ($rel in @(
    "toolchain\packages\esp32\hardware\esp32\$coreVersion\tools\partitions\boot_app0.bin",
    'toolchain\packages\esp32\tools\esptool_py\4.9.dev3\esptool.exe')) {
  if (-not (Test-Path (Join-Path $Setup $rel))) { throw "Missing after install: $rel" }
}
"== Ready: $Setup"
"Serial ports now: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')"
'Next: Build_BabyGrip_Album.ps1, then Flash_BabyGrip_Album.ps1 -Port COMx (never -FirstInstall).'
