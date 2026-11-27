#pragma once
// Board microphone (MSM261S4030H0, I2S: BCK 15, WS 2, DATA 39; its data comes in
// the right slot). One task on core 0 reads it while something needs it:
//  - BGMIC <ms>: a level measurement over USB (speaker and dropout checks), and
//  - cry help: a cry detector. Every 50 ms it takes the level of 400-3000 Hz,
//    compares it with a slowly following background level, and for loud frames
//    estimates the pitch (autocorrelation at 16 kHz). Over the last 5 s a cry is:
//    at least 55% of frames louder than the background by the chosen margin,
//    at least 25% loud with a baby's pitch (300-700 Hz; an adult voice is lower),
//    and at least two pauses (9 dB dips) after loud parts, so a sound that just
//    starts and stays (fan, TV hum, white noise) does not count.
//    It then asks the main loop (cryPending) to start the soothing sound and log it.
// While BabyGrip plays a sound the detector pauses, because it would hear itself,
// and it waits 20 s after a sound stops. Nothing is recorded or sent anywhere.
// Included inside BabyGrip.cpp's anonymous namespace.

constexpr int CRY_FRAME = 1600;    // 50 ms at 32 kHz
constexpr int CRY_WINDOW = 100;    // 5 s
constexpr int CRY_LOUD = 55;       // frames in the window louder than the background
constexpr int CRY_VOICED = 25;     // ... of them with a baby's pitch
constexpr int CRY_PAUSES = 2;      // breaths between bursts
constexpr int PITCH_N = CRY_FRAME / 2;        // the frame at 16 kHz
constexpr int PITCH_MIN_LAG = 23, PITCH_MAX_LAG = 53;  // 696-302 Hz at 16 kHz
const float CRY_MARGIN_DB[4] = {18, 24, 18, 12};  // [0] is used by BGCRY LISTEN when cry help is off
const char* const CRY_NAMES[4] = {"OFF", "LOW", "MEDIUM", "HIGH"};
float pitchFrame[PITCH_N];
float lagCorrelation(int lag) {
  float xy = 0, xx = 0, yy = 0;
  for (int i = 0; i + lag < PITCH_N; ++i) {
    const float a = pitchFrame[i], b = pitchFrame[i + lag];
    xy += a * b; xx += a * a; yy += b * b;
  }
  return xx > 0 && yy > 0 ? xy / sqrtf(xx * yy) : 0;
}
// True when the frame is voiced with its pitch between about 300 and 700 Hz, and
// the period twice as long does not fit better (then it is a lower, adult voice).
bool babyPitch() {
  float best = 0; int bestLag = 0;
  for (int lag = PITCH_MIN_LAG; lag <= PITCH_MAX_LAG; ++lag) {
    const float r = lagCorrelation(lag);
    if (r > best) { best = r; bestLag = lag; }
  }
  if (best < 0.45f) return false;
  const float lower = std::max(lagCorrelation(2 * bestLag - 1), std::max(lagCorrelation(2 * bestLag), lagCorrelation(2 * bestLag + 1)));
  return lower <= best + 0.1f;
}
volatile float cryVoicedShare = 0, cryListenMaxVoiced = 0;
volatile int cryPauses = 0, cryListenMaxPauses = 0;

std::atomic<bool> micTaskRunning{false};
std::atomic<uint32_t> micMeasureMs{0};   // a BGMIC request waiting
std::atomic<int> crySensitivity{0};      // 0 off, 1 low, 2 medium, 3 high
std::atomic<bool> cryPending{false};
std::atomic<uint32_t> cryResumeAtMs{0};  // the detector waits until then
std::atomic<uint32_t> cryListenUntilMs{0}, cryListenHits{0};
volatile float cryLevelDb = -200, cryFloorDb = -200, cryShare = 0, cryListenMaxShare = 0;

struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  float run(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};
Biquad makeBiquad(bool highPass, float fc, float fs) {
  const float w = 2 * (float)PI * fc / fs, c = cosf(w), alpha = sinf(w) / (2 * 0.7071f), a0 = 1 + alpha;
  Biquad q;
  q.b0 = (highPass ? (1 + c) : (1 - c)) / 2 / a0;
  q.b1 = (highPass ? -(1 + c) : (1 - c)) / a0;
  q.b2 = q.b0;
  q.a1 = -2 * c / a0;
  q.a2 = (1 - alpha) / a0;
  return q;
}

