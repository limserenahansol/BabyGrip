# BabyGrip

Waveshare **ESP32-S3-Touch-LCD-1.46B**(1.46인치 원형 터치 화면)를 휴대폰 그립으로 붙여 쓰는 아기용 기기의 Arduino 펌웨어와 PC 도구입니다.

- 사진 보기: 글자 없이 사진만 5초마다 넘깁니다(2분 동안 안 만지면 자동으로 돌아옴). 사진은 기기 안 저장소에 들어가며 마이크로SD가 필요 없습니다.
- 수면 소리 10가지와 타이머: 3초 동안 서서히 켜지고, 끝나기 1분 전부터 서서히 줄어듭니다.
- 수유·기저귀 기록, 낮잠 시간, 밤 모드, 엎어 두면 큰 시계, 사진 찍을 때 아기 시선을 끄는 LOOK 화면
- 기기 설정 화면(SET), 울음 감지(CRY HELP, 기본 꺼짐)
- 휴대폰 페이지: 기기 Wi-Fi `BabyGrip-xxxx`에 연결한 뒤 `http://192.168.4.1`

사용법은 [GUIDE_KO.md](GUIDE_KO.md), 아이폰 시험 순서는 [IPHONE_TEST_KO.md](IPHONE_TEST_KO.md)에 있습니다.

## 다른 PC에서 처음 시작하기 (Windows)

