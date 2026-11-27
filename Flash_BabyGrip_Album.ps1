param(
  [string]$Port = 'COM4',
  # Only for the first install over the old layout: clears the old second app
  # slot so it can become the media2 photo volume. Never use it for later
  # updates, because media2 then holds photos.
  [switch]$FirstInstall,
  [switch]$SkipBackup,
  # Toolchain folder made by Setup_BabyGrip_PC.ps1.
  [string]$Setup = $(if ($env:BABYGRIP_SETUP) { $env:BABYGRIP_SETUP } else { Join-Path $HOME 'Downloads\BabyGrip_Setup' })
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$esptool = Join-Path $Setup 'toolchain\packages\esp32\tools\esptool_py\4.9.dev3\esptool.exe'
$bootApp0 = Join-Path $Setup 'toolchain\packages\esp32\hardware\esp32\3.1.1\tools\partitions\boot_app0.bin'
foreach ($needed in $esptool, $bootApp0) {
  if (-not (Test-Path $needed)) { throw "$needed not found. Run Setup_BabyGrip_PC.ps1 first (or pass -Setup <folder>)." }
}
$fw = Join-Path $root 'firmware'
if (-not (Test-Path (Join-Path $fw 'BabyGrip_WiFi.ino.bin'))) { throw 'No firmware yet: run Build_BabyGrip_Album.ps1 first.' }
$common = @('--chip', 'esp32s3', '--port', $Port, '--baud', '921600')
function Invoke-Esptool([string[]]$arguments) {
  & $esptool @common @arguments
  if ($LASTEXITCODE -ne 0) { throw "esptool failed (exit $LASTEXITCODE): $($arguments -join ' ')" }
}
if (-not $SkipBackup) {
  $backupDir = Join-Path $root 'backup'
  New-Item -ItemType Directory -Force $backupDir | Out-Null
  $backup = Join-Path $backupDir ("before-album-flash-{0}.bin" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
  "== Backing up the full 16 MB flash to $backup"
  Invoke-Esptool @('--before', 'default_reset', '--after', 'no_reset', 'read_flash', '0', '0x1000000', $backup)
  if ((Get-Item $backup).Length -ne 16777216) { throw 'Backup is incomplete.' }
}
if ($FirstInstall) {
  "== Clearing the old second app slot (becomes the media2 photo volume)"
  Invoke-Esptool @('--before', 'default_reset', '--after', 'no_reset', 'erase_region', '0x350000', '0x2C0000')
}
"== Writing bootloader, partition table and firmware (ffat photo/sound area is not touched)"
Invoke-Esptool @('--before', 'default_reset', '--after', 'hard_reset', 'write_flash', '-z',
  '--flash_mode', 'keep', '--flash_freq', 'keep', '--flash_size', 'keep',
  '0x0', (Join-Path $fw 'BabyGrip_WiFi.ino.bootloader.bin'),
  '0x8000', (Join-Path $fw 'BabyGrip_WiFi.ino.partitions.bin'),
  '0xe000', $bootApp0,
  '0x10000', (Join-Path $fw 'BabyGrip_WiFi.ino.bin'))
"== Done"