struct MicMeasure {
  bool active = false;
  uint32_t ms = 0, startMs = 0, frames = 0, zeros = 0, wFrames = 0, windowCount = 0;
  int photoBefore = 0;
  double sum[2] = {0, 0}, sq[2] = {0, 0}, peak[2] = {0, 0}, wSum = 0, wSq = 0;
};
float micWindows[500];  // 20 ms windows of a measurement, up to 10 s

void micMeasureReport(MicMeasure& m) {
  if (!m.frames) { Serial.println("[MIC] ERR no samples"); return; }
  double rightRms = 0;
  for (int c = 0; c < 2; ++c) {
    const double mean = m.sum[c] / m.frames, var = std::max(m.sq[c] / m.frames - mean * mean, 1e-20);
    if (c) rightRms = 10 * log10(var);
    Serial.printf("[MIC] %s rms=%.1f dBFS peak=%.1f dBFS\n", c ? "right" : "left",
                  10 * log10(var), 20 * log10(std::max(m.peak[c], 1e-10)));
  }
  float lo = 0, hi = -200; uint32_t gaps = 0;
  for (uint32_t w = 0; w < m.windowCount; ++w) {
    lo = w ? std::min(lo, micWindows[w]) : micWindows[w]; hi = std::max(hi, micWindows[w]);
    gaps += micWindows[w] < rightRms - 10;
  }
  Serial.printf("[MIC] right 20ms windows=%u min=%.1f max=%.1f gaps=%u\n", (unsigned)m.windowCount, lo, hi, (unsigned)gaps);
  Serial.printf("[MIC] END frames=%u silent_frames=%u playing=%d volume=%d photos %d->%d photoLoadMaxMs=%u\n",
                (unsigned)m.frames, (unsigned)m.zeros, noisePlaying ? 1 : 0, noiseVolumeLevel,
                m.photoBefore, photoIndex, (unsigned)photoLoadMaxMs);
}

bool micWanted() { return crySensitivity.load() > 0 || micMeasureMs.load() > 0 || cryListenUntilMs.load() > 0; }

