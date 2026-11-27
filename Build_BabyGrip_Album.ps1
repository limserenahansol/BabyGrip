param(
  # Toolchain folder made by Setup_BabyGrip_PC.ps1 (arduino-cli + ESP32 core 3.1.1).
  [string]$Setup = $(if ($env:BABYGRIP_SETUP) { $env:BABYGRIP_SETUP } else { Join-Path $HOME 'Downloads\BabyGrip_Setup' })
)
$ErrorActionPreference = 'Stop'
# Builds the album firmware. The two libraries come from this repository's
# libraries\ folder (Waveshare's LVGL 8.3.10 with its src\lv_conf.h, and
# ESP32-audioI2S 2.0.0); --libraries gives them priority over any copy in the
# toolchain folder. partitions.csv in the sketch folder replaces the scheme's
# table; the app partition (app0) is 0x340000 bytes, so the size check uses that limit.
$root = $PSScriptRoot
$cli = Join-Path $Setup 'bin\arduino-cli.exe'
$config = Join-Path $Setup 'arduino-cli.yaml'
if (-not (Test-Path $cli) -or -not (Test-Path $config)) {
  throw "No toolchain in $Setup. Run Setup_BabyGrip_PC.ps1 first (or pass -Setup <folder>)."
}
$board = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc,PartitionScheme=app3M_fat9M_16MB'
& $cli --config-file $config compile --fqbn $board --jobs 8 `
  --build-property 'upload.maximum_size=3407872' `
  --libraries (Join-Path $root 'libraries') `
  --build-path (Join-Path $root 'build') --output-dir (Join-Path $root 'firmware') `
  (Join-Path $root 'sketch\BabyGrip_WiFi')
if ($LASTEXITCODE -ne 0) { throw "BabyGrip album compilation failed (exit $LASTEXITCODE)." }
$app = Get-Item (Join-Path $root 'firmware\BabyGrip_WiFi.ino.bin')
if ($app.Length -gt 0x340000) { throw "Firmware is $($app.Length) bytes, larger than app0 (3407872)." }
"Firmware OK: $($app.Length) of 3407872 bytes"
