#pragma once
// USB-only, explicitly triggered installation checks. HTTP requests go through
// the real server on this board; this does not test phone radio connectivity.
// The checks leave the user's photos, sounds and nap totals as they were: they
// add and remove only their own test photo, and play sound at volume 0.
std::atomic_bool diagnosticRunning{false};

int diagnosticRequest(const char* method, const char* path, const String& body,
                      String& response, bool token = true, const char* type = "application/x-www-form-urlencoded") {
  NetworkClient transport;
  if (!transport.connect(IPAddress(127, 0, 0, 1), 80, 2000)) return -1002;
  HTTPClient client;
  client.setTimeout(20000);
  if (!client.begin(transport, String("192.168.4.1"), 80, String(path), false)) return -1000;
  if (token) client.addHeader("X-BabyGrip-Token", csrfToken);
  client.addHeader("Content-Type", type);
  int status = strcmp(method, "GET") == 0 ? client.GET() : client.POST(body);
  response = status > 0 ? client.getString() : String();
  client.end();
  return status;
}
int diagnosticUpload(const char* path, const char* filename, const uint8_t* bytes, size_t length, String& response) {
  const String prefix = String("--bgcheck\r\nContent-Disposition: form-data; name=\"file\"; filename=\"") + filename +
                        "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
  const String suffix = "\r\n--bgcheck--\r\n";
  const size_t total = prefix.length() + length + suffix.length();
  uint8_t* payload = (uint8_t*)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!payload) return -1001;
  memcpy(payload, prefix.c_str(), prefix.length());
  if (length) memcpy(payload + prefix.length(), bytes, length);
  memcpy(payload + prefix.length() + length, suffix.c_str(), suffix.length());
  NetworkClient transport;
  if (!transport.connect(IPAddress(127, 0, 0, 1), 80, 2000)) { free(payload); return -1002; }
  HTTPClient client; client.setTimeout(20000);
  int status = -1000;
  if (client.begin(transport, String("192.168.4.1"), 80, String(path), false)) {
    client.addHeader("X-BabyGrip-Token", csrfToken);
    client.addHeader("Content-Type", "multipart/form-data; boundary=bgcheck");
    status = client.POST(payload, total);
    if (status > 0) response = client.getString();
    client.end();
  }
  free(payload);
  return status;
}
long diagnosticNumber(const String& json, const char* key) {
  const String prefix = String("\"") + key + "\":";
  const int start = json.indexOf(prefix);
  return start < 0 ? -1 : json.substring(start + prefix.length()).toInt();
}
uint32_t diagnosticHashBytes(const uint8_t* bytes, size_t length) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619UL;
  return hash;
}
uint32_t diagnosticFileHash(fs::FS& fs, const char* path) {
  File file = fs.open(path, FILE_READ);
  if (!file) return 0;
  uint32_t hash = 2166136261UL;
  uint8_t data[1024];
  while (file.available()) {
    const size_t n = file.read(data, sizeof(data));
    if (!n) { file.close(); return 0; }
    for (size_t i = 0; i < n; ++i) hash = (hash ^ data[i]) * 16777619UL;
  }
  file.close(); return hash;
}
bool diagnosticPage() {
  NetworkClient transport;
  if (!transport.connect(IPAddress(127, 0, 0, 1), 80, 2000)) return false;
  HTTPClient client; client.setTimeout(10000);
  if (!client.begin(transport, String("192.168.4.1"), 80, String("/"), false) || client.GET() != 200) return false;
  const int expected = client.getSize();
  NetworkClient& stream = client.getStream();
  size_t read = 0; bool tokenFound = false, placeholderFound = false;
  String window; char block[257]; const uint32_t started = millis();
  while ((int)read < expected && millis() - started < 10000) {
    if (!stream.available()) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
    const size_t n = stream.readBytes((uint8_t*)block, min((size_t)256, (size_t)stream.available()));
    block[n] = '\0'; read += n;
    window += block;
    tokenFound |= window.indexOf(csrfToken) >= 0;
    placeholderFound |= window.indexOf("__BG_TOKEN__") >= 0;
    if (window.length() > 48) window = window.substring(window.length() - 48);
  }
  client.end(); return expected > 10000 && (int)read == expected && tokenFound && !placeholderFound;
}
bool diagnosticAudioAdvances(String& response) {
  vTaskDelay(pdMS_TO_TICKS(3000));
  diagnosticRequest("GET", "/api/state", "", response);
  return response.indexOf("\"decoderRunning\":true") >= 0 && diagnosticNumber(response, "audioSeconds") >= 1;
}
void diagnosticTask(void*) {
  unsigned passed = 0, failed = 0;
  auto expect = [&](bool okay, const char* name) {
    Serial.printf("[CHECK] %s %s\n", okay ? "PASS" : "FAIL", name);
    okay ? ++passed : ++failed;
  };
  auto finish = [&]() {
    Serial.printf("[CHECK] COMPLETE passed=%u failed=%u (TCP loopback; phone RF/actual sound not tested)\n", passed, failed);
    diagnosticRunning.store(false); vTaskDelete(nullptr);
  };
  String response;
  Serial.print("[CHECK] listener="); Serial.println(phone.listening());
  Serial.print("[CHECK] free heap="); Serial.println(ESP.getFreeHeap());
  Serial.print("[CHECK] largest heap="); Serial.println(ESP.getMaxAllocHeap());
  for (int v = 0; v < VOLUME_COUNT; ++v)
    Serial.printf("[CHECK] volume %s ready=%d total=%u free=%u\n", volumes[v].label, volumes[v].ready,
                  (unsigned)(volumes[v].ready ? volumes[v].fs->totalBytes() : 0), (unsigned)volumeFree(v));
  int status = diagnosticRequest("GET", "/api/state", "", response);
  Serial.print("[CHECK] HTTP code: "); Serial.println(status);
  if (response.length()) Serial.println(response);
  expect(status == 200, "HTTP status endpoint");
  expect(readyVolumes() == VOLUME_COUNT, "both internal photo volumes mounted");
  if (status != 200 || response.indexOf("\"napRunning\":true") >= 0 || response.indexOf("\"noisePlaying\":true") >= 0 ||
      runningLog >= 0) {
    Serial.println("[CHECK] SKIP mutations: requires an idle device (no nap, no sound, no feed running)");
    finish(); return;
  }
  const long startCount = diagnosticNumber(response, "photoCount");
  const long startVolume = diagnosticNumber(response, "volume");
  const long startTimer = diagnosticNumber(response, "timerMinutes");
  const String startTrack = trackCount ? String(tracks[trackIndex].key) : String("white");
  const bool startNight = nightMode;
  expect(diagnosticPage(), "complete phone HTML and valid session token");
  expect(diagnosticRequest("POST", "/api/control", "action=nap_start", response, false) == 403, "reject unauthenticated control");
  status = diagnosticRequest("POST", "/api/photo", "not multipart", response, true, "application/octet-stream");
  expect(status <= 0 || status == 400, "reject raw photo upload");
  expect(diagnosticRequest("GET", "/api/state", "", response) == 200, "server alive after raw upload");

  char testName[24];
  snprintf(testName, sizeof(testName), "p00000001%08lx.jpg", (unsigned long)esp_random());
  static const uint8_t invalid[] = {'b', 'a', 'd'};
  String sizedPath = "/api/photo?size=" + String(sizeof(BG_TEST_JPEG));
  expect(diagnosticUpload(sizedPath.c_str(), testName, invalid, sizeof(invalid), response) == 400, "reject broken JPEG");
  expect(diagnosticUpload(sizedPath.c_str(), "check.bin", BG_TEST_JPEG, sizeof(BG_TEST_JPEG), response) == 400, "reject unexpected photo name");
  diagnosticRequest("GET", "/api/state", "", response);
  expect(diagnosticNumber(response, "photoCount") == startCount, "rejected photos not published");

  // A sound keeps playing (silently) while a photo arrives.
  diagnosticRequest("POST", "/api/control", "action=volume&value=0", response);
  diagnosticRequest("POST", "/api/control", "action=track&value=white", response);
  expect(diagnosticRequest("POST", "/api/control", "action=noise_start", response) == 200, "start sound before upload");
  vTaskDelay(pdMS_TO_TICKS(1500));
  status = diagnosticUpload(sizedPath.c_str(), testName, BG_TEST_JPEG, sizeof(BG_TEST_JPEG), response);
  expect(status == 200 && response.indexOf("\"duplicate\":false") >= 0, "upload 412x412 JPEG");
  expect(diagnosticAudioAdvances(response), "sound keeps playing during photo upload");
  diagnosticRequest("POST", "/api/control", "action=noise_stop", response);
  diagnosticRequest("GET", "/api/state", "", response);
  expect(diagnosticNumber(response, "photoCount") == startCount + 1, "photo counted in album");
  status = diagnosticUpload(sizedPath.c_str(), testName, BG_TEST_JPEG, sizeof(BG_TEST_JPEG), response);
  expect(status == 200 && response.indexOf("\"duplicate\":true") >= 0, "same photo again is skipped");
  diagnosticRequest("GET", "/api/state", "", response);
  expect(diagnosticNumber(response, "photoCount") == startCount + 1, "duplicate not stored twice");
  expect(diagnosticRequest("GET", "/api/photos", "", response) == 200 && response.indexOf(testName) >= 0, "photo list includes upload");
  expect(diagnosticRequest("POST", "/api/delete", String("name=") + testName, response) == 200, "delete test photo");
  diagnosticRequest("GET", "/api/state", "", response);
  expect(diagnosticNumber(response, "photoCount") == startCount, "album back to original count");

  // Sound library: a broken MP3 is refused; a real one becomes its own sound,
  // plays, and is deleted again.
  const int tracksBefore = trackCount;
  expect(diagnosticUpload("/api/noise?name=zz-bgcheck", "check.mp3", invalid, sizeof(invalid), response) == 400 &&
         trackCount == tracksBefore && !FFat.exists("/sounds/zz-bgcheck.mp3"), "invalid MP3 refused");
  expect(diagnosticUpload("/api/noise?name=Bad%20Name", "check.mp3", BG_SAMPLE_NOISE, sizeof(BG_SAMPLE_NOISE), response) == 400,
         "unsafe sound name refused");
  expect(diagnosticUpload("/api/noise?name=zz-bgcheck", "check.mp3", BG_SAMPLE_NOISE, sizeof(BG_SAMPLE_NOISE), response) == 200 &&
         diagnosticFileHash(FFat, "/sounds/zz-bgcheck.mp3") == diagnosticHashBytes(BG_SAMPLE_NOISE, sizeof(BG_SAMPLE_NOISE)) &&
         trackCount == tracksBefore + 1, "MP3 stored byte for byte as its own sound");
  expect(diagnosticRequest("POST", "/api/control", "action=track&value=f:zz-bgcheck", response) == 200 &&
         diagnosticRequest("POST", "/api/control", "action=noise_start", response) == 200 && diagnosticAudioAdvances(response),
         "uploaded sound plays");
  diagnosticRequest("POST", "/api/control", "action=noise_stop", response);
  expect(diagnosticRequest("POST", "/api/control", "action=sound_delete&value=f:zz-bgcheck", response) == 200 &&
         trackCount == tracksBefore && !FFat.exists("/sounds/zz-bgcheck.mp3"), "delete uploaded sound");

  // Feeding log: left, switch to right, stop; a bottle and a diaper; then all removed.
  const int logsBefore = logCount;
  uint32_t testIds[3] = {};
  expect(diagnosticRequest("POST", "/api/feed", "action=left", response) == 200 && runningLog >= 0 && !feedRunningRight(),
         "start left feed");
  vTaskDelay(pdMS_TO_TICKS(2300));
  expect(diagnosticRequest("POST", "/api/feed", "action=right", response) == 200 && response.indexOf("\"side\":\"right\"") >= 0,
         "switch to right side");
  vTaskDelay(pdMS_TO_TICKS(1300));
  expect(diagnosticRequest("POST", "/api/feed", "action=stop", response) == 200 && runningLog < 0 &&
         response.indexOf("\"running\":false") >= 0, "stop feed");
  testIds[0] = logs[logCount - 1].id;
  expect(logs[logCount - 1].kind == LOG_BREAST && logs[logCount - 1].left >= 2 && logs[logCount - 1].right >= 1,
         "both sides timed");
  expect(diagnosticRequest("POST", "/api/feed", "action=bottle&value=90", response) == 200 &&
         logs[logCount - 1].kind == LOG_BOTTLE && logs[logCount - 1].ml == 90, "bottle 90 ml logged");
  testIds[1] = logs[logCount - 1].id;
  expect(diagnosticRequest("POST", "/api/feed", "action=diaper&value=wet", response) == 200 &&
         logs[logCount - 1].kind == LOG_WET, "wet diaper logged");
  testIds[2] = logs[logCount - 1].id;
  expect(diagnosticRequest("GET", "/api/feeds", "", response) == 200 && response.indexOf("\"ml\":90") >= 0 &&
         response.indexOf("\"id\":" + String(testIds[0])) >= 0, "log list for the phone");
  expect(!logStorageOk || FFat.exists("/babylog.bin"), "log saved to internal storage");
  bool removed = true;
  for (uint32_t id : testIds) removed &= id && diagnosticRequest("POST", "/api/feed", "action=delete&id=" + String(id), response) == 200;
  expect(removed && logCount == logsBefore, "test entries removed");

  // Night mode on and back.
  expect(diagnosticRequest("POST", "/api/control", startNight ? "action=night&value=0" : "action=night&value=1", response) == 200 &&
         nightMode == !startNight, "night mode switches");
  diagnosticRequest("POST", "/api/control", startNight ? "action=night&value=1" : "action=night&value=0", response);
  expect(nightMode == startNight, "night mode restored");

  const uint32_t savedTotal = finishedTodaySecs;
  expect(diagnosticRequest("POST", "/api/control", "action=nap_start", response) == 200, "start nap over HTTP");
  vTaskDelay(pdMS_TO_TICKS(2200));
  diagnosticRequest("POST", "/api/control", "action=nap_start", response);
  const long elapsed = diagnosticNumber(response, "napElapsed");
  expect(elapsed >= 2 && elapsed < 8, "nap elapsed and duplicate-start idempotence");
  expect(diagnosticRequest("POST", "/api/control", "action=nap_stop", response) == 200, "stop nap over HTTP");
  finishedTodaySecs = savedTotal;  // the test nap is not part of the baby's day
  if (clockAvailable) prefs.putUInt("total", finishedTodaySecs);

  expect(diagnosticRequest("POST", "/api/control", "action=timer&value=15", response) == 200, "select 15 minute timer");
  expect(diagnosticRequest("POST", "/api/control", "action=noise_start", response) == 200, "start built-in white noise");
  expect(diagnosticAudioAdvances(response), "white-noise decoder advances from firmware");
  diagnosticRequest("POST", "/api/control", "action=noise_stop", response);
  expect(diagnosticRequest("POST", "/api/control", "action=track&value=lullaby", response) == 200, "select built-in lullaby");
  expect(diagnosticRequest("POST", "/api/control", "action=noise_start", response) == 200, "start built-in lullaby");
  expect(diagnosticAudioAdvances(response), "lullaby decoder advances from firmware");
  expect(diagnosticRequest("POST", "/api/control", "action=noise_stop", response) == 200, "stop sound over HTTP");
  // Every stored sound decodes on this board (silently, volume 0).
  for (int i = 0; i < trackCount; ++i) {
    if (tracks[i].source != SRC_FILE) continue;
    const String key = tracks[i].key;
    const bool plays = diagnosticRequest("POST", "/api/control", "action=track&value=" + key, response) == 200 &&
                       diagnosticRequest("POST", "/api/control", "action=noise_start", response) == 200 &&
                       diagnosticAudioAdvances(response);
    diagnosticRequest("POST", "/api/control", "action=noise_stop", response);
    expect(plays, ("stored sound plays: " + key).c_str());
  }

  // Soft start and the fade-out before the timer ends (still at volume 0).
  diagnosticRequest("POST", "/api/control", "action=track&value=white", response);
  diagnosticRequest("POST", "/api/control", "action=noise_start", response);
  const float startFade = fadeDb(millis());
  expect(noisePlaying && startFade < -20, "sound starts softly");
  noiseEndsMs = millis() + 30000;  // half-way through the last minute
  const float endFade = fadeDb(millis());
  expect(endFade < -15 && endFade > -30, "sound fades out in the timer's last minute");
  diagnosticRequest("POST", "/api/control", "action=noise_stop", response);

  // Cry help: what happens when the detector hears crying (BabyGrip_Mic.h).
  const int cryBefore = crySensitivity.load();
  const String cryKeyBefore = cryTrackKey;
  expect(diagnosticRequest("POST", "/api/control", "action=cry&value=2", response) == 200 && crySensitivity.load() == 2 &&
         diagnosticRequest("POST", "/api/control", "action=cry_track&value=white", response) == 200 && cryTrackKey == "white",
         "cry help settings over HTTP");
  diagnosticRequest("POST", "/api/control", "action=track&value=" + startTrack, response);
  const int trackBefore = trackIndex, cryLogsBefore = logCount;
  cryPending.store(true);
  for (int i = 0; i < 40 && !noisePlaying; ++i) vTaskDelay(pdMS_TO_TICKS(50));
  const bool cryLogged = logCount == cryLogsBefore + 1 && logs[logCount - 1].kind == LOG_CRY;
  expect(cryLogged && noisePlaying && String(tracks[trackIndex].key) == "white", "cry help logs and plays its sound");
  diagnosticRequest("POST", "/api/control", "action=noise_stop", response);
  expect(!noisePlaying && trackIndex == trackBefore, "sound choice restored after cry help");
  expect(cryLogged && diagnosticRequest("POST", "/api/feed", "action=delete&id=" + String(logs[logCount - 1].id), response) == 200 &&
         logCount == cryLogsBefore, "cry test entry removed");
  diagnosticRequest("POST", "/api/control", "action=cry_track&value=" + cryKeyBefore, response);
  diagnosticRequest("POST", "/api/control", "action=cry&value=" + String(cryBefore), response);
  expect(crySensitivity.load() == cryBefore && cryTrackKey == cryKeyBefore, "cry help settings restored");

  // The settings screens on the device.
  const Page pageBefore = page;
  requestPage(SETTINGS); vTaskDelay(pdMS_TO_TICKS(600));
  const bool firstOpens = page == SETTINGS;
  requestPage(SETTINGS2); vTaskDelay(pdMS_TO_TICKS(600));
  expect(firstOpens && page == SETTINGS2, "settings screens open on the device");
  requestPage(pageBefore == SETTINGS || pageBefore == SETTINGS2 ? HOME : pageBefore);

  diagnosticRequest("POST", "/api/control", "action=track&value=" + startTrack, response);
  diagnosticRequest("POST", "/api/control", "action=timer&value=" + String(startTimer == 15 || startTimer == 60 ? startTimer : 30), response);
  diagnosticRequest("POST", "/api/control", "action=volume&value=" + String(startVolume >= 0 ? startVolume : 3), response);
  finish();
}
// ---- USB tools -----------------------------------------------------------------
// BGPUT <name.mp3|fx:name.mp3> <size> <crc32 hex>, then base64 lines (<= 4096
// chars) each answered "[PUT] OK <bytes>"; ends "[PUT] DONE" or "[PUT] ERR".
// BGSHOT prints the last frame (412 x 412 RGB565 little endian) as base64.
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
  crc = ~crc;
  while (length--) {
    crc ^= *data++;
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320UL & (0 - (crc & 1)));
  }
  return ~crc;
}
struct SerialPut { bool active; File file; String target; size_t size, received; uint32_t crc, expected, lastMs; } serialPut = {};
void putFail(const char* why) {
  if (serialPut.file) serialPut.file.close();
  FFat.remove("/sounds/serial.part");
  serialPut.active = false;
  Serial.printf("[PUT] ERR %s\n", why);
}
void putBegin(char* args) {
  char* name = strtok(args, " ");
  char* size = strtok(nullptr, " ");
  char* crc = strtok(nullptr, " ");
  if (!name || !size || !crc) { Serial.println("[PUT] ERR usage"); return; }
  String file = name, folder = "/sounds/";
  if (file.startsWith("fx:")) { folder = "/fx/"; file = file.substring(3); }
  if (!file.endsWith(".mp3") || !soundBaseName(file.substring(0, file.length() - 4))) { Serial.println("[PUT] ERR name"); return; }
  const long bytes = atol(size);
  if (!soundStorageReady || bytes < 128 || (size_t)bytes > MAX_NOISE_SIZE) { Serial.println("[PUT] ERR size"); return; }
  if (FFat.freeBytes() < allocationFor((size_t)bytes) + STORAGE_RESERVE) { Serial.println("[PUT] ERR full"); return; }
  if (noisePlaying) stopNoise();
  serialPut.file = FFat.open("/sounds/serial.part", FILE_WRITE);
  if (!serialPut.file) { Serial.println("[PUT] ERR open"); return; }
  serialPut.active = true; serialPut.target = folder + file; serialPut.size = (size_t)bytes;
  serialPut.received = 0; serialPut.crc = 0; serialPut.expected = strtoul(crc, nullptr, 16); serialPut.lastMs = millis();
  Serial.println("[PUT] READY");
}
// Data line: "<offset> <base64>". USB can drop a few bytes now and then, so a
// damaged or out-of-place line is answered "[PUT] RETRY <bytes received>" and
// the PC resends from there; a repeated line is acknowledged without writing.
void putData(char* line, size_t length) {
  static uint8_t block[3200];
  serialPut.lastMs = millis();
  char* space = strchr(line, ' ');
  char* end = nullptr;
  const unsigned long offset = space ? strtoul(line, &end, 10) : 0;
  if (!space || end != space) { Serial.printf("[PUT] RETRY %u\n", (unsigned)serialPut.received); return; }
  if (offset < serialPut.received) { Serial.printf("[PUT] OK %u\n", (unsigned)serialPut.received); return; }
  if (offset > serialPut.received) { Serial.printf("[PUT] RETRY %u\n", (unsigned)serialPut.received); return; }
  const char* text = space + 1;
  const size_t textLength = length - (size_t)(text - line);
  size_t decoded = 0;
  if (!textLength || mbedtls_base64_decode(block, sizeof(block), &decoded, (const unsigned char*)text, textLength) != 0 || !decoded) {
    Serial.printf("[PUT] RETRY %u\n", (unsigned)serialPut.received);
    return;
  }
  if (serialPut.received + decoded > serialPut.size) { Serial.printf("[PUT] RETRY %u\n", (unsigned)serialPut.received); return; }
  if (serialPut.file.write(block, decoded) != decoded) { putFail("write"); return; }
  serialPut.crc = crc32Update(serialPut.crc, block, decoded);
  serialPut.received += decoded; serialPut.lastMs = millis();
  if (serialPut.received < serialPut.size) { Serial.printf("[PUT] OK %u\n", (unsigned)serialPut.received); return; }
  serialPut.file.close();
  serialPut.active = false;
  if (serialPut.crc != serialPut.expected) { FFat.remove("/sounds/serial.part"); Serial.println("[PUT] ERR crc"); return; }
  FFat.remove(serialPut.target);
  if (!FFat.rename("/sounds/serial.part", serialPut.target)) { FFat.remove("/sounds/serial.part"); Serial.println("[PUT] ERR rename"); return; }
  scanTracks();
  if (page == NOISE) requestPage(NOISE);
  Serial.printf("[PUT] DONE %s %u\n", serialPut.target.c_str(), (unsigned)serialPut.received);
}
void screenshot() {
  if (!Lvgl_ScreenCopy) { Serial.println("[SHOT] ERR no copy"); return; }
  const uint8_t* bytes = (const uint8_t*)Lvgl_ScreenCopy;
  const size_t total = (size_t)W * H * 2;
  static unsigned char line[4100];
  Serial.printf("[SHOT] BEGIN %d %d %u\n", W, H, (unsigned)total);
  for (size_t at = 0; at < total; at += 3072) {
    size_t written = 0;
    mbedtls_base64_encode(line, sizeof(line) - 1, &written, bytes + at, std::min((size_t)3072, total - at));
    line[written] = '\0';
    Serial.println((const char*)line);
    if ((at / 3072) % 8 == 0) audio.loop();
  }
  Serial.printf("[SHOT] END %08lx\n", (unsigned long)crc32Update(0, bytes, total));
}
bool pageByName(const char* name, Page& out) {
  static const struct { const char* name; Page page; } pages[] = {
    {"home", HOME}, {"nap", NAP}, {"noise", NOISE}, {"album", ALBUM}, {"phone", PHONE},
    {"feed", FEED}, {"glance", GLANCE}, {"look", LOOK}, {"frame", FRAME},
    {"settings", SETTINGS}, {"settings2", SETTINGS2}};
  for (const auto& p : pages) if (strcmp(name, p.name) == 0) { out = p.page; return true; }
  return false;
}
void serialCommand(char* command) {
  // BGMIC <ms>: the board microphone's level (see BabyGrip_Mic.h). It records on
  // core 0 while BabyGrip keeps running, so sound dropouts show as 20 ms gaps.
  if (strncmp(command, "BGMIC", 5) == 0) {
    const uint32_t ms = constrain(command[5] == ' ' ? strtoul(command + 6, nullptr, 10) : 2000UL, 200UL, 10000UL);
    if (micMeasureMs.load()) { Serial.println("[MIC] ERR already recording"); return; }
    micMeasureMs.store(ms);
    ensureMicTask();
    return;
  }
  // BGCRY: cry-help status. BGCRY 0-3 sets it (off, low, medium, high).
  // BGCRY SIM acts as if a cry was heard. BGCRY LISTEN <s> only counts what the
  // detector would catch, also while a sound plays (for tests with the speaker).
  if (strncmp(command, "BGCRY", 5) == 0) {
    const char* arg = command[5] == ' ' ? command + 6 : "";
    if (strcmp(arg, "SIM") == 0) { cryPending.store(true); Serial.println("[CRY] SIM"); return; }
    if (strncmp(arg, "LISTEN ", 7) == 0) {
      const uint32_t s = constrain(strtoul(arg + 7, nullptr, 10), 2UL, 60UL);
      cryListenHits.store(0); cryListenMaxShare = 0; cryListenMaxVoiced = 0; cryListenMaxPauses = 0;
      cryListenUntilMs.store(millis() + s * 1000UL);
      ensureMicTask();
      Serial.printf("[CRY] LISTEN %u s\n", (unsigned)s);
      return;
    }
    if (arg[0] >= '0' && arg[0] <= '3' && !arg[1]) setCryHelp(arg[0] - '0');
    const int last = lastLogIndex([](const LogRecord& r) { return r.kind == LOG_CRY; });
    Serial.printf("[CRY] %s track=%s level=%.1f floor=%.1f loud=%.2f voiced=%.2f pauses=%d mic=%d last_ago=%ld\n",
                  CRY_NAMES[crySensitivity.load()], cryTrackKey.c_str(), (float)cryLevelDb, (float)cryFloorDb,
                  (float)cryShare, (float)cryVoicedShare, (int)cryPauses, micTaskRunning.load() ? 1 : 0,
                  last >= 0 ? (long)logAgo(logs[last]) : -1L);
    return;
  }
  // BGLOG: the last 10 log entries (id kind seconds-ago). BGLOGDEL <id> removes one (test entries).
  if (strcmp(command, "BGLOG") == 0) {
    for (int i = std::max(0, logCount - 10); i < logCount; ++i)
      Serial.printf("[LOG] id=%lu kind=%u ago=%ld\n", (unsigned long)logs[i].id, logs[i].kind, (long)logAgo(logs[i]));
    Serial.printf("[LOG] END count=%d\n", logCount);
    return;
  }
  if (strncmp(command, "BGLOGDEL ", 9) == 0) {
    const bool ok = deleteLog((uint32_t)strtoul(command + 9, nullptr, 10));
    Serial.printf("[LOG] %s\n", ok ? "DELETED" : "ERR not found");
    return;
  }
  // BGACT <name>: press a settings button, as a finger would (for tests).
  if (strncmp(command, "BGACT ", 6) == 0) {
    static const struct { const char* name; Action action; } acts[] = {
      {"settings", ACT_SETTINGS}, {"settings2", ACT_SETTINGS2}, {"slide", ACT_SET_SLIDE},
      {"slide5", ACT_SET_SLIDE_5}, {"slide10", ACT_SET_SLIDE_10}, {"slide30", ACT_SET_SLIDE_30},
      {"light-", ACT_SET_LIGHT_DOWN}, {"light+", ACT_SET_LIGHT_UP}, {"nightauto", ACT_SET_NIGHT_AUTO},
      {"from-", ACT_SET_FROM_DOWN}, {"from+", ACT_SET_FROM_UP}, {"to-", ACT_SET_TO_DOWN}, {"to+", ACT_SET_TO_UP},
      {"cry", ACT_SET_CRY}, {"crysound", ACT_SET_CRY_SOUND}, {"home", ACT_HOME}};
    for (const auto& x : acts)
      if (strcmp(command + 6, x.name) == 0) {
        lv_disp_trig_activity(nullptr);
        runAction(x.action);
        Serial.printf("[ACT] OK %s\n", x.name);
        return;
      }
    Serial.println("[ACT] ERR unknown");
    return;
  }
  // BGTIMER <s>: the playing sound stops in s seconds (tests the fade-out).
  if (strncmp(command, "BGTIMER ", 8) == 0) {
    const long s = strtol(command + 8, nullptr, 10);
    if (!noisePlaying || s < 1 || s > 3600) { Serial.println("[TIMER] ERR play a sound first, 1-3600 s"); return; }
    noiseEndsMs = millis() + (uint32_t)s * 1000UL;
    Serial.printf("[TIMER] OK stops in %ld s\n", s);
    return;
  }
  // BGSLIDE <3-300>: seconds between photos (saved, same as the phone setting).
  if (strncmp(command, "BGSLIDE ", 8) == 0) {
    const long s = strtol(command + 8, nullptr, 10);
    if (s < 3 || s > 300) { Serial.println("[SLIDE] ERR 3-300"); return; }
    slideSeconds = (uint32_t)s; prefs.putUInt("slidesec", slideSeconds); lastSlideMs = millis();
    Serial.printf("[SLIDE] OK %u s\n", (unsigned)slideSeconds);
    return;
  }
  if (strcmp(command, "BGSHOT") == 0) { screenshot(); return; }
  if (strncmp(command, "BGPUT ", 6) == 0) { putBegin(command + 6); return; }
  if (strcmp(command, "BGSOUNDS") == 0) {
    for (int i = 0; i < trackCount; ++i) Serial.printf("[SOUND] %s %s%s\n", tracks[i].key, tracks[i].title, i == trackIndex ? " *" : "");
    Serial.printf("[SOUND] END free=%u\n", (unsigned)FFat.freeBytes());
    return;
  }
  if (strncmp(command, "BGDEL ", 6) == 0) {
    const String file = command + 6;
    const bool fx = file.startsWith("fx:");
    const String name = fx ? file.substring(3) : file;
    const bool ok = name.endsWith(".mp3") && soundBaseName(name.substring(0, name.length() - 4)) &&
                    FFat.remove((fx ? "/fx/" : "/sounds/") + name);
    if (ok && !fx) { if (noisePlaying) stopNoise(); scanTracks(); }
    Serial.printf("[DEL] %s %s\n", ok ? "OK" : "ERR", command + 6);
    return;
  }
  if (strncmp(command, "BGPAGE ", 7) == 0) {
    Page next;
    if (!pageByName(command + 7, next)) { Serial.println("[PAGE] ERR"); return; }
    if (next == LOOK) { lookPattern = 0; lookInvert = false; lookTouchedMs = lookFlipMs = millis(); }
    if (next == GLANCE) glanceReturn = HOME;
    lv_disp_trig_activity(nullptr);  // counts as a touch, so the photo show waits two minutes
    requestPage(next);
    Serial.println("[PAGE] OK");
    return;
  }
  if (strncmp(command, "BGTIME ", 7) == 0) {
    unsigned long long epoch = 0; long offset = 0;
    const bool ok = sscanf(command + 7, "%llu %ld", &epoch, &offset) == 2 && epoch >= 1735689600ULL &&
                    epoch <= 4102444800ULL && offset >= -840 && offset <= 840 && setClock(epoch, offset);
    char now[24]; formatClock(now, sizeof(now));
    Serial.printf("[TIME] %s %s\n", ok ? "OK" : "ERR", now);
    return;
  }
  if (strncmp(command, "BGNIGHT ", 8) == 0) { setNight(command[8] == '1'); Serial.println("[NIGHT] OK"); return; }
  if (strncmp(command, "BGGLANCE ", 9) == 0) {
    glanceAuto = command[9] == '1';
    if (!glanceAuto && page == GLANCE) requestPage(HOME);
    Serial.println("[GLANCE] OK (until restart)");
    return;
  }
  // BGPLAY <track key> [volume 0-5] starts a sound (as PLAY would); BGPLAY stop ends it.
  if (strncmp(command, "BGPLAY ", 7) == 0) {
    char key[24] = {0}; int level = -1;
    sscanf(command + 7, "%23s %d", key, &level);
    if (strcmp(key, "stop") == 0) { if (noisePlaying) stopNoise(); Serial.println("[PLAY] STOPPED"); requestPage(page); return; }
    const int at = findTrack(key);
    if (at < 0 || level > BG_ONBOARD_VOLUME_CAP) { Serial.println("[PLAY] ERR unknown track or volume"); return; }
    if (level >= 0) noiseVolumeLevel = level;
    if (at != trackIndex || !noisePlaying) { if (noisePlaying) stopNoise(); trackIndex = at; startNoise(); }
    else applyVolume();
    Serial.printf("[PLAY] %s %s volume=%d gain=%.1fdB\n", noisePlaying ? "OK" : "ERR", tracks[trackIndex].key,
                  noiseVolumeLevel, levelDb(noiseVolumeLevel));
    requestPage(page);
    return;
  }
  if (strcmp(command, "BGSTATUS") == 0) Serial.println(stateJson());
  if (strcmp(command, "BGLIST") == 0) {
    Serial.printf("[LIST] photos=%d\n", photoCount);
    for (int i = 0; i < photoCount; ++i) Serial.printf("[LIST] %s %s\n", volumes[photos[i].volume].label, photos[i].name);
    Serial.println("[LIST] END");
  }
  if (strcmp(command, "BGTEST") == 0 && !diagnosticRunning.exchange(true)) {
    if (xTaskCreate(diagnosticTask, "BabyGrip checks", 12288, nullptr, 1, nullptr) != pdPASS)
      { diagnosticRunning.store(false); Serial.println("[CHECK] Failed to start task"); }
  }
}
void diagnosticPoll() {
  static char command[4200]; static size_t count = 0;
  if (serialPut.active && millis() - serialPut.lastMs > 15000) putFail("timeout");
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n') {
      command[count] = '\0';
      const size_t length = count;
      count = 0;
      if (serialPut.active) {
        if (length) putData(command, length);
        continue;
      }
      serialCommand(command);
      continue;
    }
    if (c != '\r' && count < sizeof(command) - 1) command[count++] = c;
  }
}