void micServiceTask(void*) {
  for (;;) {
    I2SClass mic;
    mic.setPins(15, 2, -1, 39);
    if (!mic.begin(I2S_MODE_STD, 32000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO)) {
      Serial.println("[MIC] ERR could not start the microphone");
      micMeasureMs.store(0); cryListenUntilMs.store(0);
      micTaskRunning.store(false);
      vTaskDelete(nullptr);
      return;
    }
    static int32_t buf[256];
    MicMeasure m;
    Biquad hp = makeBiquad(true, 400, 32000), lp = makeBiquad(false, 3000, 32000);
    double frameAcc = 0; int frameN = 0;
    float levels[CRY_WINDOW]; bool loudRing[CRY_WINDOW], voicedRing[CRY_WINDOW];
    int ringPos = 0, ringFill = 0, warm = 0;
    bool floorReady = false; float floorDb = 0;
    const uint32_t startedMs = millis();
    while (micWanted() || m.active) {
      const size_t got = mic.readBytes((char*)buf, sizeof(buf));
      const uint32_t now = millis();
      if (now - startedMs < 300) continue;  // the microphone settles first
      if (!m.active && micMeasureMs.load()) {
        m = MicMeasure();
        m.active = true; m.ms = micMeasureMs.exchange(0); m.startMs = now; m.photoBefore = photoIndex;
      }
      for (size_t i = 0; i + 1 < got / 4; i += 2) {
        if (m.active) {
          for (int c = 0; c < 2; ++c) {
            const double v = buf[i + c] / 2147483648.0;
            m.sum[c] += v; m.sq[c] += v * v;
            if (fabs(v) > m.peak[c]) m.peak[c] = fabs(v);
          }
          const double r = buf[i + 1] / 2147483648.0;
          m.wSum += r; m.wSq += r * r;
          if (++m.wFrames == 640) {
            const double mean = m.wSum / 640;
            if (m.windowCount < 500) micWindows[m.windowCount++] = (float)(10 * log10(std::max(m.wSq / 640 - mean * mean, 1e-20)));
            m.wSum = m.wSq = 0; m.wFrames = 0;
          }
          m.zeros += buf[i] == 0 && buf[i + 1] == 0;
          ++m.frames;
        }
        // Cry detector, on the data (right) slot.
        const float y = lp.run(hp.run(buf[i + 1] / 2147483648.0f));
        frameAcc += (double)y * y;
        if (!(frameN & 1)) pitchFrame[frameN / 2] = y;
        if (++frameN < CRY_FRAME) continue;
        const float level = 10 * log10f((float)(frameAcc / CRY_FRAME) + 1e-20f);
        frameAcc = 0; frameN = 0;
        cryLevelDb = level;
        const uint32_t listenUntil = cryListenUntilMs.load();
        const bool listening = listenUntil && (int32_t)(listenUntil - now) > 0;
        if (listenUntil && !listening) {
          Serial.printf("[CRY] LISTEN END hits=%u maxLoud=%.2f maxVoiced=%.2f maxPauses=%d floor=%.1f\n",
                        (unsigned)cryListenHits.load(), (float)cryListenMaxShare, (float)cryListenMaxVoiced,
                        (int)cryListenMaxPauses, floorDb);
          cryListenUntilMs.store(0);
        }
        const int s = crySensitivity.load();
        const bool armed = listening || (s > 0 && !noisePlaying && (int32_t)(now - cryResumeAtMs.load()) >= 0);
        if (!armed) { ringFill = ringPos = 0; cryShare = 0; continue; }
        if (!floorReady) {
          floorDb = warm ? std::min(floorDb, level) : level;
          floorReady = ++warm >= 20;  // 1 s to learn the room
          cryFloorDb = floorDb;
          continue;
        }
        floorDb += (level - floorDb) * (level < floorDb ? 0.3f : 0.0015f);
        cryFloorDb = floorDb;
        const bool isLoud = level >= floorDb + CRY_MARGIN_DB[listening && s == 0 ? 0 : s];
        levels[ringPos] = level;
        loudRing[ringPos] = isLoud;
        voicedRing[ringPos] = isLoud && babyPitch();
        ringPos = (ringPos + 1) % CRY_WINDOW;
        if (ringFill < CRY_WINDOW) ++ringFill;
        int loud = 0, voiced = 0, pauses = 0; float top = -200;
        for (int k = 0; k < ringFill; ++k) { loud += loudRing[k]; voiced += voicedRing[k]; top = std::max(top, levels[k]); }
        // Pauses: dips of 9 dB below the loudest frame that come after something loud.
        bool seenLoud = false, inDip = false;
        for (int k = 0; k < ringFill; ++k) {
          const int at = (ringPos - ringFill + k + CRY_WINDOW) % CRY_WINDOW;
          const bool dip = levels[at] <= top - 9;
          if (dip && !inDip && seenLoud) ++pauses;
          inDip = dip;
          seenLoud |= loudRing[at] && !dip;
        }
        cryShare = (float)loud / CRY_WINDOW;
        cryVoicedShare = (float)voiced / CRY_WINDOW;
        cryPauses = pauses;
        if (listening) {
          if (cryShare > cryListenMaxShare) cryListenMaxShare = cryShare;
          if (cryVoicedShare > cryListenMaxVoiced) cryListenMaxVoiced = cryVoicedShare;
          if (pauses > cryListenMaxPauses) cryListenMaxPauses = pauses;
        }
        if (ringFill == CRY_WINDOW && loud >= CRY_LOUD && voiced >= CRY_VOICED && pauses >= CRY_PAUSES) {
          ringFill = ringPos = 0;
          if (listening) cryListenHits.fetch_add(1);
          else { cryPending.store(true); cryResumeAtMs.store(now + 30000); }
        }
      }
      if (m.active && now - m.startMs >= m.ms) { micMeasureReport(m); m.active = false; }
    }
    mic.end();
    micTaskRunning.store(false);
    // Something asked for the microphone while it was stopping: start again.
    if (micWanted() && !micTaskRunning.exchange(true)) continue;
    vTaskDelete(nullptr);
    return;
  }
}
void ensureMicTask() {
  if (micTaskRunning.exchange(true)) return;
  if (xTaskCreatePinnedToCore(micServiceTask, "BabyGrip mic", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    micTaskRunning.store(false);
    Serial.println("[MIC] ERR could not start the task");
  }
}
