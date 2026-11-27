# 음원 출처

## 2026-11-25 추가한 수면 소리 (`audio/device_sounds/`, `tools/make_sounds.py`로 생성)

모두 모노·22,050 Hz·48 kbps MP3이고, 내장 백색소음과 같은 크기(-47 LUFS)로 맞췄습니다. 녹음 3개는 가장 고른 구간을 잘라 끝과 처음을 겹쳐(cross-fade) 끊김 없이 반복되게 했습니다.

- 2026-11-26 추가: 기기 스피커로 거의 들리지 않아서, 이 8개와 내장 백색소음을 모두 -20 LUFS로 다시 만들었습니다(약 27 dB 키움, 파도·시냇물은 큰 순간만 줄이는 리미터 사용). 위의 -47 LUFS는 처음 만든 때의 값입니다. 녹음 원본은 바꾸지 않았습니다.

| 파일 | 원본 | 작가 | 라이선스 |
|---|---|---|---|
| `01-rain.mp3` (36초) | https://commons.wikimedia.org/wiki/File:Rain_(1).ogg (4–42초 구간) | ezwa | 퍼블릭 도메인 |
| `02-ocean.mp3` (56초) | https://commons.wikimedia.org/wiki/File:Oceanwavescrushing.ogg (4–63초 구간) | Luftrum | CC BY 3.0 (https://creativecommons.org/licenses/by/3.0/) |
| `03-stream.mp3` (14초) | https://commons.wikimedia.org/wiki/File:Brook_sound.ogg (0.3–15.5초 구간) | TwoWings | CC BY 3.0 (https://creativecommons.org/licenses/by/3.0/) |
| `04-pink_noise.mp3`, `05-brown_noise.mp3`, `06-fan.mp3`, `07-heartbeat.mp3`, `08-shush.mp3` (각 30초) | 직접 합성 (원본 녹음 없음) | — | 제한 없음 |
| `fx/look.mp3` (0.9초, ‘여기 봐!’ 효과음) | 직접 합성 | — | 제한 없음 |

- 원본 파일은 `audio/downloads_20261125/`에 그대로 보관했습니다(받은 그대로, 변경 없음).
- CC BY 3.0 음원은 다른 사람에게 배포할 때 위 작가 이름과 라이선스를 함께 적어야 합니다. 원본을 자르고 크기를 바꿨다는 점도 함께 밝혀 주세요.
- 심장 소리는 Wikimedia의 태아 도플러 녹음(CC BY-SA 3.0)을 받으려 했지만 서버가 요청을 막아(HTTP 429) 직접 합성했습니다. 72 bpm의 ‘쿵-쿵’과 낮은 배경음입니다.

## Lullaby / Baby Song — Alex McCulloch (Pro Sensory)

- 작가가 직접 올린 원본·라이선스: https://opengameart.org/content/lullaby-0
- CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/
- 원본 WAV는 `sources/Alex_McCulloch_Baby_Song_original.wav`에 보존했습니다.
- `Lullaby_Alex_McCulloch_CC0.mp3`: 원본 전체 205.68초, 모노·22,050 Hz·64 kbps로 변환한 휴대폰 업로드용 파일(1,646,357바이트).
- `Lullaby_Device_32k.mp3`: 원본 전체 205.68초, 모노·22,050 Hz·32 kbps로 변환한 내장 기본 음원(823,372바이트). 3 MB 펌웨어 영역에 기본 음원 두 개를 넣기 위한 설정입니다.
- 두 파일은 원본에 없는 노래나 효과를 추가하지 않았고, 자르지 않고 전체를 변환했습니다. 포맷 변환은 손실 압축이며 원본과 음질이 동일하지 않습니다.

## White_Noise_90sec.mp3

사용자가 제공한 BabyGrip 프로젝트의 `SD_CARD/white_noise.mp3`를 변경 없이 복사했습니다. 90초, 모노, 22,050 Hz, 48 kbps. 공개 재배포 라이선스는 원본 패키지에서 확인되지 않아 공개 도메인이라고 주장하지 않습니다.

## 검색했으나 이번 기기에는 넣지 않은 파일

- 브람스 자장가(Pracchia-78): https://commons.wikimedia.org/wiki/File:Johannes_Brahms_ninna_nanna_op_49_n_4.ogg
- 백색소음(Bautsch): https://commons.wikimedia.org/wiki/File:White.Noise.ogg

위 Wikimedia 파일의 권리 표시는 확인했으나 현재 환경에서 다운로드가 HTTP 429로 제한되어, 실제 확보한 Alex McCulloch 음원으로 대신 준비했습니다. 수면 효과나 실제 스피커 음압을 검증한 자료는 아닙니다.

## 2026-11-26 울음 감지 시험용 녹음 (`audio/downloads_20261126/`)

| 파일 | 원본 | 라이선스 | GitHub 저장소 |
|---|---|---|---|
| `Crying_newborn_baby.ogg` | https://commons.wikimedia.org/wiki/File:Crying_newborn_baby.ogg | CC0 | 넣음(받은 그대로) |
| `Baby_long_wanting_cry.ogg` | https://commons.wikimedia.org/wiki/File:Baby_long_wanting_cry.ogg | 확인하지 않음 | 뺌(원래 PC에만 있음) |

- 기기에서 울음 감지를 시험할 때만 썼고, 기기에 넣는 소리가 아닙니다.
- `audio/downloads_20261125/`의 `Hemlock_stream.ogg`, `Rain_against_the_window.ogg`는 받았지만 쓰지 않았고 라이선스를 적어 두지 않아서 GitHub 저장소에서 뺐습니다(원래 PC에만 있음).
- 내장 백색소음 원본은 이 폴더에서 `audio/original/white_noise_original_quiet.mp3`(위 ‘White_Noise_90sec.mp3’와 같은 파일, 정확히 90.000초, 파일 정보의 인코더 표시는 FFmpeg `Lavf61.7.103`)이고, 이것을 키운 것이 `audio/device_sounds/rom/white_noise.mp3`와 `sketch/BabyGrip_WiFi/BabyGrip_SoundData.h`입니다.
