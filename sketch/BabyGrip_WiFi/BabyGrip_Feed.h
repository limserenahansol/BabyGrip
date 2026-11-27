#pragma once
// Feeding and diaper log. Included inside BabyGrip.cpp's anonymous namespace.
//
// Records are kept in PSRAM in the order they were made and saved to
// /babylog.bin on the ffat volume after every change (write-new-then-rename,
// so a power cut keeps either the old or the new file). Times are UTC epoch
// seconds. Without a clock (RTC lost power and no phone yet) a record keeps
// seconds-since-boot and is converted when the phone sets the time; records
// from an earlier boot that never got a clock are marked "time unknown".
constexpr int MAX_LOG = 600;                       // about 40 days at 15 entries a day
constexpr uint32_t FEED_AUTO_STOP_SECS = 60 * 60;  // one side left running for an hour
constexpr uint32_t QUICK_UNDO_MS = 30000;
// LOG_CRY: cry help heard crying and started a sound (BabyGrip_Mic.h).
enum LogKind : uint8_t { LOG_BREAST = 1, LOG_BOTTLE = 2, LOG_WET = 3, LOG_DIRTY = 4, LOG_BOTH = 5, LOG_DIAPER = 6, LOG_CRY = 7 };
enum LogFlag : uint8_t { LF_RUNNING = 1, LF_RIGHT = 2, LF_AUTO = 4, LF_PENDING = 8, LF_UNKNOWN = 16 };
struct LogRecord {
  uint32_t id;
  uint32_t start;        // UTC epoch; with LF_PENDING seconds since this boot
  uint16_t left, right;  // breast seconds per side (finished segments)
  uint16_t ml;           // bottle amount, 0 = not entered
  uint8_t kind, flags;
};
static_assert(sizeof(LogRecord) == 16, "log record layout");
struct LogHeader { uint32_t magic, count, nextId, reserved; };
constexpr uint32_t LOG_MAGIC = 0x314C4742;  // "BGL1"

LogRecord* logs = nullptr;
int logCount = 0, runningLog = -1;
uint32_t nextLogId = 1, logRev = 1;
uint32_t segmentStartMs = 0;  // when the current side started
uint32_t lastQuickLogId = 0, lastQuickLogMs = 0;
bool logStorageOk = false;

uint32_t uptimeSecs() { return millis() / 1000; }
bool logIsFeed(const LogRecord& r) { return r.kind == LOG_BREAST || r.kind == LOG_BOTTLE; }
bool logIsDiaper(const LogRecord& r) { return r.kind >= LOG_WET && r.kind <= LOG_DIAPER; }
bool logKnownTime(const LogRecord& r) { return !(r.flags & LF_UNKNOWN); }
uint32_t segmentSecs() { return runningLog >= 0 ? (millis() - segmentStartMs) / 1000 : 0; }
bool feedRunningRight() { return runningLog >= 0 && (logs[runningLog].flags & LF_RIGHT); }
uint32_t feedSideSecs(int index, bool right) {
  const LogRecord& r = logs[index];
  uint32_t secs = right ? r.right : r.left;
  if (index == runningLog && (bool)(r.flags & LF_RIGHT) == right) secs += segmentSecs();
  return secs;
}
uint32_t feedTotalSecs(int index) { return feedSideSecs(index, false) + feedSideSecs(index, true); }
// Seconds since a record started, or -1 when its time is unknown.
int64_t logAgo(const LogRecord& r) {
  if (r.flags & LF_UNKNOWN) return -1;
  if (r.flags & LF_PENDING) return (int64_t)uptimeSecs() - r.start;
  if (!clockAvailable) return -1;
  return (int64_t)time(nullptr) - r.start;
}
int lastLogIndex(bool (*match)(const LogRecord&)) {
  for (int i = logCount - 1; i >= 0; --i)
    if (match(logs[i]) && logKnownTime(logs[i])) return i;
  return -1;
}
int lastFeedIndex() { return lastLogIndex(logIsFeed); }
int lastBreastIndex() { return lastLogIndex([](const LogRecord& r) { return r.kind == LOG_BREAST; }); }
int lastDiaperIndex() { return lastLogIndex(logIsDiaper); }
int findLog(uint32_t id) {
  for (int i = 0; i < logCount; ++i) if (logs[i].id == id) return i;
  return -1;
}
// Start of today (local midnight) as epoch, or 0 without a clock.
uint32_t todayStartEpoch() {
  if (!clockAvailable) return 0;
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  local.tm_hour = local.tm_min = local.tm_sec = 0;
  local.tm_isdst = -1;
  return (uint32_t)mktime(&local);
}
struct DaySummary { int feeds, breast, bottles, mlTotal, diapers, wet, dirty, cries; };
// With a clock: since local midnight. Without: since this power-on.
DaySummary todaySummary() {
  DaySummary s = {};
  const uint32_t midnight = todayStartEpoch();
  for (int i = 0; i < logCount; ++i) {
    const LogRecord& r = logs[i];
    if (r.flags & LF_UNKNOWN) continue;
    const bool today = midnight ? (!(r.flags & LF_PENDING) && r.start >= midnight) : (r.flags & LF_PENDING);
    if (!today) continue;
    if (logIsFeed(r)) { ++s.feeds; if (r.kind == LOG_BOTTLE) { ++s.bottles; s.mlTotal += r.ml; } else ++s.breast; }
    if (r.kind == LOG_CRY) ++s.cries;
    if (logIsDiaper(r)) {
      ++s.diapers;
      if (r.kind == LOG_WET || r.kind == LOG_BOTH) ++s.wet;
      if (r.kind == LOG_DIRTY || r.kind == LOG_BOTH) ++s.dirty;
    }
  }
  return s;
}