1. Git이 없으면 설치합니다(또는 https://git-scm.com/download/win).
   ```powershell
   winget install --id Git.Git -e
   ```
2. 저장소를 받습니다. 원래 PC와 같은 자리(`다운로드\BabyGrip_Album_Claude`)에 둡니다.
   ```powershell
   git clone https://github.com/limserenahansol/BabyGrip.git "$HOME\Downloads\BabyGrip_Album_Claude"
   ```
3. 빌드 도구를 설치합니다. 한 번만 하면 되고, 약 1.1 GB를 내려받아 `$HOME\Downloads\BabyGrip_Setup`에 설치합니다(디스크 약 5 GB).
   ```powershell
   powershell -ExecutionPolicy Bypass -File "$HOME\Downloads\BabyGrip_Album_Claude\Setup_BabyGrip_PC.ps1"
   ```
4. 빌드합니다. 끝에 `Firmware OK: … of 3407872 bytes`가 나오면 성공입니다.
   ```powershell
   powershell -ExecutionPolicy Bypass -File "$HOME\Downloads\BabyGrip_Album_Claude\Build_BabyGrip_Album.ps1"
   ```
5. BabyGrip을 USB-C로 연결하고 포트 이름(예: `COM4`)을 확인합니다.
   ```powershell
   [System.IO.Ports.SerialPort]::GetPortNames()
   ```
6. 기기에 올립니다. 먼저 16 MB 전체를 그 PC의 `backup\`에 백업한 뒤 펌웨어만 씁니다. 사진·소리·기록·Wi-Fi 비밀번호는 그대로 남습니다.
   ```powershell
   powershell -ExecutionPolicy Bypass -File "$HOME\Downloads\BabyGrip_Album_Claude\Flash_BabyGrip_Album.ps1" -Port COM4
   ```
   **`-FirstInstall`은 절대 붙이지 마세요.** 맨 처음 설치 때 한 번만 쓰는 옵션이라, 지금 붙이면 사진 영역(media2)이 지워집니다.
7. 10초쯤 기다린 뒤 확인합니다. `-RunTests`를 붙이면 기기 자체 검사 60개를 돌립니다(약 4분).
   ```powershell
   powershell -ExecutionPolicy Bypass -File "$HOME\Downloads\BabyGrip_Album_Claude\Check_BabyGrip.ps1" -Port COM4 -RunTests
   ```

빌드 도구를 다른 곳에 설치하려면 환경 변수 `BABYGRIP_SETUP`을 정하거나, Setup·Build·Flash 스크립트에 `-Setup <폴더>`를 붙이세요.

## 두 PC에서 번갈아 작업하기

작업을 시작할 때 받습니다.
```powershell
git -C "$HOME\Downloads\BabyGrip_Album_Claude" pull
```
끝나면 올립니다. 처음 `git push` 때 GitHub 로그인 창이 한 번 뜹니다.
```powershell
cd "$HOME\Downloads\BabyGrip_Album_Claude"; git add -A; git commit -m "바꾼 내용"; git push
```
새 PC에서 처음 커밋하기 전에 한 번만 작성자를 정합니다. GitHub가 주는 공개용 주소라 개인 메일 주소가 드러나지 않습니다.
```powershell
git -C "$HOME\Downloads\BabyGrip_Album_Claude" config user.name "limserenahansol"; git -C "$HOME\Downloads\BabyGrip_Album_Claude" config user.email "57984763+limserenahansol@users.noreply.github.com"
```

## 저장소에 넣지 않은 것

원래 PC에만 있고 `.gitignore`로 막아 두었습니다.

| 무엇 | 이유 |
|---|---|
| `backup\` 16 MB 플래시 백업 | 가족 사진과 기기 Wi-Fi 비밀번호가 들어 있음 |
| `*.log` (기기 검사·설치 기록) | 기기가 켜질 때 Wi-Fi 비밀번호를 출력함 |
| `qa\screens\` 기기 화면 캡처 | 사진이 보임 |
| `build\`, `firmware\` | 빌드하면 다시 생김 |
| `audio\test\`, 아기 울음 녹음 1개, 시냇물·창문 빗소리 원본 2개 | 시험용이거나 라이선스를 확인하지 않음 |

- Wi-Fi 비밀번호는 소스 코드에 없습니다. 기기가 처음 켜질 때 무작위로 만들어 기기 안에만 저장하고, 기기 화면의 `Wi-Fi` 버튼을 누르면 보여 줍니다.
- 새 PC에서 올릴 때 생기는 백업과 기록도 `.gitignore`에 걸려서 GitHub에 올라가지 않습니다.

## 폴더

| 경로 | 내용 |
|---|---|
| `sketch\BabyGrip_WiFi\` | 펌웨어 소스. `BabyGrip.cpp`(화면·기능), `BabyGrip_PhoneUI.h`(휴대폰 페이지), `BabyGrip_Feed.h`(기록), `BabyGrip_Mic.h`(울음 감지), `BabyGrip_Diagnostics.h`(USB 명령·자체 검사). 나머지는 Waveshare 예제의 하드웨어 드라이버 |
| `libraries\` | 빌드에 쓰는 LVGL 8.3.10과 ESP32-audioI2S 2.0.0 ([libraries/README.md](libraries/README.md)) |
| `Setup_BabyGrip_PC.ps1` | 새 PC에 arduino-cli 1.6.0과 ESP32 core 3.1.1 설치 |
| `Build_BabyGrip_Album.ps1`, `Flash_BabyGrip_Album.ps1` | 빌드, 기기에 올리기(먼저 전체 백업) |
| `Check_BabyGrip.ps1`, `Device_Tool.ps1` | 상태·자체 검사, USB로 소리 넣기·화면 캡처·명령 보내기 |
| `tools\` | 소리 만들기(`make_sounds.py`: numpy, FFmpeg), 큰 글꼴(`make_fonts.py`: Pillow), 시험용 울음(`make_cry_test.py`) |
| `audio\` | 기기 소리와 원본, 출처·라이선스는 [audio/CREDITS_KO.md](audio/CREDITS_KO.md) |
| `tests\`, `qa\fixtures\` | 휴대폰 페이지를 모의 서버로 시험하는 도구(Node.js). 사진은 모두 합성한 가짜 사진 |

`tools\`의 파이썬 도구는 소리나 글꼴을 다시 만들 때만 필요합니다. FFmpeg는 [BtbN 빌드](https://github.com/BtbN/FFmpeg-Builds/releases)의 `ffmpeg-master-latest-win64-gpl-shared.zip`을 `다운로드` 폴더에 풀어 두거나, PATH에 있는 ffmpeg를 씁니다(`BABYGRIP_FFMPEG`로 직접 지정할 수도 있음).

## 다른 사람이 만든 코드와 소리

- LVGL 8.3.10은 MIT, ESP32-audioI2S 2.0.0은 GPL-3.0입니다.
- 하드웨어 드라이버(`Display_SPD2010`, `Touch_SPD2010`, `esp_lcd_spd2010`, `Gyro_QMI8658`, `RTC_PCF85063`, `TCA9554PWR` 등)와 두 라이브러리는 Waveshare가 이 보드용으로 배포하는 Arduino 예제에서 왔습니다(https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46B).
- 소리 출처와 라이선스는 [audio/CREDITS_KO.md](audio/CREDITS_KO.md)에 있습니다. 파도·시냇물은 CC BY 3.0이라 다시 나눠 줄 때 작가 이름을 함께 적어야 합니다.
- 기본 백색소음은 처음 받은 BabyGrip 1.0 패키지에 들어 있던 파일이며, 그 패키지에는 라이선스가 적혀 있지 않았습니다.

## 안전

의료 기기가 아닙니다. 가장 낮은 음량부터 쓰고, 기기를 아기 침대 안이나 아기 손이 닿는 곳에 두지 마세요.
