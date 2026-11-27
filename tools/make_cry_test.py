"""A synthetic infant-cry test sound for BabyGrip's cry help (not a recording).

Twelve seconds of cry-like bursts: a voiced tone whose pitch rises from about
380 to 520 Hz and falls back (with 6 Hz vibrato), eight harmonics, a little
breath noise, 0.9-1.3 s bursts and 0.3-0.5 s pauses. It is used to check the
detector over the air (played by the PC speaker, or by BabyGrip itself with
BGCRY LISTEN). It does not sound like a real baby, and it is not meant to.

Output: audio/test/cry_test.wav (44.1 kHz, for the PC) and
        audio/test/zz-crytest.mp3 (22,050 Hz mono, for BabyGrip)
"""
import os
import shutil
import subprocess
import sys
import wave
from pathlib import Path
import numpy as np


def find_ffmpeg():
    """ffmpeg.exe: $BABYGRIP_FFMPEG, else the FFmpeg build unzipped in Downloads, else ffmpeg on PATH."""
    unzipped = (Path.home() / "Downloads" / "ffmpeg-master-latest-win64-gpl-shared"
                / "ffmpeg-master-latest-win64-gpl-shared" / "bin" / "ffmpeg.exe")
    for candidate in (os.environ.get("BABYGRIP_FFMPEG"), unzipped, shutil.which("ffmpeg")):
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    sys.exit("ffmpeg not found. Unzip ffmpeg-master-latest-win64-gpl-shared.zip "
             "(https://github.com/BtbN/FFmpeg-Builds/releases) into Downloads, or set BABYGRIP_FFMPEG.")


ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "audio" / "test"
FF = find_ffmpeg()
RATE = 44100
rng = np.random.default_rng(7)


def burst(seconds):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    shape = np.sin(np.pi * t / seconds)  # rise and fall of the pitch over the burst
    f0 = 380 + 140 * shape + 10 * np.sin(2 * np.pi * 6 * t)
    phase = 2 * np.pi * np.cumsum(f0) / RATE
    voice = sum(np.sin(k * phase) / k ** 0.8 for k in range(1, 9))
    breath = rng.standard_normal(n) * 0.08
    env = np.minimum(1, t / 0.05) * np.minimum(1, (seconds - t) / 0.08)
    return (voice + breath) * env


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    parts, total = [], 0.0
    while total < 12:
        on, off = rng.uniform(0.9, 1.3), rng.uniform(0.3, 0.5)
        parts += [burst(on), np.zeros(int(off * RATE))]
        total += on + off
    x = np.concatenate(parts)
    x = x / np.max(np.abs(x)) * 0.7
    wav = OUT / "cry_test.wav"
    with wave.open(str(wav), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes((x * 32767).astype("<i2").tobytes())
    subprocess.run([str(FF), "-hide_banner", "-nostdin", "-y", "-i", str(wav), "-codec:a", "libmp3lame", "-b:a", "48k",
                    "-ac", "1", "-ar", "22050", "-write_xing", "0", "-id3v2_version", "0", str(OUT / "zz-crytest.mp3")],
                   check=True, capture_output=True)
    print(f"{wav} {total:.1f} s; {OUT / 'zz-crytest.mp3'}")


if __name__ == "__main__":
    main()
