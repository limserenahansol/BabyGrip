# Libraries used by the build

`Build_BabyGrip_Album.ps1` compiles against these two folders (`arduino-cli --libraries libraries`).
They are copies of the libraries in Waveshare's ESP32-S3-Touch-LCD-1.46 Arduino demo, the versions the
board was tested with, so a new PC builds the same firmware without looking for the right versions.

| Folder | Version | Licence | Notes |
|---|---|---|---|
| `lvgl/` | 8.3.10 | MIT (`lvgl/LICENCE.txt`) | Waveshare's copy: `src/lv_conf.h` is their configuration (16-bit colour, Montserrat 12/14/16, LVGL demos copied into `src/demos`, which the Waveshare example files in the sketch, `LVGL_Example.cpp` and `LVGL_Music.cpp`, refer to). |
| `ESP32-audioI2S-master/` | 2.0.0 | GPL-3.0 (`ESP32-audioI2S-master/LICENSE`) | Unchanged. |

All compiled files (`.c`, `.h`, `.cpp`) are byte-for-byte the same as Waveshare's. Left out of `lvgl/`,
because Arduino never compiles them or they may not be shared:

- `demos/`, `docs/`, `env_support/`, `tests/` and `scripts/` at the top level (except
  `scripts/built_in_font/Montserrat-Medium.ttf`, SIL Open Font License 1.1, read by `tools/make_fonts.py`)
- `src/font/korean.ttf` (HYGothic-Medium, a commercial HanYang Systems font) and
  `src/extra/libs/freetype/arial.ttf` (Arial, a commercial Monotype font). Nothing in the build uses them.