void saveRunningSegment() {
  if (runningLog < 0) { prefs.putUInt("segstart", 0); return; }
  // Without a clock a running feed cannot be resumed after a restart.
  prefs.putUInt("segstart", clockAvailable ? (uint32_t)time(nullptr) - segmentSecs() : 0);
}
bool saveLog() {
  ++logRev;
  if (!logStorageOk) return false;
  File f = FFat.open("/babylog.tmp", FILE_WRITE);
  if (!f) return false;
  const LogHeader h = {LOG_MAGIC, (uint32_t)logCount, nextLogId, 0};
  bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h) &&
            f.write((const uint8_t*)logs, sizeof(LogRecord) * logCount) == sizeof(LogRecord) * logCount;
  f.close();
  if (!ok) { FFat.remove("/babylog.tmp"); return false; }
  FFat.remove("/babylog.bak");
  if (FFat.exists("/babylog.bin") && !FFat.rename("/babylog.bin", "/babylog.bak")) return false;
  if (!FFat.rename("/babylog.tmp", "/babylog.bin")) { FFat.rename("/babylog.bak", "/babylog.bin"); return false; }
  FFat.remove("/babylog.bak");
  return true;
}
bool readLogFile(const char* path) {
  File f = FFat.open(path, FILE_READ);
  if (!f) return false;
  LogHeader h = {};
  bool ok = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == LOG_MAGIC && h.count <= MAX_LOG &&
            f.size() == sizeof(h) + h.count * sizeof(LogRecord) &&
            f.read((uint8_t*)logs, h.count * sizeof(LogRecord)) == h.count * sizeof(LogRecord);
  f.close();
  if (!ok) return false;
  logCount = (int)h.count;
  nextLogId = h.nextId ? h.nextId : 1;
  for (int i = 0; i < logCount; ++i) if (logs[i].id >= nextLogId) nextLogId = logs[i].id + 1;
  return true;
}
// Call after storage and the clock (RTC) are set up.
void loadLog() {
  logs = (LogRecord*)heap_caps_calloc(MAX_LOG, sizeof(LogRecord), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!logs) { Serial.println("[BabyGrip] Log memory allocation failed"); return; }
  logStorageOk = soundStorageReady;
  if (!logStorageOk) return;
  FFat.remove("/babylog.tmp");
  if (!readLogFile("/babylog.bin") && !readLogFile("/babylog.bak")) logCount = 0;
  bool changed = false;
  for (int i = 0; i < logCount; ++i) {
    LogRecord& r = logs[i];
    if (r.flags & LF_PENDING) { r.flags = (r.flags & ~LF_PENDING) | LF_UNKNOWN; r.start = 0; changed = true; }
    if (r.flags & LF_RUNNING) {
      const uint32_t segStart = prefs.getUInt("segstart", 0);
      const uint32_t now = clockAvailable ? (uint32_t)time(nullptr) : 0;
      if (i == logCount - 1 && segStart && now >= segStart && now - segStart < FEED_AUTO_STOP_SECS) {
        runningLog = i;  // resume the side that was running before the restart
        segmentStartMs = millis() - (now - segStart) * 1000UL;
      } else {
        r.flags = (r.flags & ~LF_RUNNING) | LF_AUTO;
        changed = true;
      }
    }
  }
  if (changed) { saveLog(); saveRunningSegment(); }
  Serial.printf("[BabyGrip] Feeding log: %d entries%s\n", logCount, runningLog >= 0 ? " (feed running)" : "");
}
// The phone or NTP set the clock: place records made before that.
void logClockArrived() {
  if (!logs) return;
  const uint32_t now = (uint32_t)time(nullptr), up = uptimeSecs();
  bool changed = false;
  for (int i = 0; i < logCount; ++i)
    if (logs[i].flags & LF_PENDING) {
      logs[i].start = now - (up - std::min(up, logs[i].start));
      logs[i].flags &= ~LF_PENDING;
      changed = true;
    }
  if (changed) saveLog();
  if (runningLog >= 0) saveRunningSegment();
}
LogRecord* appendLog(uint8_t kind) {
  if (!logs) return nullptr;
  if (logCount >= MAX_LOG) {
    // Drop the oldest finished record.
    memmove(&logs[0], &logs[1], sizeof(LogRecord) * (logCount - 1));
    --logCount;
    if (runningLog > 0) --runningLog; else if (runningLog == 0) runningLog = -1;
  }
  LogRecord& r = logs[logCount++];
  memset(&r, 0, sizeof(r));
  r.id = nextLogId++;
  r.kind = kind;
  r.start = clockAvailable ? (uint32_t)time(nullptr) : uptimeSecs();
  r.flags = clockAvailable ? 0 : LF_PENDING;
  return &r;
}
void finishSegment() {
  if (runningLog < 0) return;
  LogRecord& r = logs[runningLog];
  const uint32_t secs = std::min(segmentSecs(), FEED_AUTO_STOP_SECS);
  uint16_t& side = (r.flags & LF_RIGHT) ? r.right : r.left;
  side = (uint16_t)std::min<uint32_t>(65535, side + secs);
}
void feedStop(bool automatic = false) {
  if (runningLog < 0) return;
  finishSegment();
  logs[runningLog].flags &= ~LF_RUNNING;
  if (automatic) logs[runningLog].flags |= LF_AUTO;
  runningLog = -1;
  saveLog();
  saveRunningSegment();
}
// LEFT / RIGHT: start a feed, switch sides, or stop when the running side is tapped.
void feedSide(bool right) {
  if (runningLog >= 0) {
    if (feedRunningRight() == right) { feedStop(); return; }
    finishSegment();
    logs[runningLog].flags ^= LF_RIGHT;
    segmentStartMs = millis();
    saveLog();
    saveRunningSegment();
    return;
  }
  LogRecord* r = appendLog(LOG_BREAST);
  if (!r) return;
  r->flags |= LF_RUNNING | (right ? LF_RIGHT : 0);
  runningLog = logCount - 1;
  segmentStartMs = millis();
  saveLog();
  saveRunningSegment();
}
uint32_t logQuick(uint8_t kind, uint16_t ml) {
  LogRecord* r = appendLog(kind);
  if (!r) return 0;
  r->ml = ml;
  lastQuickLogId = r->id;
  lastQuickLogMs = millis();
  saveLog();
  return r->id;
}
bool deleteLog(uint32_t id) {
  const int at = findLog(id);
  if (at < 0) return false;
  if (at == runningLog) runningLog = -1;
  else if (runningLog > at) --runningLog;
  memmove(&logs[at], &logs[at + 1], sizeof(LogRecord) * (logCount - at - 1));
  --logCount;
  if (lastQuickLogId == id) lastQuickLogId = 0;
  saveLog();
  saveRunningSegment();
  return true;
}
// After a start time was corrected: keep the list in time order.
void sortLogs() {
  const uint32_t runningId = runningLog >= 0 ? logs[runningLog].id : 0;
  std::stable_sort(logs, logs + logCount, [](const LogRecord& a, const LogRecord& b) { return a.start < b.start; });
  runningLog = runningId ? findLog(runningId) : -1;
}
bool quickUndoAvailable() { return lastQuickLogId && millis() - lastQuickLogMs < QUICK_UNDO_MS && findLog(lastQuickLogId) >= 0; }
// Called once a second.
bool feedAutoStop() {
  if (runningLog >= 0 && segmentSecs() >= FEED_AUTO_STOP_SECS) { feedStop(true); return true; }
  return false;
}
