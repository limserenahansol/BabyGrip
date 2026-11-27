"""Makes BabyGrip's sleep-sound loops (mono, 22,050 Hz MP3).

Recordings (Wikimedia Commons, licences in audio/CREDITS_KO.md) are cut to
their steadiest stretch and cross-faded so the end runs into the start.
Pink/brown noise, fan, heartbeat and shush are synthesised by FFT over exactly
one loop length, so they repeat without a seam. Every loop, and the built-in
white noise, is set to TARGET_LUFS so the volume steps behave the same.

2026-11-26: the original white noise was recorded at -47 LUFS (mean -50 dB) and
was inaudible on the board's speaker, so everything is now made at -20 LUFS.
The original quiet file stays in audio/original/.

Output: audio/device_sounds/*.mp3, audio/device_sounds/fx/look.mp3 and
sketch/BabyGrip_WiFi/BabyGrip_SoundData.h (the built-in white noise)
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
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
FFMPEG = find_ffmpeg()
DOWN = ROOT / "audio" / "downloads_20261125"
OUT = ROOT / "audio" / "device_sounds"
RATE = 22050
BITRATE = "48k"
TARGET_LUFS = -20.0
ORIGINAL_WHITE = ROOT / "audio" / "original" / "white_noise_original_quiet.mp3"
SOUND_HEADER = ROOT / "sketch" / "BabyGrip_WiFi" / "BabyGrip_SoundData.h"
rng = np.random.default_rng(20261125)


def ffmpeg(*args, capture=False):
    run = subprocess.run([str(FFMPEG), "-hide_banner", "-nostdin", "-y", *args],
                         capture_output=True, text=False)
    if run.returncode:
        raise RuntimeError(run.stderr.decode("utf-8", "replace")[-2000:])
    return run


def decode(path):
    run = ffmpeg("-i", str(path), "-map", "0:a:0", "-ac", "1", "-ar", str(RATE), "-f", "f32le", "-")
    return np.frombuffer(run.stdout, dtype=np.float32).astype(np.float64)


def loudness(samples):
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / "x.wav"
        write_wav(wav, samples)
        run = ffmpeg("-i", str(wav), "-af", "ebur128", "-f", "null", "-")
        text = run.stderr.decode("utf-8", "replace")
        values = re.findall(r"I:\s+(-?[\d.]+) LUFS", text)
        return float(values[-1])


def write_wav(path, samples):
    data = (np.clip(samples, -1, 1) * 32767).astype("<i2").tobytes()
    import wave
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


def builtin_white():
    """The original bundled white noise (90 s), decoded."""
    return decode(ORIGINAL_WHITE)


def write_sound_header(mp3_path):
    data = mp3_path.read_bytes()
    lines = [",".join(f"0x{b:02x}" for b in data[i:i + 24]) for i in range(0, len(data), 24)]
    SOUND_HEADER.write_text(
        "#pragma once\n#include <Arduino.h>\n"
        "// Built-in white noise: the original bundled 90-second recording, raised to -20 LUFS\n"
        "// by tools/make_sounds.py (the quiet original is audio/original/white_noise_original_quiet.mp3).\n"
        "static const uint8_t BG_SAMPLE_NOISE[] PROGMEM = {\n" + ",\n".join(lines) + "\n};\n",
        encoding="ascii")


def crossfade_loop(samples, start_s, length_s, fade_s=1.5):
    a, n, cf = int(start_s * RATE), int(length_s * RATE), int(fade_s * RATE)
    seg = samples[a:a + n + cf].copy()
    assert len(seg) == n + cf, "source too short"
    w = np.sin(np.linspace(0, np.pi / 2, cf)) ** 2  # equal-power-ish ramp
    loop = seg[:n].copy()
    loop[:cf] = seg[n:n + cf] * np.sqrt(1 - w) + seg[:cf] * np.sqrt(w)
    return loop


def shaped_noise(seconds, gain_fn):
    """Periodic noise: random phases, magnitude gain_fn(f), length = one loop."""
    n = int(seconds * RATE)
    f = np.fft.rfftfreq(n, 1 / RATE)
    spectrum = gain_fn(np.maximum(f, 1e-3)) * np.exp(2j * np.pi * rng.random(len(f)))
    spectrum[0] = 0
    x = np.fft.irfft(spectrum, n)
    return x / np.max(np.abs(x))


def band(f, low, high, order=2):
    hp = 1 / np.sqrt(1 + (low / f) ** (2 * order))
    lp = 1 / np.sqrt(1 + (f / high) ** (2 * order))
    return hp * lp


def pink(seconds):
    return shaped_noise(seconds, lambda f: band(f, 25, 9000) / np.sqrt(f))


def brown(seconds):
    return shaped_noise(seconds, lambda f: band(f, 30, 6000) / f)


def fan(seconds):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    air = shaped_noise(seconds, lambda f: band(f, 40, 1600, 3) / f ** 0.7)
    hum = 0.05 * np.sin(2 * np.pi * 120 * t) + 0.025 * np.sin(2 * np.pi * 240 * t)  # whole cycles per loop
    return air + hum


def heartbeat(seconds, bpm=72):
    """Womb-like: low whoosh plus a soft lub-dub at 72 beats a minute."""
    n = int(seconds * RATE)
    beats = int(round(seconds * bpm / 60))
    period = n / beats
    whoosh = 0.45 * shaped_noise(seconds, lambda f: band(f, 30, 350, 3) / f ** 0.5)
    t = np.arange(int(0.25 * RATE)) / RATE

    def thump(freq, decay, amp):
        return amp * np.sin(2 * np.pi * freq * t) * np.exp(-t / decay) * (1 - np.exp(-t / 0.006))

    lub, dub = thump(48, 0.07, 1.0), thump(60, 0.05, 0.6)
    out = whoosh.copy()
    for k in range(beats):
        for shape, offset in ((lub, 0), (dub, int(0.28 * RATE))):
            at = int(k * period) + offset
            idx = (at + np.arange(len(shape))) % n  # wraps: seamless loop
            out[idx] += shape
    return out


def shush(seconds, cycle_s=2.5):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    hiss = shaped_noise(seconds, lambda f: band(f, 1500, 6000, 3))
    cycles = round(seconds / cycle_s)
    envelope = 0.55 + 0.45 * np.sin(np.pi * t * cycles / seconds) ** 2  # whole cycles per loop
    return hiss * envelope


def chime():
    t = np.arange(int(0.9 * RATE)) / RATE
    out = np.zeros_like(t)
    for start, freq in ((0.0, 1318.5), (0.18, 1046.5)):
        tt = np.clip(t - start, 0, None)
        env = (t >= start) * (1 - np.exp(-tt / 0.004)) * np.exp(-tt / 0.22)
        out += env * (np.sin(2 * np.pi * freq * tt) + 0.3 * np.sin(2 * np.pi * 2 * freq * tt))
    return out / np.max(np.abs(out))


def limit(x, ceiling=0.89, hold_s=0.005, release_s=0.2):
    """Look-ahead peak limiter, applied around the loop so the seam stays clean."""
    n, pre = len(x), min(len(x), RATE)
    y = np.concatenate([x[-pre:], x, x[:pre]])
    need = np.minimum(1.0, ceiling / np.maximum(np.abs(y), 1e-9))
    w = 2 * int(hold_s * RATE) + 1
    view = np.lib.stride_tricks.sliding_window_view
    g = view(np.pad(need, w // 2, mode="edge"), w).min(axis=1)
    g = view(np.pad(g, w // 2, mode="edge"), w).mean(axis=1)  # smooth attack, still <= need at the peak
    a = np.exp(-1 / (release_s * RATE))
    out, cur = np.empty_like(g), 1.0
    for i, v in enumerate(g):
        cur = v if v < cur else a * cur + (1 - a) * v
        out[i] = cur
    return x * out[pre:pre + n]


def save(name, samples, target_lufs):
    samples = samples - np.mean(samples)
    gain = 10 ** ((target_lufs - loudness(samples / np.max(np.abs(samples)) * 0.5)) / 20) * 0.5 / np.max(np.abs(samples))
    raw = samples
    samples = raw * gain
    if np.max(np.abs(samples)) > 0.89:  # loud peaks (waves, splashes): limit them instead of turning it all down
        for _ in range(3):
            samples = limit(raw * gain)
            gain *= 10 ** ((target_lufs - loudness(samples)) / 20)
        samples = limit(raw * gain)
    peak = np.max(np.abs(samples))
    if peak > 0.89:  # keep 1 dB headroom
        samples *= 0.89 / peak
    path = OUT / name
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / "x.wav"
        write_wav(wav, samples)
        ffmpeg("-i", str(wav), "-codec:a", "libmp3lame", "-b:a", BITRATE, "-ac", "1", "-ar", str(RATE),
               "-write_xing", "0", "-id3v2_version", "0", str(path))
    print(f"{name:28s} {len(samples) / RATE:5.1f} s  {path.stat().st_size:7d} bytes  "
          f"{loudness(samples):6.1f} LUFS  peak {20 * np.log10(np.max(np.abs(samples))):5.1f} dBFS")


def main():
    reference = TARGET_LUFS
    save("rom/white_noise.mp3", builtin_white(), reference)
    write_sound_header(OUT / "rom" / "white_noise.mp3")
    rain = decode(DOWN / "Rain_(1).ogg")
    ocean = decode(DOWN / "Oceanwavescrushing.ogg")
    brook = decode(DOWN / "Brook_sound.ogg")
    save("01-rain.mp3", crossfade_loop(rain, 4.0, 36.0), reference)
    save("02-ocean.mp3", crossfade_loop(ocean, 4.0, 56.0, 3.0), reference)
    save("03-stream.mp3", crossfade_loop(brook, 0.3, 14.0, 1.2), reference)
    save("04-pink_noise.mp3", pink(30), reference)
    save("05-brown_noise.mp3", brown(30), reference)
    save("06-fan.mp3", fan(30), reference)
    save("07-heartbeat.mp3", heartbeat(30), reference)
    save("08-shush.mp3", shush(30), reference)
    save("fx/look.mp3", chime(), reference - 4)


if __name__ == "__main__":
    sys.exit(main())
