// BabyGrip for Waveshare ESP32-S3-Touch-LCD-1.46B.
// Album build: photos are 412 x 412 JPEG files on two internal flash volumes,
// "ffat" (same place as before, so earlier files stay) and "media2" (the
// unused second OTA slot). The built-in sounds play straight from the
// firmware image, so they no longer take photo space.
#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FFat.h>
#include <vfs_api.h>
#include <WebServer.h>
#include <dirent.h>
#include <sys/time.h>
#include <esp_random.h>
#include <atomic>
#include <algorithm>
#include <Preferences.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "BabyGrip.h"
#include "BabyGrip_Config.h"
#include "Audio_PCM5101.h"
#include "BabyGrip_PhoneUI.h"
#include "BabyGrip_SoundData.h"
#include "BabyGrip_LullabyData.h"
#include "BabyGrip_TestJpeg.h"
#include "BabyGrip_Jpeg.h"
#include "BabyGrip_RomFS.h"
#include "BabyGrip_Fonts.h"
#include "RTC_PCF85063.h"
#include "Gyro_QMI8658.h"
#include <mbedtls/base64.h>
#include <ESP_I2S.h>

// Board drivers (defined in Display_SPD2010.cpp / LVGL_Driver.cpp).
void Set_Backlight(uint8_t Light);
extern uint16_t* Lvgl_ScreenCopy;
void Lvgl_EnableScreenCopy(void);

namespace {
constexpr int W = 412, H = 412;
constexpr size_t PHOTO_SIZE = (size_t)W * H * 2;
constexpr int MAX_PHOTOS = 2000;
constexpr size_t MAX_JPEG_SIZE = 196608;
constexpr size_t STORAGE_ALLOCATION_UNIT = 4096;
constexpr size_t MAX_NOISE_SIZE = 2 * 1024 * 1024;
constexpr size_t STORAGE_RESERVE = 65536;
constexpr size_t MULTIPART_ALLOWANCE = 4096;
constexpr int MAX_TRACKS = 24;
enum Page { HOME, NAP, NOISE, ALBUM, PHONE, FEED, GLANCE, LOOK, FRAME, SETTINGS, SETTINGS2 };
enum Action {
  ACT_HOME=1, ACT_NAP, ACT_NOISE, ACT_ALBUM, ACT_NAP_TOGGLE,
  ACT_NOISE_TOGGLE, ACT_VOLUME_DOWN, ACT_VOLUME_UP,
  ACT_TIMER_15, ACT_TIMER_30, ACT_TIMER_60,
  ACT_PREVIOUS, ACT_NEXT, ACT_SLIDESHOW, ACT_PHONE,
  ACT_FEED, ACT_FEED_LEFT, ACT_FEED_RIGHT, ACT_BOTTLE, ACT_DIAPER, ACT_UNDO,
  ACT_NIGHT, ACT_TRACK_PREV, ACT_TRACK_NEXT, ACT_LOOK, ACT_LOOK_TAP,
  // on-device settings
  ACT_SETTINGS, ACT_SETTINGS2, ACT_SET_SLIDE, ACT_SET_SLIDE_5, ACT_SET_SLIDE_10, ACT_SET_SLIDE_30,
  ACT_SET_LIGHT_DOWN, ACT_SET_LIGHT_UP, ACT_SET_NIGHT_AUTO, ACT_SET_FROM_DOWN, ACT_SET_FROM_UP,
  ACT_SET_TO_DOWN, ACT_SET_TO_UP, ACT_SET_CRY, ACT_SET_CRY_SOUND
};

// ---- storage -------------------------------------------------------------
fs::F_Fat Media2(fs::FSImplPtr(new VFSImpl()));
struct Volume {
  fs::F_Fat* fs;
  const char* label;  // partition label
  const char* mount;  // VFS mount point
  const char* pref;   // set once the volume has been initialized
  uint8_t maxFiles;
  bool ready;
};
Volume volumes[] = {
  {&FFat, "ffat", "/ffat", "media_v1", 10, false},
  {&Media2, "media2", "/media2", "media2_v1", 6, false},
};
constexpr int VOLUME_COUNT = sizeof(volumes) / sizeof(volumes[0]);

const bgrom::Entry romSounds[] = {
  {"/white_noise.mp3", BG_SAMPLE_NOISE, sizeof(BG_SAMPLE_NOISE)},
  {"/lullaby.mp3", BG_SAMPLE_LULLABY, sizeof(BG_SAMPLE_LULLABY)},
};
fs::FS RomFS(fs::FSImplPtr(new bgrom::RomFSImpl(romSounds, 2)));

struct PhotoEntry {
  char name[28];
  uint8_t volume;
};
PhotoEntry* photos = nullptr;  // sorted by name, in PSRAM
int photoCount = 0, photoIndex = 0;
uint32_t photosRev = 1;

Page page = HOME;
Page pendingPage = HOME;
bool needsRebuild = false;
lv_obj_t* photoObj = nullptr;
lv_obj_t* elapsedLabel = nullptr;
lv_obj_t* totalLabel = nullptr;
lv_obj_t* napButtonLabel = nullptr;
lv_obj_t* noiseStatus = nullptr;
lv_obj_t* noiseVolume = nullptr;
lv_obj_t* albumStatus = nullptr;
lv_obj_t* slideButtonLabel = nullptr;
uint8_t* pixels = nullptr;
uint8_t* photoStaging = nullptr;
lv_img_dsc_t picture = {};
bool autoplay = false, napRunning = false, noisePlaying = false, clockAvailable = false;
uint32_t napStartedMs = 0, lastSlideMs = 0, lastUiMs = 0;
// Photo show (the default screen): BabyGrip starts on it, and after two minutes
// without a touch it shows only the photos again, with no text (FRAME). A tap
// returns to the home screen. At night the photos are dimmed and warm-tinted.
bool slideAuto = true;
uint32_t slideSeconds = 5;
const uint32_t FRAME_IDLE_MS = 120000;
uint32_t photoLoadMs = 0, photoLoadMaxMs = 0;  // how long the last / slowest photo change took
uint32_t noiseEndsMs = 0, lastAudioRetryMs = 0;
uint32_t finishedTodaySecs = 0;
uint32_t persistedTotalSecs = 0;
uint32_t storedDay = 0;
uint64_t persistedStartEpoch = 0;
bool persistedActive = false, recovered = false, ntpStarted = false;
int noiseTimerMinutes = 30, noiseVolumeLevel = 3;
Preferences prefs;
String soundMessage;
class BabyGripServer : public WebServer {
 public:
  using WebServer::WebServer;
  bool listening() { return (bool)_server; }
};
BabyGripServer phone(80);
bool storageReady = false;       // at least one photo volume is mounted
bool soundStorageReady = false;  // ffat is mounted (custom MP3 slot)
String apName, apPassword, csrfToken;
File uploadFile;
String uploadError, uploadErrorCode, uploadTarget, uploadPart;
size_t uploadedBytes = 0, uploadExpected = 0;
uint32_t uploadStartedMs = 0;
int uploadVolume = 0;
bool uploadIsNoise = false, uploadIsJpeg = false, uploadStarted = false;
bool uploadEnded = false, uploadDuplicate = false;
String uploadSoundKey;

// ---- night mode ------------------------------------------------------------
struct Palette { uint32_t bg, text, accent, paleBg, paleText, darkBg, darkText; };
const Palette DAY_COLORS = {0x171929, 0xF7F0FF, 0xDDB6D9, 0xDDB6D9, 0x242137, 0x38324B, 0xFFFFFF};
// Warm, dim colours: no blue or white light at night.
const Palette NIGHT_COLORS = {0x000000, 0xD9822B, 0xA9581C, 0x4A2406, 0xFFA040, 0x1C0F05, 0xC87028};
bool nightMode = false, nightAuto = true;
int nightStartMin = 20 * 60, nightEndMin = 7 * 60;   // 8 pm to 7 am
int dayLight = 50, nightLight = 6;                   // backlight percent
int nightWindowSeen = -1;                            // last schedule state seen
const Palette& pal() { return nightMode ? NIGHT_COLORS : DAY_COLORS; }

// Labels refreshed once a second on HOME and FEED.
lv_obj_t *liveClock = nullptr, *liveCaption = nullptr, *liveBig = nullptr,
         *liveInfo = nullptr, *liveToday = nullptr;

// ---- sound library -----------------------------------------------------------
// WHITE (built in, or an MP3 uploaded by an earlier version into its slot),
// every /sounds/*.mp3 on ffat sorted by name, then the built-in LULLABY.
enum SoundSource : uint8_t { SRC_ROM_WHITE, SRC_LEGACY_WHITE, SRC_FILE, SRC_ROM_LULLABY };
struct Track { char key[24]; char title[20]; uint8_t source; };
Track tracks[MAX_TRACKS];
int trackCount = 0, trackIndex = 0;

bool customNoise() { return soundStorageReady && FFat.exists("/white_noise.mp3"); }
// Sound file base names: 1-20 of [a-z0-9_-].
bool soundBaseName(const String& base) {
  if (!base.length() || base.length() > 20) return false;
  for (size_t i = 0; i < base.length(); ++i) {
    const char c = base[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  }
  return true;
}
// "02-ocean_waves" -> "OCEAN WAVES" (an "NN-" prefix only sets the order).
void soundTitle(const char* base, char* out, size_t size) {
  if (isdigit((unsigned char)base[0]) && isdigit((unsigned char)base[1]) && base[2] == '-' && base[3]) base += 3;
  size_t n = 0;
  for (; *base && n + 1 < size; ++base)
    out[n++] = (*base == '_' || *base == '-') ? ' ' : (char)toupper((unsigned char)*base);
  out[n] = '\0';
}
String trackPath(int i) {
  if (tracks[i].source == SRC_FILE) return String("/sounds/") + (tracks[i].key + 2) + ".mp3";
  return tracks[i].source == SRC_ROM_LULLABY ? "/lullaby.mp3" : "/white_noise.mp3";
}
fs::FS& trackFs(int i) {
  return tracks[i].source == SRC_FILE || tracks[i].source == SRC_LEGACY_WHITE ? (fs::FS&)FFat : (fs::FS&)RomFS;
}
int findTrack(const String& key) {
  for (int i = 0; i < trackCount; ++i) if (key == tracks[i].key) return i;
  return -1;
}
void addTrack(const char* key, const char* title, uint8_t source) {
  if (trackCount >= MAX_TRACKS) return;
  strlcpy(tracks[trackCount].key, key, sizeof(tracks[trackCount].key));
  strlcpy(tracks[trackCount].title, title, sizeof(tracks[trackCount].title));
  tracks[trackCount].source = source;
  ++trackCount;
}
int userTrackCount() {
  int n = 0;
  for (int i = 0; i < trackCount; ++i) n += tracks[i].source == SRC_FILE;
  return n;
}
void scanTracks() {
  String keep = trackCount ? String(tracks[trackIndex].key)
                           : prefs.getString("trackkey", prefs.getBool("lullaby", false) ? "lullaby" : "white");
  trackCount = 0;
  addTrack("white", "WHITE NOISE", customNoise() ? SRC_LEGACY_WHITE : SRC_ROM_WHITE);
  if (soundStorageReady) {
    struct SoundName { char s[24]; };
    SoundName names[MAX_TRACKS];
    int found = 0;
    DIR* dir = opendir("/ffat/sounds");
    struct dirent* item;
    while (dir && found < MAX_TRACKS - 2 && (item = readdir(dir)) != nullptr) {
      String name = item->d_name;
      if (item->d_type == DT_DIR || !name.endsWith(".mp3")) continue;
      const String base = name.substring(0, name.length() - 4);
      if (!soundBaseName(base)) continue;
      strlcpy(names[found++].s, base.c_str(), sizeof(names[0].s));
    }
    if (dir) closedir(dir);
    std::sort(names, names + found, [](const SoundName& a, const SoundName& b) { return strcmp(a.s, b.s) < 0; });
    for (int i = 0; i < found; ++i) {
      char key[24], title[20];
      snprintf(key, sizeof(key), "f:%s", names[i].s);
      soundTitle(names[i].s, title, sizeof(title));
      addTrack(key, title, SRC_FILE);
    }
  }
  addTrack("lullaby", "LULLABY", SRC_ROM_LULLABY);
  const int at = findTrack(keep);
  trackIndex = at >= 0 ? at : 0;
}

size_t allocationFor(size_t bytes) {
  return ((bytes + STORAGE_ALLOCATION_UNIT - 1) / STORAGE_ALLOCATION_UNIT) * STORAGE_ALLOCATION_UNIT;
}
size_t volumeFree(int v) { return volumes[v].ready ? volumes[v].fs->freeBytes() : 0; }
size_t storageTotal() {
  size_t total = 0;
  for (int v = 0; v < VOLUME_COUNT; ++v) if (volumes[v].ready) total += volumes[v].fs->totalBytes();
  return total;
}
size_t storageUsed() {
  size_t used = 0;
  for (int v = 0; v < VOLUME_COUNT; ++v) if (volumes[v].ready) used += volumes[v].fs->usedBytes();
  return used;
}
// Bytes still available for photos, after keeping a small reserve per volume.
size_t photoFreeBytes() {
  size_t free = 0;
  for (int v = 0; v < VOLUME_COUNT; ++v) {
    const size_t f = volumeFree(v);
    if (f > STORAGE_RESERVE) free += f - STORAGE_RESERVE;
  }
  return free;
}
int readyVolumes() {
  int n = 0;
  for (int v = 0; v < VOLUME_COUNT; ++v) n += volumes[v].ready ? 1 : 0;
  return n;
}
int pickPhotoVolume(size_t bytes) {
  const size_t need = allocationFor(bytes) + STORAGE_RESERVE;
  int best = -1;
  size_t bestFree = 0;
  for (int v = 0; v < VOLUME_COUNT; ++v) {
    const size_t f = volumeFree(v);
    if (f >= need && f > bestFree) { best = v; bestFree = f; }
  }
  return best;
}

// ---- photo names -----------------------------------------------------------
bool hexRun(const char* text, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}
bool endsWith(const char* text, const char* suffix) {
  const size_t a = strlen(text), b = strlen(suffix);
  return a >= b && strcmp(text + a - b, suffix) == 0;
}
bool jpegPhotoName(const char* name) { return endsWith(name, ".jpg") || endsWith(name, ".jpeg"); }
// New uploads: "p" + capture time (8 hex) + photo id (8 hex) + ".jpg".
// Sorting by name therefore plays the album in the order the photos were taken.
bool uploadPhotoName(const char* name) {
  return strlen(name) == 21 && name[0] == 'p' && hexRun(name + 1, 16) && strcmp(name + 17, ".jpg") == 0;
}
// Any photo name an earlier BabyGrip version may have stored.
bool safePhotoName(const char* name) {
  const size_t length = strlen(name);
  const size_t suffix = endsWith(name, ".jpeg") ? 5 : 4;
  if (length <= suffix || length >= sizeof(PhotoEntry::name) ||
      (!endsWith(name, ".rgb") && !jpegPhotoName(name))) return false;
  for (size_t i = 0; i < length - suffix; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  }
  return true;
}
String photoPath(const char* name) { return String("/photos/") + name; }

int findPhoto(const char* name) {
  int lo = 0, hi = photoCount - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const int c = strcmp(photos[mid].name, name);
    if (!c) return mid;
    if (c < 0) lo = mid + 1; else hi = mid - 1;
  }
  return -1;
}
void insertPhoto(const char* name, uint8_t volume) {
  if (!photos || photoCount >= MAX_PHOTOS) return;
  int at = 0;
  while (at < photoCount && strcmp(photos[at].name, name) < 0) ++at;
  memmove(&photos[at + 1], &photos[at], (size_t)(photoCount - at) * sizeof(PhotoEntry));
  strlcpy(photos[at].name, name, sizeof(photos[at].name));
  photos[at].volume = volume;
  ++photoCount;
  if (photoCount > 1 && photoIndex >= at) ++photoIndex;
  ++photosRev;
}
void erasePhotoAt(int at) {
  if (at < 0 || at >= photoCount) return;
  memmove(&photos[at], &photos[at + 1], (size_t)(photoCount - at - 1) * sizeof(PhotoEntry));
  --photoCount;
  if (photoIndex > at) --photoIndex;
  if (photoIndex >= photoCount) photoIndex = 0;
  ++photosRev;
}
void scanPhotos() {
  photoCount = 0;
  if (!photos) return;
  for (int v = 0; v < VOLUME_COUNT; ++v) {
    if (!volumes[v].ready) continue;
    const String directory = String(volumes[v].mount) + "/photos";
    DIR* dir = opendir(directory.c_str());
    if (!dir) continue;
    struct dirent* item;
    while (photoCount < MAX_PHOTOS && (item = readdir(dir)) != nullptr) {
      if (item->d_type == DT_DIR || !safePhotoName(item->d_name)) continue;
      strlcpy(photos[photoCount].name, item->d_name, sizeof(photos[photoCount].name));
      photos[photoCount].volume = (uint8_t)v;
      ++photoCount;
    }
    closedir(dir);
  }
  std::sort(photos, photos + photoCount,
            [](const PhotoEntry& a, const PhotoEntry& b) { return strcmp(a.name, b.name) < 0; });
  int unique = 0;  // a name on both volumes is listed once
  for (int i = 0; i < photoCount; ++i)
    if (!unique || strcmp(photos[unique - 1].name, photos[i].name) != 0) photos[unique++] = photos[i];
  photoCount = unique;
  if (photoIndex >= photoCount || photoIndex < 0) photoIndex = 0;
  ++photosRev;
}

// ---- photo display -----------------------------------------------------------
size_t readPhotoJpeg(void* opaque, size_t offset, uint8_t* destination, size_t count) {
  File& file = *static_cast<File*>(opaque);
  if (!file || offset > file.size() || count > file.size() - offset) return 0;
  if (!file.seek(offset)) return 0;
  if (!destination) return file.seek(offset + count) ? count : 0;
  return file.read(destination, count);
}
void servicePhotoDecode() {
  audio.loop();
  yield();
}
bool decodePhoto(File& file, bool jpeg, uint8_t* destination) {
  if (!destination || !file) return false;
  if (jpeg)
    return BabyGripDecodeJpeg(file.size(), readPhotoJpeg, &file, destination,
                              PHOTO_SIZE, servicePhotoDecode);
  return file.size() == PHOTO_SIZE && file.seek(0) &&
         file.read(destination, PHOTO_SIZE) == PHOTO_SIZE;
}
bool loadPhoto(int idx) {
  if (!pixels || !photoStaging || !photoCount) return false;
  idx = ((idx % photoCount) + photoCount) % photoCount;
  const uint32_t startedMs = millis();
  const PhotoEntry& entry = photos[idx];
  File f = volumes[entry.volume].fs->open(photoPath(entry.name), FILE_READ);
  const bool decoded = decodePhoto(f, jpegPhotoName(entry.name), photoStaging);
  if (f) f.close();
  photoLoadMs = millis() - startedMs;
  photoLoadMaxMs = std::max(photoLoadMaxMs, photoLoadMs);
  if (!decoded) return false;
  // A failed decode may change the staging buffer, never the displayed photo.
  lv_img_cache_invalidate_src(&picture);
  memcpy(pixels, photoStaging, PHOTO_SIZE);
  photoIndex = idx;
  if (photoObj) { lv_img_set_src(photoObj, &picture); lv_obj_invalidate(photoObj); }
  return true;
}
// Move by step, skipping up to two unreadable photos.
bool stepPhoto(int step) {
  if (!photoCount) return false;
  int target = photoIndex;
  for (int attempt = 0; attempt < 3 && attempt < photoCount; ++attempt) {
    target += step;
    if (loadPhoto(target)) return true;
  }
  photoIndex = ((target % photoCount) + photoCount) % photoCount;
  return false;
}

lv_color_t accent() { return lv_color_hex(pal().accent); }
lv_color_t textColor() { return lv_color_hex(pal().text); }
void wipe() {
  photoObj = elapsedLabel = totalLabel = napButtonLabel = noiseStatus =
    noiseVolume = albumStatus = slideButtonLabel = nullptr;
  liveClock = liveCaption = liveBig = liveInfo = liveToday = nullptr;
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(pal().bg), 0);
  lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
}
lv_obj_t* label(const char* content, int x, int y, int width=320,
                lv_color_t color=textColor(), const lv_font_t* font=nullptr) {
  lv_obj_t* o = lv_label_create(lv_scr_act());
  lv_label_set_text(o, content);
  lv_obj_set_width(o, width);
  lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(o, color, 0);
  lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
  if (font) lv_obj_set_style_text_font(o, font, 0);
  lv_obj_set_pos(o, x, y);
  return o;
}
// Changing a label redraws the whole screen, so only set changed text.
void setText(lv_obj_t* o, const char* text) {
  if (o && strcmp(lv_label_get_text(o), text) != 0) lv_label_set_text(o, text);
}
void showPage(Page next);
void requestPage(Page next) { pendingPage = next; needsRebuild = true; }
void onClick(lv_event_t* e);
lv_obj_t* button(const char* name, int x, int y, int width, int height,
                 Action action, bool pale=true, const lv_font_t* font=nullptr) {
  lv_obj_t* o = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, width, height);
  lv_obj_set_style_radius(o, 19, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(pale ? pal().paleBg : pal().darkBg), 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_add_event_cb(o, onClick, LV_EVENT_CLICKED,
                      (void*)(intptr_t)action);
  lv_obj_t* text = lv_label_create(o);
  lv_label_set_text(text, name);
  lv_obj_set_style_text_color(text, lv_color_hex(pale ? pal().paleText : pal().darkText), 0);
  if (font) lv_obj_set_style_text_font(text, font, 0);
  lv_obj_center(text);
  return text;
}
// A dark rounded panel that keeps text readable over a photo; tapping it opens `action`.
lv_obj_t* panel(int x, int y, int width, int height, Action action) {
  lv_obj_t* o = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, width, height);
  lv_obj_set_style_radius(o, 22, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(o, nightMode ? LV_OPA_TRANSP : LV_OPA_60, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, onClick, LV_EVENT_CLICKED, (void*)(intptr_t)action);
  return o;
}
void header(const char* title) {
  label(title, 76, 44, 260, accent());
  button("<", 64, 40, 46, 38, ACT_HOME, false);
}
#include "BabyGrip_Feed.h"
#include "BabyGrip_Mic.h"

// ---- clock -------------------------------------------------------------------
// The PCF85063 keeps UTC while the board has power, so the time and the log
// survive restarts and firmware updates. Its OS bit says the clock stopped.
void applyTimezone(long offsetMinutes) {
  char zone[24];
  snprintf(zone, sizeof(zone), "UTC%s%ld:%02ld", offsetMinutes > 0 ? "-" : "+", labs(offsetMinutes) / 60, labs(offsetMinutes) % 60);
  setenv("TZ", zone, 1);
  tzset();
}
uint8_t bcdByte(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0f)); }
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}
bool rtcValid = false;
void restoreClockFromRtc() {
  uint8_t raw[7] = {};
  if (I2C_Read(PCF85063_ADDRESS, RTC_SECOND_ADDR, raw, sizeof(raw))) { Serial.println("[BabyGrip] RTC not readable"); return; }
  if (raw[0] & 0x80) { Serial.println("[BabyGrip] RTC lost power; the phone page will set the time"); return; }
  const int year = bcdByte(raw[6]) + YEAR_OFFSET, month = bcdByte(raw[5] & 0x1f), day = bcdByte(raw[3] & 0x3f);
  const int hour = bcdByte(raw[2] & 0x3f), minute = bcdByte(raw[1] & 0x7f), second = bcdByte(raw[0] & 0x7f);
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) return;
  const int64_t epoch = daysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
  if (epoch < 1735689600LL || epoch > 4102444800LL) return;
  const struct timeval tv = {(time_t)epoch, 0};
  if (settimeofday(&tv, nullptr) == 0) {
    rtcValid = true;
    Serial.printf("[BabyGrip] Clock restored from RTC: %04d-%02d-%02d %02d:%02d:%02d UTC\n", year, month, day, hour, minute, second);
  }
}
void saveClockToRtc() {
  const time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  datetime_t t = {};
  t.year = (uint16_t)(utc.tm_year + 1900); t.month = (uint8_t)(utc.tm_mon + 1); t.day = (uint8_t)utc.tm_mday;
  t.dotw = (uint8_t)utc.tm_wday; t.hour = (uint8_t)utc.tm_hour; t.minute = (uint8_t)utc.tm_min; t.second = (uint8_t)utc.tm_sec;
  PCF85063_Set_All(t);  // also clears the oscillator-stop flag
  rtcValid = true;
}
// "3:42 PM"
void formatClock(char* out, size_t size, bool withSuffix = true) {
  if (!clockAvailable) { snprintf(out, size, "--:--"); return; }
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  const int hour = local.tm_hour % 12 ? local.tm_hour % 12 : 12;
  if (withSuffix) snprintf(out, size, "%d:%02d %s", hour, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
  else snprintf(out, size, "%d:%02d", hour, local.tm_min);
}
void formatClockAt(uint32_t epoch, char* out, size_t size) {
  const time_t t = (time_t)epoch;
  struct tm local;
  localtime_r(&t, &local);
  const int hour = local.tm_hour % 12 ? local.tm_hour % 12 : 12;
  snprintf(out, size, "%d:%02d %s", hour, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
}
// "2h 05m", "45m" (big font has only digits, ':', 'h', 'm', '-')
void formatAgo(int64_t seconds, char* out, size_t size) {
  if (seconds < 0) { snprintf(out, size, "--"); return; }
  const unsigned minutes = (unsigned)(seconds / 60);
  if (minutes < 60) snprintf(out, size, "%um", minutes);
  else snprintf(out, size, "%uh %02um", std::min(99u, minutes / 60), minutes % 60);
}
void formatTimer(uint32_t seconds, char* out, size_t size) {
  snprintf(out, size, "%u:%02u", (unsigned)(seconds / 60), (unsigned)(seconds % 60));
}
uint32_t dateKey() {
  time_t epoch = time(nullptr);
  if (epoch < 1735689600) return 0; // Until clock sync, do not claim calendar-day totals
  struct tm local;
  localtime_r(&epoch, &local);
  return (uint32_t)(local.tm_year + 1900) * 1000 + local.tm_yday;
}
void checkCalendar() {
  const uint32_t d = dateKey();
  if (!d) return;
  const bool firstSync = !clockAvailable;
  clockAvailable = true;
  if (firstSync) logClockArrived();
  const uint32_t offlineSecs = firstSync ? finishedTodaySecs : 0;
  if (firstSync && storedDay == d) {
    finishedTodaySecs = persistedTotalSecs + offlineSecs;
    prefs.putUInt("total", finishedTodaySecs);
  }
  if (storedDay != d) {
    storedDay = d;
    finishedTodaySecs = offlineSecs;
    prefs.putUInt("day", storedDay);
    prefs.putUInt("total", finishedTodaySecs);
  }
  if (!recovered) {
    recovered = true;
    if (persistedActive && persistedStartEpoch > 0) {
      const uint64_t now = (uint64_t)time(nullptr);
      if (now >= persistedStartEpoch && now - persistedStartEpoch < 24ULL*3600ULL) {
        napRunning = true;
        napStartedMs = millis() - (uint32_t)((now - persistedStartEpoch)*1000ULL);
        if (page == NAP) requestPage(NAP);
      } else {
        prefs.putBool("active", false);
      }
    }
  }
}
uint32_t napElapsed() {
  return napRunning ? (millis() - napStartedMs) / 1000UL : 0;
}
void fmtDuration(uint32_t seconds, char* out, size_t n) {
  snprintf(out, n, "%02luh %02lum", (unsigned long)(seconds/3600),
           (unsigned long)((seconds/60)%60));
}
void toggleNap() {
  if (!napRunning) {
    napRunning = true;
    napStartedMs = millis();
    persistedActive=false;
    persistedStartEpoch=0;
    prefs.putBool("active",false);
    if (clockAvailable) {
      prefs.putULong64("start", (uint64_t)time(nullptr));
      prefs.putBool("active", true);
    }
  } else {
    const uint32_t seconds = napElapsed();
    napRunning = false;
    finishedTodaySecs += seconds;
    if (clockAvailable) prefs.putUInt("total", finishedTodaySecs);
    prefs.putBool("active", false);
  }
}
void createPhoto(Action tap = ACT_ALBUM) {
  if (!pixels || !photoCount) {
    label("BABY PHOTO FRAME", 74, 132, 264, accent());
    label("Tap Wi-Fi to add photos from your phone", 62, 156, 288);
    return;
  }
  photoObj = lv_img_create(lv_scr_act());
  lv_img_set_src(photoObj, &picture);
  lv_obj_set_pos(photoObj, 0, 0);
  if (nightMode) {
    // Warm tint instead of the photo's blue and white light.
    lv_obj_set_style_img_recolor(photoObj, lv_color_hex(0xFF7A00), 0);
    lv_obj_set_style_img_recolor_opa(photoObj, LV_OPA_60, 0);
  }
  lv_obj_add_flag(photoObj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(photoObj, onClick, LV_EVENT_CLICKED,
                      (void*)(intptr_t)tap);
}

// ---- live text (once a second) ------------------------------------------------
const char* sideWord(bool right) { return right ? "RIGHT" : "LEFT"; }
void sidesText(int index, char* out, size_t size, bool clock) {
  const uint32_t l = feedSideSecs(index, false), r = feedSideSecs(index, true);
  if (clock) {
    char a[12], b[12];
    formatTimer(l, a, sizeof(a)); formatTimer(r, b, sizeof(b));
    snprintf(out, size, "L %s   R %s", a, b);
  } else snprintf(out, size, "L %um  R %um", (unsigned)(l / 60), (unsigned)(r / 60));
}
// What the last feed was, e.g. "LEFT - started 1:05 AM" or "BOTTLE 90 ml - 1:05 AM".
void lastFeedText(int index, char* out, size_t size) {
  const LogRecord& r = logs[index];
  char at[16] = "";
  if (!(r.flags & LF_PENDING) && clockAvailable) formatClockAt(r.start, at, sizeof(at));
  if (r.kind == LOG_BOTTLE) {
    if (r.ml) snprintf(out, size, "BOTTLE %u ml%s%s", r.ml, at[0] ? " - " : "", at);
    else snprintf(out, size, "BOTTLE%s%s", at[0] ? " - " : "", at);
  } else {
    char sides[24];
    sidesText(index, sides, sizeof(sides), false);
    snprintf(out, size, "%s%s%s%s", sides, at[0] ? " - " : "", at, (r.flags & LF_AUTO) ? " (auto-stop)" : "");
  }
}
void todayText(char* out, size_t size) {
  const DaySummary s = todaySummary();
  snprintf(out, size, "%s%d FEED%s - %d DIAPER%s", clockAvailable ? "TODAY: " : "SINCE POWER ON: ",
           s.feeds, s.feeds == 1 ? "" : "S", s.diapers, s.diapers == 1 ? "" : "S");
}
bool undoShown = false;
void refreshLive() {
  char text[64];
  const int last = lastFeedIndex();
  const bool running = runningLog >= 0;
  if (liveClock) {
    if (page == GLANCE) formatClock(text, sizeof(text), false);
    else if (page == HOME && !nightMode && !clockAvailable) snprintf(text, sizeof(text), "BabyGrip");
    else formatClock(text, sizeof(text));
    setText(liveClock, text);
  }
  if (page == HOME && !nightMode) {
    // One line over the photo.
    if (running) {
      snprintf(text, sizeof(text), "FEEDING - %s", sideWord(feedRunningRight()));
      setText(liveCaption, text);
      formatTimer(feedTotalSecs(runningLog), text, sizeof(text));
    } else if (last >= 0) {
      setText(liveCaption, "LAST FEED");
      char ago[16];
      formatAgo(logAgo(logs[last]), ago, sizeof(ago));
      snprintf(text, sizeof(text), "%s ago (%s)", ago,
               logs[last].kind == LOG_BOTTLE ? "BOTTLE" : (logs[last].flags & LF_RIGHT) ? "R" : "L");
    } else {
      setText(liveCaption, "FEEDING LOG");
      snprintf(text, sizeof(text), "tap to start");
    }
    setText(liveBig, text);
    return;
  }
  if (page != HOME && page != FEED && page != GLANCE) return;
  if (running) {
    snprintf(text, sizeof(text), "FEEDING - %s", sideWord(feedRunningRight()));
    setText(liveCaption, text);
    formatTimer(feedTotalSecs(runningLog), text, sizeof(text));
    setText(liveBig, text);
    sidesText(runningLog, text, sizeof(text), true);
    setText(liveInfo, text);
  } else if (last >= 0) {
    setText(liveCaption, "SINCE LAST FEED");
    formatAgo(logAgo(logs[last]), text, sizeof(text));
    setText(liveBig, text);
    lastFeedText(last, text, sizeof(text));
    setText(liveInfo, text);
  } else {
    setText(liveCaption, "NO FEED LOGGED YET");
    setText(liveBig, "--");
    setText(liveInfo, page == FEED ? "Tap LEFT or RIGHT to start" : "");
  }
  if (liveToday) { todayText(text, sizeof(text)); setText(liveToday, text); }
  if (page == FEED && undoShown != quickUndoAvailable()) requestPage(FEED);
}

void showHome() {
  wipe();
  if (nightMode) {
    // Night: no photo, warm text, the time since the last feed in large type.
    liveClock = label("", 106, 48, 200, textColor(), &bg_font_mid);
    liveCaption = label("", 76, 98, 260, accent());
    liveBig = label("", 26, 116, 360, textColor(), &bg_font_big);
    liveInfo = label("", 56, 178, 300, textColor(), &lv_font_montserrat_16);
    lv_obj_add_flag(liveBig, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(liveBig, onClick, LV_EVENT_CLICKED, (void*)(intptr_t)ACT_FEED);
    button("FEED", 66, 214, 136, 52, ACT_FEED, true, &bg_font_mid);
    button("NAP", 210, 214, 136, 52, ACT_NAP, false, &bg_font_mid);
    button("NOISE", 104, 276, 100, 46, ACT_NOISE, false);
    button("ALBUM", 208, 276, 100, 46, ACT_ALBUM, false);
    button("DAY", 94, 334, 104, 40, ACT_NIGHT, false);
    button("SET", 214, 334, 104, 40, ACT_SETTINGS, false);
    refreshLive();
    return;
  }
  createPhoto();
  panel(126, 30, 160, 44, ACT_HOME);
  liveClock = label("", 126, 37, 160, lv_color_hex(0xFFFFFF), &bg_font_mid);
  button("Wi-Fi", 52, 84, 72, 38, ACT_PHONE, false);
  button("LOOK", 128, 84, 72, 38, ACT_LOOK, false);
  button("NIGHT", 204, 84, 72, 38, ACT_NIGHT, false);
  button("SET", 280, 84, 72, 38, ACT_SETTINGS, false);
  panel(56, 204, 300, 68, ACT_FEED);
  liveCaption = label("", 56, 210, 300, accent());
  liveBig = label("", 56, 230, 300, lv_color_hex(0xFFFFFF), &bg_font_mid);
  button("FEED", 66, 282, 136, 50, ACT_FEED, true, &bg_font_mid);
  button("NAP", 210, 282, 136, 50, ACT_NAP, true, &bg_font_mid);
  button("NOISE", 104, 340, 100, 42, ACT_NOISE, false);
  button("ALBUM", 208, 340, 100, 42, ACT_ALBUM, false);
  refreshLive();
}
void showFeed() {
  wipe(); header("FEEDING");
  liveCaption = label("", 76, 86, 260, accent());
  liveBig = label("", 26, 104, 360, textColor(), &bg_font_big);
  liveInfo = label("", 56, 164, 300, textColor(), &lv_font_montserrat_16);
  const bool running = runningLog >= 0, right = feedRunningRight();
  button(running && !right ? "STOP" : "LEFT", 66, 194, 136, 64, ACT_FEED_LEFT, !running || !right, &bg_font_mid);
  button(running && right ? "STOP" : "RIGHT", 210, 194, 136, 64, ACT_FEED_RIGHT, !running || right, &bg_font_mid);
  button("BOTTLE", 80, 268, 122, 48, ACT_BOTTLE, false, &lv_font_montserrat_16);
  button("DIAPER", 210, 268, 122, 48, ACT_DIAPER, false, &lv_font_montserrat_16);
  undoShown = quickUndoAvailable();
  if (undoShown) {
    const int at = findLog(lastQuickLogId);
    label(logs[at].kind == LOG_BOTTLE ? "BOTTLE SAVED" : "DIAPER SAVED", 86, 324, 240, accent());
    button("UNDO", 146, 344, 120, 40, ACT_UNDO, true);
  } else {
    liveToday = label("", 76, 330, 260, textColor());
  }
  refreshLive();
}
// Shown while the phone lies face down (this screen faces up).
void showGlance() {
  wipe();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
  liveClock = label("", 26, 52, 360, textColor(), &bg_font_big);
  liveCaption = label("", 76, 132, 260, accent());
  liveBig = label("", 26, 150, 360, textColor(), &bg_font_big);
  liveInfo = label("", 56, 212, 300, textColor(), &lv_font_montserrat_16);
  liveToday = label("", 76, 246, 260, accent());
  lv_obj_t* area = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(area);
  lv_obj_set_size(area, W, H);
  lv_obj_add_flag(area, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(area, onClick, LV_EVENT_CLICKED, (void*)(intptr_t)ACT_HOME);
  refreshLive();
}

// ---- "look here": high-contrast patterns to catch the baby's eye for photos --
int lookPattern = 0;
bool lookInvert = false;
uint32_t lookFlipMs = 0, lookTouchedMs = 0;
void disc(int cx, int cy, int r, lv_color_t color) {
  lv_obj_t* o = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, r * 2, r * 2);
  lv_obj_set_pos(o, cx - r, cy - r);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, color, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}
void block(int x, int y, int w, int h, lv_color_t color, int radius = 0) {
  lv_obj_t* o = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_color(o, color, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}
void showLook() {
  wipe();
  const lv_color_t ink = lv_color_hex(lookInvert ? 0xFFFFFF : 0x000000);
  const lv_color_t paper = lv_color_hex(lookInvert ? 0x000000 : 0xFFFFFF);
  lv_obj_set_style_bg_color(lv_scr_act(), paper, 0);
  switch (lookPattern % 4) {
    case 0:  // bullseye
      for (int ring = 0; ring < 6; ++ring) disc(206, 206, 206 - ring * 34, ring % 2 ? paper : ink);
      break;
    case 1:  // checkerboard
      for (int row = 0; row < 6; ++row)
        for (int col = 0; col < 6; ++col)
          if ((row + col) % 2) block(col * 69, row * 69, 69, 69, ink);
      break;
    case 2:  // simple face
      disc(206, 206, 190, ink); disc(206, 206, 172, paper);
      disc(146, 168, 34, ink); disc(266, 168, 34, ink);
      block(136, 266, 140, 30, ink, 15);
      break;
    default:  // stripes
      for (int bar = 0; bar < 8; bar += 2) block(bar * 52, 0, 52, H, ink);
      break;
  }
  lv_obj_t* area = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(area);
  lv_obj_set_size(area, W, H);
  lv_obj_add_flag(area, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(area, onClick, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)ACT_LOOK_TAP);
  lv_obj_add_event_cb(area, onClick, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)ACT_HOME);
}

void showNap() {
  wipe(); header("NAP TRACKER");
  label("CURRENT NAP", 75, 100, 260, accent());
  elapsedLabel = label("00h 00m", 26, 120, 360, textColor(), &bg_font_big);
  label(clockAvailable ? "TOTAL TODAY" : "TOTAL THIS SESSION",
        56, 196, 300, accent());
  totalLabel = label("00h 00m", 56, 216, 300, textColor(), &bg_font_mid);
  napButtonLabel = button(napRunning ? "STOP NAP" : "START NAP",
                          104, 284, 204, 62, ACT_NAP_TOGGLE, true, &bg_font_mid);
}
void showNoise() {
  wipe(); header(BG_USE_HOME_ASSISTANT ? "ROOM SPEAKER" : "SOUND");
  button("<", 60, 92, 44, 46, ACT_TRACK_PREV, false);
  const char* title = trackCount ? tracks[trackIndex].title : "WHITE NOISE";
  const bool shortTitle = strlen(title) <= 8;
  label(title, 108, shortTitle ? 99 : 106, 196, textColor(), shortTitle ? &bg_font_mid : &lv_font_montserrat_16);
  button(">", 308, 92, 44, 46, ACT_TRACK_NEXT, false);
  char line[40];
  snprintf(line, sizeof(line), "%d / %d", trackIndex + 1, trackCount);
  label(line, 156, 142, 100, accent());
  noiseStatus = label(soundMessage.c_str(), 60, 160, 292);
  button(noisePlaying ? "STOP" : "PLAY", 128, 188, 156, 54, ACT_NOISE_TOGGLE, true, &bg_font_mid);
  button("-", 79, 252, 58, 46, ACT_VOLUME_DOWN, false);
  noiseVolume = label("VOL 3", 148, 266, 118);
  button("+", 276, 252, 58, 46, ACT_VOLUME_UP, false);
  button("15m", 89, 314, 70, 43, ACT_TIMER_15, noiseTimerMinutes == 15);
  button("30m", 170, 314, 70, 43, ACT_TIMER_30, noiseTimerMinutes == 30);
  button("60m", 251, 314, 70, 43, ACT_TIMER_60, noiseTimerMinutes == 60);
}
void updateAlbumStatus() {
  if (!albumStatus) return;
  char line[32];
  snprintf(line, sizeof(line), "%d / %d", photoCount ? photoIndex + 1 : 0, photoCount);
  lv_label_set_text(albumStatus, line);
}
void showAlbum() {
  wipe(); createPhoto();
  button("<", 60, 43, 48, 43, ACT_HOME, false);
  button("PREV", 53, 309, 92, 49, ACT_PREVIOUS, false);
  slideButtonLabel = button(autoplay ? "PAUSE" : "PLAY",
                            157, 309, 100, 49, ACT_SLIDESHOW);
  button("NEXT", 268, 309, 92, 49, ACT_NEXT, false);
  albumStatus = label("", 149, 49, 114);
  updateAlbumStatus();
}
void showSettings();
void showSettings2();
void showPage(Page next) {
  if (next == FRAME && !photoCount) next = HOME;
  page = next;
  if (next == HOME || next == FRAME) lastSlideMs = millis();
  if (next == HOME) { autoplay = false; showHome(); }
  if (next == FRAME) { autoplay = false; wipe(); createPhoto(ACT_HOME); }
  if (next == SETTINGS) showSettings();
  if (next == SETTINGS2) showSettings2();
  if (next == NAP) showNap();
  if (next == NOISE) showNoise();
  if (next == FEED) showFeed();
  if (next == GLANCE) showGlance();
  if (next == LOOK) showLook();
  if (next == ALBUM) {
    autoplay = photoCount > 1;
    lastSlideMs = millis();
    showAlbum();
  }
  if (next == PHONE) {
    wipe(); header("PHONE CONNECTION");
    label("1. JOIN WI-FI", 76, 112, 260, accent());
    label(apName.c_str(), 56, 140, 300);
    label("PASSWORD", 76, 179, 260, accent());
    label(apPassword.c_str(), 56, 205, 300);
    label("2. OPEN IN YOUR BROWSER", 56, 250, 300, accent());
    label("http://192.168.4.1", 56, 278, 300);
    label("No internet or microSD needed", 56, 326, 300);
  }
}

bool haPost(const String& domain, const String& action, const String& json) {
  if (WiFi.status() != WL_CONNECTED || !strlen(BG_HA_BEARER_TOKEN)) {
    soundMessage = "Configure Wi-Fi / Home Assistant";
    return false;
  }
  HTTPClient http;
  String url = String(BG_HA_BASE_URL) + "/api/services/" + domain + "/" + action;
  if (!http.begin(url)) { soundMessage="Home Assistant URL error"; return false; }
  http.setTimeout(2000);
  http.addHeader("Authorization", String("Bearer ") + BG_HA_BEARER_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int status = http.POST(json);
  http.end();
  if (status < 200 || status >= 300) {
    soundMessage = "HA error " + String(status);
    return false;
  }
  return true;
}
bool haScript(const char* entity) {
  return haPost("script", "turn_on", String("{\"entity_id\":\"") + entity + "\"}");
}
// ---- speaker volume and fades ---------------------------------------------------
// Volume 0-5 is about -22, -15, -10, -5, 0 dB (the 21-step table {6, 9, 12, 16, 21}
// of 2026-11-26). The library now runs with 255 steps (square curve) so a fade
// moves smoothly: a sound starts softly over 3 s, and in the last minute before
// its timer ends it fades out instead of stopping at once.
uint32_t noiseStartedMs = 0;
float appliedFadeDb = 0;
constexpr uint32_t FADE_IN_MS = 3000, FADE_OUT_MS = 60000;
constexpr float FADE_IN_FROM_DB = -30, FADE_OUT_TO_DB = -45;
int trackBeforeCry = -1;             // the sound to return to after cry help played its own
String cryTrackKey = "f:08-shush";   // what cry help plays
float levelDb(int level) {
  static const float table[] = {-200, -21.8f, -14.7f, -9.7f, -4.7f, 0};
  return table[constrain(level, 0, 5)];
}
float fadeDb(uint32_t now) {
  float db = 0;
  const uint32_t since = now - noiseStartedMs;
  if (since < FADE_IN_MS) db = FADE_IN_FROM_DB * (1 - (float)since / FADE_IN_MS);
  const int32_t left = (int32_t)(noiseEndsMs - now);
  if (left < (int32_t)FADE_OUT_MS)
    db = std::min(db, FADE_OUT_TO_DB * (1 - std::max<int32_t>(left, 0) / (float)FADE_OUT_MS));
  return db;
}
void setSpeakerDb(float db) {
  const float amplitude = db <= -120 ? 0 : powf(10, db / 20);
  audio.setVolume((uint8_t)constrain(lroundf(255 * sqrtf(amplitude)), 0L, 255L));
}
void applyVolume() {
  if (BG_USE_HOME_ASSISTANT) {
    const float f = BG_HA_VOLUME_CAP * noiseVolumeLevel / BG_ONBOARD_VOLUME_CAP;
    String data = String("{\"entity_id\":\"") + BG_HA_MEDIA_PLAYER +
                  "\",\"volume_level\":" + String(f, 3) + "}";
    haPost("media_player", "volume_set", data);
  } else {
    appliedFadeDb = noisePlaying ? fadeDb(millis()) : 0;
    setSpeakerDb(noiseVolumeLevel > 0 ? levelDb(noiseVolumeLevel) + appliedFadeDb : -200);
  }
}
void startNoise() {
  noiseStartedMs = millis();
  noiseEndsMs = noiseStartedMs + (uint32_t)noiseTimerMinutes * 60000UL;
  if (BG_USE_HOME_ASSISTANT) {
    if (!haScript(BG_HA_PLAY_SCRIPT)) return;
    applyVolume();
  } else {
    if (!trackCount) scanTracks();
    appliedFadeDb = FADE_IN_FROM_DB;  // start soft; BabyGrip_Update raises it
    setSpeakerDb(noiseVolumeLevel > 0 ? levelDb(noiseVolumeLevel) + appliedFadeDb : -200);
    if (!audio.connecttoFS(trackFs(trackIndex), trackPath(trackIndex).c_str())) {
      soundMessage="Audio decoder could not start";
      return;
    }
    audio.setFileLoop(true);
  }
  noisePlaying = true;
  soundMessage="Playing - stops automatically";
}
void stopNoise() {
  if (BG_USE_HOME_ASSISTANT) {
    haScript(BG_HA_STOP_SCRIPT);
  } else {
    audio.stopSong();
  }
  noisePlaying = false;
  soundMessage="Stopped";
  cryResumeAtMs.store(millis() + 20000);  // the cry detector would hear the end of it
  if (trackBeforeCry >= 0 && trackBeforeCry < trackCount) trackIndex = trackBeforeCry;
  trackBeforeCry = -1;
}
// Choosing another sound while one plays switches to it.
void selectTrack(int i) {
  if (i < 0 || i >= trackCount) return;
  const bool wasPlaying = noisePlaying;
  if (noisePlaying) stopNoise();
  trackBeforeCry = -1;
  trackIndex = i;
  prefs.putString("trackkey", tracks[i].key);
  soundMessage = "Sound selected";
  if (wasPlaying) startNoise();
}
// ---- cry help (detector in BabyGrip_Mic.h) ------------------------------------
void setCryHelp(int sensitivity) {
  crySensitivity.store(constrain(sensitivity, 0, 3));
  prefs.putUChar("cry", (uint8_t)crySensitivity.load());
  if (crySensitivity.load() > 0) { cryResumeAtMs.store(millis() + 2000); ensureMicTask(); }
}
int cryTrackIndex() { const int at = findTrack(cryTrackKey); return at >= 0 ? at : 0; }
// Crying was heard: log it and start the chosen sound with the current volume and timer.
void cryAction() {
  if (appendLog(LOG_CRY)) saveLog();
  Serial.println("[CRY] heard");
  if (noisePlaying || BG_USE_HOME_ASSISTANT || !trackCount) return;
  const int at = cryTrackIndex();
  if (at != trackIndex) { trackBeforeCry = trackIndex; trackIndex = at; }
  startNoise();  // the current volume and timer (volume 0 stays silent)
  if (!noisePlaying && trackBeforeCry >= 0) { trackIndex = trackBeforeCry; trackBeforeCry = -1; }
  if (noisePlaying) soundMessage = "Cry heard - playing";
  Serial.printf("[CRY] playing %s: %s\n", tracks[trackIndex].key, noisePlaying ? "OK" : "ERR");
  if (page == NOISE) requestPage(NOISE);
}

// ---- settings on the device (no phone needed) -----------------------------------
void headerBack(const char* title, Action back) {
  label(title, 76, 44, 260, accent());
  button("<", 64, 40, 46, 38, back, false);
}
void minutesText(int minutes, char* out, size_t size) {
  const int h = minutes / 60, m = minutes % 60;
  snprintf(out, size, "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
}
// Page 1: photo show, how often photos change, screen brightness.
void showSettings() {
  wipe(); headerBack("SETTINGS", ACT_HOME);
  label("PHOTO SHOW", 40, 103, 180, textColor(), &lv_font_montserrat_16);
  button(slideAuto ? "ON" : "OFF", 230, 92, 124, 44, ACT_SET_SLIDE, slideAuto, &lv_font_montserrat_16);
  label("EVERY", 40, 157, 100, textColor(), &lv_font_montserrat_16);
  button("5 s", 146, 146, 66, 44, ACT_SET_SLIDE_5, slideSeconds == 5, &lv_font_montserrat_16);
  button("10 s", 218, 146, 66, 44, ACT_SET_SLIDE_10, slideSeconds == 10, &lv_font_montserrat_16);
  button("30 s", 290, 146, 66, 44, ACT_SET_SLIDE_30, slideSeconds == 30, &lv_font_montserrat_16);
  label("LIGHT", 40, 211, 100, textColor(), &lv_font_montserrat_16);
  button("-", 146, 200, 56, 44, ACT_SET_LIGHT_DOWN, false, &bg_font_mid);
  char light[16];
  snprintf(light, sizeof(light), "%s %d", nightMode ? "NIGHT" : "DAY", nightMode ? nightLight : dayLight);
  label(light, 204, 213, 88, textColor());
  button("+", 294, 200, 56, 44, ACT_SET_LIGHT_UP, false, &bg_font_mid);
  button("NIGHT & CRY  >", 96, 262, 220, 46, ACT_SETTINGS2, false, &lv_font_montserrat_16);
  button("DONE", 146, 324, 120, 40, ACT_HOME, true);
}
// Page 2: automatic night mode and its times, cry help and the sound it plays.
void showSettings2() {
  wipe(); headerBack("NIGHT & CRY", ACT_SETTINGS);
  char text[32];
  label("AUTO NIGHT", 40, 101, 180, textColor(), &lv_font_montserrat_16);
  button(nightAuto ? "ON" : "OFF", 230, 90, 124, 42, ACT_SET_NIGHT_AUTO, nightAuto, &lv_font_montserrat_16);
  label("FROM", 40, 151, 72, textColor(), &lv_font_montserrat_16);
  button("-", 114, 140, 52, 42, ACT_SET_FROM_DOWN, false, &bg_font_mid);
  minutesText(nightStartMin, text, sizeof(text));
  label(text, 168, 151, 128, textColor(), &lv_font_montserrat_16);
  button("+", 298, 140, 52, 42, ACT_SET_FROM_UP, false, &bg_font_mid);
  label("TO", 40, 201, 72, textColor(), &lv_font_montserrat_16);
  button("-", 114, 190, 52, 42, ACT_SET_TO_DOWN, false, &bg_font_mid);
  minutesText(nightEndMin, text, sizeof(text));
  label(text, 168, 201, 128, textColor(), &lv_font_montserrat_16);
  button("+", 298, 190, 52, 42, ACT_SET_TO_UP, false, &bg_font_mid);
  const int cry = crySensitivity.load();
  label("CRY HELP", 40, 258, 132, textColor(), &lv_font_montserrat_16);
  button(CRY_NAMES[cry], 178, 246, 176, 44, ACT_SET_CRY, cry > 0, &lv_font_montserrat_16);
  label("SOUND", 56, 310, 96, textColor(), &lv_font_montserrat_16);
  button(trackCount ? tracks[cryTrackIndex()].title : "WHITE NOISE", 156, 298, 188, 42, ACT_SET_CRY_SOUND, false);
  const int last = lastLogIndex([](const LogRecord& r) { return r.kind == LOG_CRY; });
  if (!cry) snprintf(text, sizeof(text), "MIC OFF");
  else if (last < 0) snprintf(text, sizeof(text), "LISTENING");
  else {
    char at[16] = "";
    if (clockAvailable && !(logs[last].flags & (LF_PENDING | LF_UNKNOWN))) formatClockAt(logs[last].start, at, sizeof(at));
    snprintf(text, sizeof(text), "LAST CRY %s", at[0] ? at : "(time unknown)");
  }
  label(text, 96, 350, 220, accent());
}

// ---- night mode, look-here sound, orientation ----------------------------------
bool screenDark = false;
void applyBacklight() { Set_Backlight(screenDark ? 0 : (uint8_t)(nightMode ? nightLight : dayLight)); }
void setNight(bool on) {
  if (nightMode == on) return;
  nightMode = on;
  prefs.putBool("night", on);
  applyBacklight();
  requestPage(page);
}
bool inNightWindow() {
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  const int minute = local.tm_hour * 60 + local.tm_min;
  if (nightStartMin == nightEndMin) return false;
  return nightStartMin > nightEndMin ? (minute >= nightStartMin || minute < nightEndMin)
                                     : (minute >= nightStartMin && minute < nightEndMin);
}
// Switches only when the schedule crosses a boundary, so a manual change holds until then.
void checkNightSchedule() {
  if (!nightAuto || !clockAvailable) return;
  const int now = inNightWindow() ? 1 : 0;
  if (now != nightWindowSeen) { nightWindowSeen = now; setNight(now); }
}
void playLookSound() {
  if (noisePlaying || BG_USE_HOME_ASSISTANT || !soundStorageReady || !FFat.exists("/fx/look.mp3")) return;
  setSpeakerDb(levelDb(std::max(noiseVolumeLevel, 2)));
  if (audio.isRunning()) audio.stopSong();
  if (audio.connecttoFS(FFat, "/fx/look.mp3")) audio.setFileLoop(false);
}
// QMI8658 accelerometer (g). displayUpSign makes z positive when this screen
// faces up. The chip sits on the back of the board, so the default is -1; the
// phone page can measure it ("glance_calibrate": flip the phone within 5 s).
float displayUpSign = -1.0f;
uint32_t calibrateAtMs = 0;
String calibrationResult;
bool glanceAuto = true;
uint32_t faceUpSinceMs = 0, faceDownSinceMs = 0, notUpSinceMs = 0, lastMotionMs = 0;
float lastAx = 0, lastAy = 0, lastAz = 0;
Page glanceReturn = HOME;
void checkOrientation(uint32_t now) {
  if (calibrateAtMs && (int32_t)(now - calibrateAtMs) >= 0) {
    calibrateAtMs = 0;
    if (fabsf(Accel.z) > 0.7f) {
      displayUpSign = Accel.z > 0 ? 1.0f : -1.0f;
      prefs.putInt("upsign", (int)displayUpSign);
      calibrationResult = "ok";
    } else calibrationResult = "tilted";
  }
  const float x = Accel.x, y = Accel.y, z = Accel.z * displayUpSign;
  if (x != lastAx || y != lastAy || z != lastAz) {
    if (fabsf(x - lastAx) + fabsf(y - lastAy) + fabsf(z - lastAz) > 0.12f) lastMotionMs = now;
    lastAx = x; lastAy = y; lastAz = z;
  }
  const bool level = fabsf(x) < 0.35f && fabsf(y) < 0.35f;
  const bool up = level && z > 0.8f, down = level && z < -0.8f;
  faceUpSinceMs = up ? (faceUpSinceMs ? faceUpSinceMs : now) : 0;
  faceDownSinceMs = down ? (faceDownSinceMs ? faceDownSinceMs : now) : 0;
  notUpSinceMs = up ? 0 : (notUpSinceMs ? notUpSinceMs : now);
  bool dark = false;
  if (glanceAuto) {
    if (faceUpSinceMs && now - faceUpSinceMs > 2000 && (page == HOME || page == ALBUM || page == FRAME)) {
      glanceReturn = page;
      lastMotionMs = now;
      requestPage(GLANCE);
    }
    if (page == GLANCE && notUpSinceMs && now - notUpSinceMs > 500) requestPage(glanceReturn);
    // Dark while this screen faces down, or after a still, untouched minute of the glance screen.
    dark = (faceDownSinceMs && now - faceDownSinceMs > 5000) ||
           (page == GLANCE && now - lastMotionMs > 60000 && lv_disp_get_inactive_time(nullptr) > 60000);
  }
  if (dark != screenDark) { screenDark = dark; applyBacklight(); }
}
void runAction(Action a);
void onClick(lv_event_t* e) { runAction((Action)(intptr_t)lv_event_get_user_data(e)); }
// What a button does (also used by the USB command BGACT, for tests).
void runAction(Action a) {
  switch (a) {
    case ACT_HOME: requestPage(HOME); break;
    case ACT_FEED: requestPage(FEED); break;
    case ACT_FEED_LEFT:
    case ACT_FEED_RIGHT: feedSide(a == ACT_FEED_RIGHT); requestPage(FEED); break;
    case ACT_BOTTLE: logQuick(LOG_BOTTLE, 0); requestPage(FEED); break;
    case ACT_DIAPER: logQuick(LOG_DIAPER, 0); requestPage(FEED); break;
    case ACT_UNDO: if (quickUndoAvailable()) deleteLog(lastQuickLogId); requestPage(FEED); break;
    case ACT_NIGHT: setNight(!nightMode); requestPage(HOME); break;
    case ACT_TRACK_PREV:
    case ACT_TRACK_NEXT:
      if (trackCount) selectTrack((trackIndex + (a == ACT_TRACK_NEXT ? 1 : trackCount - 1)) % trackCount);
      requestPage(NOISE); break;
    case ACT_LOOK:
    case ACT_LOOK_TAP:
      lookPattern = a == ACT_LOOK ? 0 : lookPattern + 1;
      lookInvert = false;
      lookTouchedMs = lookFlipMs = millis();
      playLookSound();
      requestPage(LOOK); break;
    case ACT_NAP: requestPage(NAP); break;
    case ACT_NOISE: requestPage(NOISE); break;
    case ACT_ALBUM: requestPage(ALBUM); break;
    case ACT_NAP_TOGGLE: toggleNap(); requestPage(NAP); break;
    case ACT_NOISE_TOGGLE:
      if (noisePlaying) stopNoise(); else startNoise();
      requestPage(NOISE); break;
    case ACT_VOLUME_DOWN:
    case ACT_VOLUME_UP:
      noiseVolumeLevel += a == ACT_VOLUME_UP ? 1 : -1;
      noiseVolumeLevel = constrain(noiseVolumeLevel, 0, BG_ONBOARD_VOLUME_CAP);
      if (noisePlaying) applyVolume();
      requestPage(NOISE); break;
    case ACT_TIMER_15: noiseTimerMinutes=15; if (noisePlaying) noiseEndsMs=millis()+900000UL; requestPage(NOISE); break;
    case ACT_TIMER_30: noiseTimerMinutes=30; if (noisePlaying) noiseEndsMs=millis()+1800000UL; requestPage(NOISE); break;
    case ACT_TIMER_60: noiseTimerMinutes=60; if (noisePlaying) noiseEndsMs=millis()+3600000UL; requestPage(NOISE); break;
    case ACT_PREVIOUS:
    case ACT_NEXT:
      autoplay=false;
      if (photoCount) stepPhoto(a == ACT_NEXT ? 1 : -1);
      if (slideButtonLabel) lv_label_set_text(slideButtonLabel, "PLAY");
      updateAlbumStatus();
      break;
    case ACT_SLIDESHOW:
      autoplay=!autoplay;
      lastSlideMs=millis();
      if (slideButtonLabel) lv_label_set_text(slideButtonLabel, autoplay ? "PAUSE" : "PLAY");
      break;
    case ACT_PHONE: requestPage(PHONE); break;
    case ACT_SETTINGS: requestPage(SETTINGS); break;
    case ACT_SETTINGS2: requestPage(SETTINGS2); break;
    case ACT_SET_SLIDE:
      slideAuto = !slideAuto; prefs.putBool("slideauto", slideAuto);
      requestPage(SETTINGS); break;
    case ACT_SET_SLIDE_5:
    case ACT_SET_SLIDE_10:
    case ACT_SET_SLIDE_30:
      slideSeconds = a == ACT_SET_SLIDE_5 ? 5 : a == ACT_SET_SLIDE_10 ? 10 : 30;
      prefs.putUInt("slidesec", slideSeconds); lastSlideMs = millis();
      requestPage(SETTINGS); break;
    case ACT_SET_LIGHT_DOWN:
    case ACT_SET_LIGHT_UP: {
      const bool up = a == ACT_SET_LIGHT_UP;
      if (nightMode) {
        nightLight = constrain(up ? nightLight + (nightLight < 10 ? 1 : 5) : nightLight - (nightLight <= 10 ? 1 : 5), 1, 40);
        prefs.putUInt("nightlight", nightLight);
      } else {
        dayLight = constrain(dayLight + (up ? 10 : -10), 10, 100);
        prefs.putUInt("daylight", dayLight);
      }
      applyBacklight();
      requestPage(SETTINGS); break;
    }
    case ACT_SET_NIGHT_AUTO:
      nightAuto = !nightAuto; prefs.putBool("nightauto", nightAuto);
      nightWindowSeen = -1; checkNightSchedule();
      requestPage(SETTINGS2); break;
    case ACT_SET_FROM_DOWN:
    case ACT_SET_FROM_UP:
    case ACT_SET_TO_DOWN:
    case ACT_SET_TO_UP: {
      const bool from = a == ACT_SET_FROM_DOWN || a == ACT_SET_FROM_UP;
      const int step = (a == ACT_SET_FROM_UP || a == ACT_SET_TO_UP) ? 30 : -30;
      int& t = from ? nightStartMin : nightEndMin;
      const int other = from ? nightEndMin : nightStartMin;
      int next = (t + step + 1440) % 1440;
      if (next == other) next = (next + step + 1440) % 1440;  // start and end must differ
      t = next;
      prefs.putUInt(from ? "nightstart" : "nightend", (uint32_t)t);
      nightWindowSeen = -1; checkNightSchedule();
      requestPage(SETTINGS2); break;
    }
    case ACT_SET_CRY: setCryHelp((crySensitivity.load() + 1) % 4); requestPage(SETTINGS2); break;
    case ACT_SET_CRY_SOUND:
      if (trackCount) {
        cryTrackKey = tracks[(cryTrackIndex() + 1) % trackCount].key;
        prefs.putString("crytrack", cryTrackKey);
      }
      requestPage(SETTINGS2); break;
  }
}

// ---- phone API -------------------------------------------------------------
String quoted(const String& value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = (uint8_t)value[i];
    if (c == '\\' || c == '"') { result += '\\'; result += (char)c; }
    else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); result += escaped; }
    else result += (char)c;
  }
  return result + "\"";
}
String feedStateJson() {
  const int last = lastFeedIndex(), breast = lastBreastIndex(), diaper = lastDiaperIndex();
  const DaySummary s = todaySummary();
  String data = ",\"feed\":{\"rev\":" + String(logRev) + ",\"count\":" + String(logCount);
  data += String(",\"running\":") + (runningLog >= 0 ? "true" : "false");
  if (runningLog >= 0) {
    data += ",\"side\":" + String(feedRunningRight() ? "\"right\"" : "\"left\"");
    data += ",\"left\":" + String(feedSideSecs(runningLog, false)) + ",\"right\":" + String(feedSideSecs(runningLog, true));
    data += ",\"runningId\":" + String(logs[runningLog].id);
  }
  if (last >= 0) {
    data += ",\"lastId\":" + String(logs[last].id) + ",\"lastAgo\":" + String((long)logAgo(logs[last]));
    data += ",\"lastKind\":" + String(logs[last].kind);
  }
  if (breast >= 0) data += ",\"lastSide\":" + String((logs[breast].flags & LF_RIGHT) ? "\"right\"" : "\"left\"");
  if (diaper >= 0) data += ",\"diaperAgo\":" + String((long)logAgo(logs[diaper]));
  data += ",\"today\":{\"feeds\":" + String(s.feeds) + ",\"breast\":" + String(s.breast) + ",\"bottles\":" + String(s.bottles) +
          ",\"ml\":" + String(s.mlTotal) + ",\"diapers\":" + String(s.diapers) + ",\"wet\":" + String(s.wet) +
          ",\"dirty\":" + String(s.dirty) + ",\"cries\":" + String(s.cries) + "}";
  data += String(",\"saved\":") + (logStorageOk ? "true" : "false") + "}";
  return data;
}
String hhmm(int minutes) {
  char text[8];
  snprintf(text, sizeof(text), "%02d:%02d", minutes / 60, minutes % 60);
  return text;
}
String stateJson() {
  String data;
  if (!data.reserve(3600)) return "{\"error\":\"기기 상태를 준비할 메모리가 부족합니다. 잠시 후 다시 시도해 주세요.\"}";
  data = String("{\"napRunning\":") + (napRunning ? "true" : "false");
  data += ",\"napElapsed\":" + String(napElapsed());
  data += ",\"totalSecs\":" + String(finishedTodaySecs + napElapsed());
  data += String(",\"clockAvailable\":") + (clockAvailable ? "true" : "false");
  data += String(",\"noisePlaying\":") + (noisePlaying ? "true" : "false");
  data += String(",\"decoderRunning\":") + (audio.isRunning() ? "true" : "false");
  data += ",\"audioSeconds\":" + String(audio.getAudioCurrentTime());
  const int32_t remaining = (int32_t)(noiseEndsMs - millis());
  data += ",\"noiseRemaining\":" + String(noisePlaying && remaining > 0 ? (uint32_t)remaining / 1000 : 0);
  data += ",\"volume\":" + String(noiseVolumeLevel) + ",\"volumeCap\":" + String(BG_ONBOARD_VOLUME_CAP);
  data += ",\"timerMinutes\":" + String(noiseTimerMinutes);
  data += ",\"track\":" + quoted(trackCount ? tracks[trackIndex].key : "white");
  data += ",\"tracks\":[";
  for (int i = 0; i < trackCount; ++i) {
    if (i) data += ',';
    data += "{\"key\":" + quoted(tracks[i].key) + ",\"title\":" + quoted(tracks[i].title) +
            ",\"user\":" + (tracks[i].source == SRC_FILE ? "true" : "false") + "}";
  }
  data += "],\"maxTracks\":" + String(MAX_TRACKS);
  data += ",\"lullabyReady\":true";
  data += String(",\"customNoise\":") + (customNoise() ? "true" : "false");
  data += String(",\"night\":") + (nightMode ? "true" : "false") + ",\"nightAuto\":" + (nightAuto ? "true" : "false");
  data += ",\"nightStart\":\"" + hhmm(nightStartMin) + "\",\"nightEnd\":\"" + hhmm(nightEndMin) + "\"";
  data += ",\"dayLight\":" + String(dayLight) + ",\"nightLight\":" + String(nightLight);
  data += String(",\"glanceAuto\":") + (glanceAuto ? "true" : "false");
  data += String(",\"slideAuto\":") + (slideAuto ? "true" : "false") + ",\"slideSeconds\":" + String(slideSeconds);
  data += ",\"photoLoadMs\":" + String(photoLoadMs) + ",\"photoLoadMaxMs\":" + String(photoLoadMaxMs);
  {
    const int lastCry = lastLogIndex([](const LogRecord& r) { return r.kind == LOG_CRY; });
    data += ",\"cry\":" + String(crySensitivity.load()) + ",\"cryTrack\":" + quoted(cryTrackKey);
    data += ",\"cryLevel\":" + String((float)cryLevelDb, 1) + ",\"cryFloor\":" + String((float)cryFloorDb, 1);
    data += ",\"lastCryAgo\":" + String(lastCry >= 0 ? (long)logAgo(logs[lastCry]) : -1L);
    data += ",\"fadeDb\":" + String(noisePlaying ? fadeDb(millis()) : 0.0f, 1);
  }
  data += ",\"page\":" + String((int)page);
  data += ",\"upSign\":" + String((int)displayUpSign) + ",\"calibration\":" + quoted(calibrationResult);
  data += String(",\"rtc\":") + (rtcValid ? "true" : "false");
  data += ",\"epoch\":" + String(clockAvailable ? (uint32_t)time(nullptr) : 0);
  char accel[64];
  snprintf(accel, sizeof(accel), ",\"accel\":[%.2f,%.2f,%.2f]", Accel.x, Accel.y, Accel.z);
  data += accel;
  data += feedStateJson();
  data += ",\"soundMessage\":" + quoted(soundMessage);
  data += ",\"photoCount\":" + String(photoCount) + ",\"photoIndex\":" + String(photoIndex);
  data += ",\"photosRev\":" + String(photosRev);
  data += ",\"photoFormat\":\"jpeg\",\"photoNaming\":\"time-id\",\"photoLimit\":" + String(MAX_PHOTOS);
  data += ",\"photoMaxBytes\":" + String(MAX_JPEG_SIZE);
  data += ",\"storageAllocationUnit\":" + String(STORAGE_ALLOCATION_UNIT);
  data += ",\"storageReserve\":" + String(STORAGE_RESERVE);
  data += String(",\"autoplay\":") + (autoplay ? "true" : "false");
  data += String(",\"storageReady\":") + (storageReady ? "true" : "false");
  data += String(",\"soundStorageReady\":") + (soundStorageReady ? "true" : "false");
  data += ",\"volumes\":" + String(readyVolumes());
  data += ",\"storageTotal\":" + String(storageTotal());
  data += ",\"storageUsed\":" + String(storageUsed());
  data += ",\"storageFree\":" + String(photoFreeBytes());
  data += ",\"ssid\":" + quoted(apName) + "}";
  return data;
}
void jsonReply(int status, const String& data) {
  phone.sendHeader("Cache-Control", "no-store");
  phone.sendHeader("X-Content-Type-Options", "nosniff");
  phone.send(status, "application/json; charset=utf-8", data);
}
void apiError(int status, const String& message, const char* code = "error") {
  jsonReply(status, "{\"error\":" + quoted(message) + ",\"code\":" + quoted(code) + "}");
}
bool localHost() {
  const String host = phone.hostHeader();
  const String ap = WiFi.softAPIP().toString();
  const String station = WiFi.localIP().toString();
  return host == ap || host == ap + ":80" ||
    (WiFi.status() == WL_CONNECTED && (host == station || host == station + ":80"));
}
bool authorized() {
  return localHost() && csrfToken.length() == 32 && phone.header("X-BabyGrip-Token") == csrfToken;
}
bool requireAuth() {
  if (authorized()) return true;
  apiError(403, "연결 확인에 실패했습니다. 기기 주소에서 화면을 다시 열어 주세요.", "auth");
  return false;
}
bool integerArg(const char* key, long minimum, long maximum, long& value) {
  const String text = phone.arg(key);
  if (!text.length() || text.length() > 12) return false;
  char* end = nullptr;
  value = strtol(text.c_str(), &end, 10);
  return end != text.c_str() && *end == '\0' && value >= minimum && value <= maximum;
}
void showFirstPhotoIfNeeded() {
  if (photoCount == 1) { loadPhoto(0); if (page == HOME || page == PHONE) requestPage(HOME); }
}
void afterPhotosRemoved() {
  autoplay = false;
  if (photoCount) loadPhoto(photoIndex);
  if (page == HOME || page == ALBUM) requestPage(page == ALBUM && photoCount ? ALBUM : HOME);
}

// Earlier builds kept copies of the built-in sounds in ffat. They now play from
// the firmware, so byte-identical copies are removed to free photo space.
bool sameAsBuiltIn(const char* path, const uint8_t* bytes, size_t length) {
  File f = FFat.open(path, FILE_READ);
  if (!f) return false;
  if (f.size() != length) { f.close(); return false; }
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  bool same = buffer != nullptr;
  size_t offset = 0;
  while (same && offset < length) {
    const size_t n = f.read(buffer, std::min((size_t)4096, length - offset));
    if (!n || memcmp(buffer, bytes + offset, n) != 0) same = false;
    offset += n;
  }
  free(buffer);
  f.close();
  return same && offset == length;
}
void reclaimBuiltInCopies() {
  if (!soundStorageReady) return;
  if (sameAsBuiltIn("/lullaby.mp3", BG_SAMPLE_LULLABY, sizeof(BG_SAMPLE_LULLABY)) && FFat.remove("/lullaby.mp3"))
    Serial.println("[BabyGrip] Removed duplicate lullaby copy (plays from firmware)");
  if (sameAsBuiltIn("/white_noise.mp3", BG_SAMPLE_NOISE, sizeof(BG_SAMPLE_NOISE)) && FFat.remove("/white_noise.mp3"))
    Serial.println("[BabyGrip] Removed duplicate white-noise copy (plays from firmware)");
}
void mountVolume(int v) {
  Volume& vol = volumes[v];
  vol.ready = vol.fs->begin(false, vol.mount, vol.maxFiles, vol.label);
  if (!vol.ready && !prefs.getBool(vol.pref, false)) {
    // Initialize a volume only on its first use. Never reformat afterwards.
    Serial.printf("[BabyGrip] Initializing internal volume %s (first use)\n", vol.label);
    if (vol.fs->format(FFAT_WIPE_QUICK, (char*)vol.label))
      vol.ready = vol.fs->begin(false, vol.mount, vol.maxFiles, vol.label);
  }
  if (!vol.ready) {
    Serial.printf("[BabyGrip] Volume %s mount failed; preserving data\n", vol.label);
    return;
  }
  prefs.putBool(vol.pref, true);
  if (!vol.fs->exists("/photos")) vol.fs->mkdir("/photos");
  vol.fs->remove("/photos/upload.part");
  Serial.printf("[BabyGrip] Volume %s: %u / %u bytes used\n", vol.label,
                (unsigned)vol.fs->usedBytes(), (unsigned)vol.fs->totalBytes());
}
void initStorage() {
  for (int v = 0; v < VOLUME_COUNT; ++v) mountVolume(v);
  soundStorageReady = volumes[0].ready;
  storageReady = readyVolumes() > 0;
  if (soundStorageReady) {
    bool soundOk = true;
    if (FFat.exists("/white_noise.bak")) {
      if (FFat.exists("/white_noise.mp3")) FFat.remove("/white_noise.bak");
      else if (!FFat.rename("/white_noise.bak", "/white_noise.mp3")) {
        Serial.println("[BabyGrip] Sound recovery failed; preserving backup");
        soundOk = false;
      }
    }
    FFat.remove("/photo-upload.part");
    FFat.remove("/noise-upload.part");
    FFat.remove("/sound-replace.bak");
    FFat.remove("/sounds/serial.part");
    if (!FFat.exists("/sounds")) FFat.mkdir("/sounds");
    if (!FFat.exists("/fx")) FFat.mkdir("/fx");
    if (soundOk) reclaimBuiltInCopies();
  }
}
void handleControl() {
  if (!requireAuth()) return;
  const String action = phone.arg("action");
  long value = 0;
  if (action == "nap_start" || action == "nap_stop") {
    const bool wanted = action == "nap_start";
    if (napRunning != wanted) toggleNap();
    if (page == NAP) requestPage(NAP);
  } else if (action == "noise_start") {
    if (!noisePlaying) startNoise();
    if (!noisePlaying) { apiError(409, soundMessage, "sound"); return; }
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "noise_stop") {
    if (noisePlaying) stopNoise();
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "noise_default") {
    if (noisePlaying) stopNoise();
    if (customNoise() && !FFat.remove("/white_noise.mp3")) { apiError(500, "올린 음원을 지우지 못했습니다.", "storage"); return; }
    scanTracks();
    selectTrack(findTrack("white"));
    soundMessage = "White noise ready";
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "sound_delete") {
    const int at = findTrack(phone.arg("value"));
    if (at < 0 || tracks[at].source != SRC_FILE) { apiError(404, "지울 음원을 찾지 못했습니다.", "missing"); return; }
    if (noisePlaying && at == trackIndex) stopNoise();
    if (!FFat.remove(trackPath(at))) { apiError(500, "음원을 지우지 못했습니다.", "storage"); return; }
    const String keep = at == trackIndex ? String("white") : String(tracks[trackIndex].key);
    scanTracks();
    const int next = findTrack(keep);
    trackIndex = next >= 0 ? next : 0;
    prefs.putString("trackkey", tracks[trackIndex].key);
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "night") {
    if (!integerArg("value", 0, 1, value)) { apiError(400, "잘못된 밤 모드 설정입니다."); return; }
    setNight(value == 1);
  } else if (action == "night_auto") {
    long on = 0;
    const String start = phone.arg("start"), end = phone.arg("end");
    auto minutes = [](const String& t) -> int {
      if (t.length() != 5 || t[2] != ':' || !isdigit(t[0]) || !isdigit(t[1]) || !isdigit(t[3]) || !isdigit(t[4])) return -1;
      const int h = t.substring(0, 2).toInt(), m = t.substring(3).toInt();
      return h < 24 && m < 60 ? h * 60 + m : -1;
    };
    const int s = minutes(start), e = minutes(end);
    if (!integerArg("value", 0, 1, on) || s < 0 || e < 0 || s == e) { apiError(400, "밤 모드 시간을 확인해 주세요."); return; }
    nightAuto = on == 1; nightStartMin = s; nightEndMin = e; nightWindowSeen = -1;
    prefs.putBool("nightauto", nightAuto); prefs.putUInt("nightstart", s); prefs.putUInt("nightend", e);
    checkNightSchedule();
  } else if (action == "brightness") {
    long day = 0, night = 0;
    if (!integerArg("day", 10, 100, day) || !integerArg("night", 1, 40, night)) { apiError(400, "밝기 범위를 확인해 주세요."); return; }
    dayLight = (int)day; nightLight = (int)night;
    prefs.putUInt("daylight", dayLight); prefs.putUInt("nightlight", nightLight);
    applyBacklight();
  } else if (action == "glance") {
    if (!integerArg("value", 0, 1, value)) { apiError(400, "잘못된 설정입니다."); return; }
    glanceAuto = value == 1; prefs.putBool("glance", glanceAuto);
    if (!glanceAuto && page == GLANCE) requestPage(HOME);
  } else if (action == "slide_auto") {
    if (!integerArg("value", 0, 1, value)) { apiError(400, "잘못된 설정입니다."); return; }
    slideAuto = value == 1; prefs.putBool("slideauto", slideAuto);
    if (!slideAuto && page == FRAME) requestPage(HOME);
  } else if (action == "slide_seconds") {
    if (!integerArg("value", 3, 300, value)) { apiError(400, "넘기는 간격은 3~300초입니다."); return; }
    slideSeconds = (uint32_t)value; prefs.putUInt("slidesec", slideSeconds);
    lastSlideMs = millis();
  } else if (action == "cry") {
    if (!integerArg("value", 0, 3, value)) { apiError(400, "울음 감지 설정을 확인해 주세요."); return; }
    setCryHelp((int)value);
    if (page == SETTINGS2) requestPage(SETTINGS2);
  } else if (action == "cry_track") {
    const String key = phone.arg("value");
    if (findTrack(key) < 0) { apiError(400, "그 소리를 찾지 못했습니다. 새로고침해 주세요."); return; }
    cryTrackKey = key; prefs.putString("crytrack", cryTrackKey);
    if (page == SETTINGS2) requestPage(SETTINGS2);
  } else if (action == "glance_calibrate") {
    calibrateAtMs = millis() + 5000;
    calibrationResult = "waiting";
  } else if (action == "look") {
    lookPattern = 0; lookInvert = false; lookTouchedMs = lookFlipMs = millis();
    playLookSound();
    requestPage(LOOK);
  } else if (action == "volume") {
    if (!integerArg("value", 0, BG_ONBOARD_VOLUME_CAP, value)) { apiError(400, "음량 범위를 확인해 주세요."); return; }
    noiseVolumeLevel = (int)value;
    applyVolume();
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "track") {
    const int at = findTrack(phone.arg("value"));
    if (at < 0) { apiError(400, "음원을 선택해 주세요."); return; }
    if (noisePlaying) stopNoise();
    selectTrack(at);
    soundMessage = tracks[at].source == SRC_ROM_LULLABY ? "Lullaby ready" : "Sound selected";
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "timer") {
    if (!integerArg("value", 15, 60, value) || (value != 15 && value != 30 && value != 60)) { apiError(400, "타이머는 15/30/60분 중 선택해 주세요."); return; }
    noiseTimerMinutes = (int)value;
    if (noisePlaying) noiseEndsMs = millis() + (uint32_t)value * 60000UL;
    if (page == NOISE) requestPage(NOISE);
  } else if (action == "previous" || action == "next") {
    if (!photoCount) { apiError(409, "먼저 사진을 올려 주세요."); return; }
    autoplay = false;
    if (!stepPhoto(action == "next" ? 1 : -1)) { apiError(500, "사진을 읽지 못했습니다."); return; }
    requestPage(HOME);
  } else if (action == "slideshow") {
    if (!integerArg("value", 0, 1, value)) { apiError(400, "잘못된 앨범 설정입니다."); return; }
    if (!photoCount) { apiError(409, "먼저 사진을 올려 주세요."); return; }
    if (value) requestPage(ALBUM);
    else { autoplay = false; if (page == ALBUM) requestPage(HOME); }
  } else { apiError(400, "알 수 없는 명령입니다."); return; }
  jsonReply(200, stateJson());
}
// Phone page or USB (BGTIME): UTC epoch and minutes east of UTC.
bool setClock(uint64_t epoch, long offset) {
  applyTimezone(offset);
  if (prefs.getInt("tzoff", 100000) != offset) prefs.putInt("tzoff", (int)offset);
  const struct timeval tv = {(time_t)epoch, 0};
  if (settimeofday(&tv, nullptr) != 0) return false;
  saveClockToRtc();
  checkCalendar();
  if (runningLog >= 0) saveRunningSegment();
  if (napRunning) {
    prefs.putULong64("start", epoch - napElapsed());
    prefs.putBool("active", true);
  }
  if (page == NAP || page == HOME || page == FEED) requestPage(page);
  return true;
}
void handleTime() {
  if (!requireAuth()) return;
  const String text = phone.arg("epoch");
  char* end = nullptr;
  const uint64_t epoch = strtoull(text.c_str(), &end, 10);
  long offset = 0;
  if (text.length() != 10 || end == text.c_str() || *end || epoch < 1735689600ULL || epoch > 4102444800ULL ||
      !integerArg("offset", -840, 840, offset)) { apiError(400, "휴대폰 시간 정보를 확인해 주세요."); return; }
  if (!setClock(epoch, offset)) { apiError(500, "기기 시간을 설정하지 못했습니다."); return; }
  jsonReply(200, "{\"ok\":true}");
}
void handlePhotoList() {
  if (!localHost()) { apiError(400, "잘못된 기기 주소입니다."); return; }
  phone.sendHeader("Cache-Control", "no-store");
  phone.sendHeader("X-Content-Type-Options", "nosniff");
  phone.setContentLength(CONTENT_LENGTH_UNKNOWN);
  phone.send(200, "application/json; charset=utf-8", "");
  String chunk;
  chunk.reserve(1200);
  chunk = "{\"rev\":" + String(photosRev) + ",\"count\":" + String(photoCount) + ",\"photos\":[";
  for (int i = 0; i < photoCount; ++i) {
    if (i) chunk += ',';
    chunk += '"';
    chunk += photos[i].name;  // names contain only [A-Za-z0-9_-.]
    chunk += '"';
    if (chunk.length() > 1000) { phone.sendContent(chunk); chunk = ""; }
  }
  chunk += "]}";
  phone.sendContent(chunk);
  phone.sendContent("");
}
// Feeding log for the phone: newest first, chunked.
void handleFeedList() {
  if (!localHost()) { apiError(400, "잘못된 기기 주소입니다."); return; }
  phone.sendHeader("Cache-Control", "no-store");
  phone.sendHeader("X-Content-Type-Options", "nosniff");
  phone.setContentLength(CONTENT_LENGTH_UNKNOWN);
  phone.send(200, "application/json; charset=utf-8", "");
  String chunk;
  chunk.reserve(1400);
  chunk = "{\"rev\":" + String(logRev) + ",\"clock\":" + (clockAvailable ? "true" : "false") +
          ",\"now\":" + String(clockAvailable ? (uint32_t)time(nullptr) : 0) + ",\"items\":[";
  bool first = true;
  for (int i = logCount - 1; i >= 0; --i) {
    const LogRecord& r = logs[i];
    char item[160];
    snprintf(item, sizeof(item), "%s{\"id\":%lu,\"kind\":%u,\"ago\":%ld,\"left\":%lu,\"right\":%lu,\"ml\":%u,\"flags\":%u}",
             first ? "" : ",", (unsigned long)r.id, r.kind, (long)logAgo(r), (unsigned long)feedSideSecs(i, false),
             (unsigned long)feedSideSecs(i, true), r.ml, r.flags);
    first = false;
    chunk += item;
    if (chunk.length() > 1200) { phone.sendContent(chunk); chunk = ""; audio.loop(); }
  }
  chunk += "]}";
  phone.sendContent(chunk);
  phone.sendContent("");
}
void handleFeedAction() {
  if (!requireAuth()) return;
  const String action = phone.arg("action");
  long value = 0, id = 0;
  if (!logs) { apiError(500, "기록을 준비할 메모리가 없습니다.", "memory"); return; }
  if (action == "left" || action == "right") feedSide(action == "right");
  else if (action == "stop") feedStop();
  else if (action == "bottle") {
    if (!integerArg("value", 0, 400, value)) { apiError(400, "분유·젖병 양(ml)을 확인해 주세요."); return; }
    logQuick(LOG_BOTTLE, (uint16_t)value);
  } else if (action == "diaper") {
    const String kind = phone.arg("value");
    const uint8_t k = kind == "wet" ? LOG_WET : kind == "dirty" ? LOG_DIRTY : kind == "both" ? LOG_BOTH : kind == "any" ? LOG_DIAPER : 0;
    if (!k) { apiError(400, "기저귀 종류를 선택해 주세요."); return; }
    logQuick(k, 0);
  } else if (action == "delete") {
    if (!integerArg("id", 1, 0x7fffffff, id) || !deleteLog((uint32_t)id)) { apiError(404, "기록을 찾지 못했습니다. 목록을 새로고침해 주세요.", "missing"); return; }
  } else if (action == "update") {
    const int at = integerArg("id", 1, 0x7fffffff, id) ? findLog((uint32_t)id) : -1;
    if (at < 0) { apiError(404, "기록을 찾지 못했습니다. 목록을 새로고침해 주세요.", "missing"); return; }
    LogRecord& r = logs[at];
    if (phone.hasArg("ml")) {
      if (!integerArg("ml", 0, 400, value) || r.kind != LOG_BOTTLE) { apiError(400, "분유·젖병 양(ml)을 확인해 주세요."); return; }
      r.ml = (uint16_t)value;
    }
    if (phone.hasArg("diaper")) {
      const String kind = phone.arg("diaper");
      const uint8_t k = kind == "wet" ? LOG_WET : kind == "dirty" ? LOG_DIRTY : kind == "both" ? LOG_BOTH : kind == "any" ? LOG_DIAPER : 0;
      if (!k || !logIsDiaper(r)) { apiError(400, "기저귀 종류를 선택해 주세요."); return; }
      r.kind = k;
    }
    if (phone.hasArg("minutesAgo")) {
      // Correct the start time (e.g. a feed logged late). Only finished records.
      if (!integerArg("minutesAgo", 0, 24 * 60, value) || !clockAvailable || at == runningLog) { apiError(400, "시간을 확인해 주세요."); return; }
      r.start = (uint32_t)time(nullptr) - (uint32_t)value * 60;
      r.flags &= ~(LF_PENDING | LF_UNKNOWN);
      sortLogs();
    }
    saveLog();
  } else { apiError(400, "알 수 없는 기록 명령입니다."); return; }
  if (page == FEED || page == HOME || page == GLANCE) requestPage(page);
  jsonReply(200, stateJson());
}
fs::FS& uploadFs() { return uploadIsNoise ? (fs::FS&)FFat : (fs::FS&)*volumes[uploadVolume].fs; }
bool uploadFsReady() { return uploadIsNoise ? soundStorageReady : volumes[uploadVolume].ready; }
void discardUpload() {
  if (uploadFile) uploadFile.close();
  if (uploadPart.length() && uploadFsReady()) uploadFs().remove(uploadPart);
}
void rejectUpload(const String& message, const char* code) {
  if (!uploadError.length()) { uploadError = message; uploadErrorCode = code; }
  discardUpload();
}
// Errors are recorded and the rest of the request is drained, so the phone
// receives a readable JSON answer instead of a dropped connection.
void handleUpload(bool noise) {
  // The core dispatches raw POSTs to this callback too; upload() is null then.
  if (!phone.header("Content-Type").startsWith("multipart/form-data;")) {
    phone.client().stop(); return;
  }
  HTTPUpload& item = phone.upload();
  if (item.status == UPLOAD_FILE_START) {
    // A second file part in the same request follows a finished first part.
    if (uploadStarted && uploadEnded) { rejectUpload("파일은 한 번에 하나씩 올려 주세요.", "invalid"); phone.client().stop(); return; }
    if (uploadStarted) discardUpload();  // left over from an interrupted request
    uploadStarted = true; uploadEnded = false; uploadDuplicate = false;
    uploadedBytes = 0; uploadExpected = 0; uploadError = ""; uploadErrorCode = "";
    uploadIsNoise = noise; uploadIsJpeg = false; uploadStartedMs = millis();
    uploadPart = ""; uploadTarget = ""; uploadVolume = 0;
    const int declared = phone.clientContentLength();
    const size_t requestLimit = (noise ? MAX_NOISE_SIZE : MAX_JPEG_SIZE) + MULTIPART_ALLOWANCE;
    if (declared <= 0 || (size_t)declared > requestLimit) {
      rejectUpload("파일이 너무 큽니다.", "too_large"); phone.client().stop(); return;
    }
    if (!authorized()) { rejectUpload("연결 확인에 실패했습니다. 기기 주소에서 화면을 다시 열어 주세요.", "auth"); return; }
    if (noise) {
      // Each MP3 becomes its own sound: /sounds/<name>.mp3 (same name replaces it).
      const String base = phone.arg("name");
      if (!soundStorageReady) { rejectUpload("내장 저장소를 사용할 수 없습니다.", "storage"); return; }
      if (!soundBaseName(base)) { rejectUpload("음원 이름은 영문 소문자·숫자·-·_ 20자 이하로 정해 주세요.", "name"); return; }
      const String key = "f:" + base;
      if (findTrack(key) < 0 && trackCount >= MAX_TRACKS) { rejectUpload("음원은 최대 22개까지 저장할 수 있습니다. 하나를 지운 뒤 다시 시도해 주세요.", "limit"); return; }
      if (FFat.freeBytes() < allocationFor((size_t)declared) + STORAGE_RESERVE) {
        rejectUpload("음원을 저장할 공간이 부족합니다. 사진을 일부 지운 뒤 다시 시도해 주세요.", "full"); return;
      }
      if (noisePlaying) stopNoise();
      uploadSoundKey = key;
      uploadTarget = "/sounds/" + base + ".mp3";
      uploadPart = "/noise-upload.part";
    } else {
      long expected = 0;
      const String filename = item.filename;
      if (!storageReady || !photos) { rejectUpload("내장 저장소를 사용할 수 없습니다.", "storage"); return; }
      if (!uploadPhotoName(filename.c_str())) { rejectUpload("휴대폰 화면을 새로고침한 뒤 사진을 다시 선택해 주세요.", "name"); return; }
      if (!integerArg("size", 128, MAX_JPEG_SIZE, expected)) { rejectUpload("사진 크기 정보를 확인할 수 없습니다. 화면을 새로고침해 주세요.", "invalid"); return; }
      uploadExpected = (size_t)expected;
      uploadIsJpeg = true;
      uploadTarget = photoPath(filename.c_str());
      if (findPhoto(filename.c_str()) >= 0) { uploadDuplicate = true; return; }
      if (photoCount >= MAX_PHOTOS) { rejectUpload("앨범 사진 수 한도에 도달했습니다. 기존 사진을 지워 주세요.", "limit"); return; }
      if (!pixels || !photoStaging) { rejectUpload("사진을 준비할 메모리가 부족합니다. 기기를 다시 시작해 주세요.", "memory"); return; }
      const int volume = pickPhotoVolume(uploadExpected);
      if (volume < 0) { rejectUpload("기기 저장 공간이 가득 찼습니다.", "full"); return; }
      uploadVolume = volume;
      uploadPart = "/photos/upload.part";
    }
    uploadFile = uploadFs().open(uploadPart, FILE_WRITE);
    if (!uploadFile) rejectUpload("임시 파일을 만들지 못했습니다.", "storage");
  } else if (item.status == UPLOAD_FILE_WRITE && uploadStarted) {
    audio.loop();  // keep a playing sound fed while the request streams in
    if (uploadError.length() || uploadDuplicate) return;
    const size_t limit = noise ? MAX_NOISE_SIZE : uploadExpected;
    if (millis() - uploadStartedMs > 45000 || uploadedBytes > limit || item.currentSize > limit - uploadedBytes)
      rejectUpload("파일 크기·전송 시간을 확인해 주세요.", "invalid");
    else if (noise && uploadedBytes == 0 && (item.currentSize < 3 ||
             !(memcmp(item.buf, "ID3", 3) == 0 || (item.buf[0] == 0xff && (item.buf[1] & 0xe0) == 0xe0))))
      rejectUpload("MP3 형식의 파일을 선택해 주세요.", "invalid");
    else if (!noise && uploadedBytes == 0 && (item.currentSize < 2 || item.buf[0] != 0xff || item.buf[1] != 0xd8))
      rejectUpload("JPEG 사진이 아닙니다.", "invalid");
    else if (!uploadFile || uploadFile.write(item.buf, item.currentSize) != item.currentSize)
      rejectUpload("저장 중 오류가 발생했습니다.", "storage");
    else uploadedBytes += item.currentSize;
  } else if (item.status == UPLOAD_FILE_END && uploadStarted) {
    uploadEnded = true;
    if (uploadFile) { uploadFile.flush(); uploadFile.close(); }
  } else if (item.status == UPLOAD_FILE_ABORTED) {
    discardUpload(); uploadStarted = false; uploadEnded = false;
  }
}
String uploadedPhotoReply(const String& name, bool duplicate) {
  return "{\"ok\":true,\"duplicate\":" + String(duplicate ? "true" : "false") +
         ",\"name\":" + quoted(name) + ",\"photoCount\":" + String(photoCount) +
         ",\"storageFree\":" + String(photoFreeBytes()) + "}";
}
void finishUpload() {
  const bool started = uploadStarted, ended = uploadEnded;
  uploadStarted = false; uploadEnded = false;
  if ((!started || !ended) && !uploadError.length()) { uploadError = "파일을 하나 선택해 다시 올려 주세요."; uploadErrorCode = "invalid"; }
  if (!uploadError.length() && !authorized()) { uploadError = "연결 확인에 실패했습니다. 기기 주소에서 화면을 다시 열어 주세요."; uploadErrorCode = "auth"; }
  const String name = uploadTarget.substring(uploadTarget.lastIndexOf('/') + 1);
  if (!uploadError.length() && uploadDuplicate) { jsonReply(200, uploadedPhotoReply(name, true)); return; }
  if (!uploadError.length()) {
    File check = uploadFs().open(uploadPart, FILE_READ);
    if (!check || check.size() != uploadedBytes || (uploadIsNoise ? uploadedBytes < 128 : uploadedBytes != uploadExpected)) {
      uploadError = "파일이 완전히 전송되지 않았습니다. 다시 시도해 주세요."; uploadErrorCode = "incomplete";
    } else if (!uploadIsNoise && !decodePhoto(check, true, photoStaging)) {
      uploadError = "사진을 열지 못했습니다. 다른 사진을 선택해 주세요."; uploadErrorCode = "invalid";
    }
    if (check) check.close();
  }
  if (!uploadError.length() && !uploadIsNoise && uploadFs().exists(uploadTarget)) {
    uploadFs().remove(uploadPart);  // the same photo was stored meanwhile
    jsonReply(200, uploadedPhotoReply(name, true));
    return;
  }
  bool movedOld = false;
  if (!uploadError.length() && uploadIsNoise && FFat.exists(uploadTarget)) {
    FFat.remove("/sound-replace.bak");
    movedOld = FFat.rename(uploadTarget, "/sound-replace.bak");
    if (!movedOld) { uploadError = "기존 음원을 보관하지 못했습니다."; uploadErrorCode = "storage"; }
  }
  if (!uploadError.length() && !uploadFs().rename(uploadPart, uploadTarget)) {
    uploadError = "새 파일을 저장하지 못했습니다."; uploadErrorCode = "storage";
    if (movedOld && !FFat.rename("/sound-replace.bak", uploadTarget)) uploadError = "기존 음원을 되돌리지 못했습니다. 다시 올려 주세요.";
  }
  if (uploadError.length()) {
    discardUpload();
    const int status = uploadErrorCode == "auth" ? 403 : uploadErrorCode == "full" ? 507 :
                       uploadErrorCode == "limit" ? 409 : uploadErrorCode == "too_large" ? 413 : 400;
    apiError(status, uploadError, uploadErrorCode.c_str());
    return;
  }
  if (uploadIsNoise) {
    if (movedOld) FFat.remove("/sound-replace.bak");
    scanTracks();
    selectTrack(findTrack(uploadSoundKey));
    soundMessage = "Ready - tap PLAY";
    if (page == NOISE) requestPage(NOISE);
    jsonReply(200, "{\"ok\":true,\"key\":" + quoted(uploadSoundKey) + "}");
    return;
  }
  insertPhoto(name.c_str(), (uint8_t)uploadVolume);
  showFirstPhotoIfNeeded();
  jsonReply(200, uploadedPhotoReply(name, false));
}
void handleDelete() {
  if (!requireAuth()) return;
  const String name = phone.arg("name");
  const int at = safePhotoName(name.c_str()) ? findPhoto(name.c_str()) : -1;
  if (at < 0) { apiError(404, "사진을 찾지 못했습니다. 목록을 새로고침해 주세요.", "missing"); return; }
  if (!volumes[photos[at].volume].fs->remove(photoPath(photos[at].name))) { apiError(500, "사진을 지우지 못했습니다.", "storage"); return; }
  const bool shown = at == photoIndex;
  erasePhotoAt(at);
  if (shown || !photoCount) afterPhotosRemoved();
  jsonReply(200, "{\"ok\":true,\"photoCount\":" + String(photoCount) + "}");
}
void handleDeleteAll() {
  if (!requireAuth()) return;
  if (phone.arg("confirm") != "all") { apiError(400, "전체 삭제 확인이 필요합니다.", "confirm"); return; }
  unsigned removed = 0, failed = 0;
  for (int i = photoCount - 1; i >= 0; --i) {
    if (volumes[photos[i].volume].fs->remove(photoPath(photos[i].name))) ++removed; else ++failed;
    if ((i & 7) == 0) audio.loop();
  }
  scanPhotos();
  afterPhotosRemoved();
  jsonReply(failed ? 500 : 200, "{\"ok\":" + String(failed ? "false" : "true") + ",\"removed\":" + String(removed) +
                                ",\"failed\":" + String(failed) + ",\"photoCount\":" + String(photoCount) + "}");
}
void phoneBegin() {
  // Enable the radio entropy source before generating pairing credentials.
  WiFi.mode(strlen(BG_WIFI_SSID) ? WIFI_AP_STA : WIFI_AP);
  char suffix[24]; snprintf(suffix, sizeof(suffix), "BabyGrip-%04X", (unsigned)((ESP.getEfuseMac() >> 32) & 0xffff));
  apName = suffix;
  apPassword = prefs.getString("ap_password", "");
  if (apPassword.length() < 12) {
    char password[20]; snprintf(password, sizeof(password), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    apPassword = password; prefs.putString("ap_password", apPassword);
  }
  char token[33]; for (int i = 0; i < 4; ++i) snprintf(token + i * 8, 9, "%08lx", (unsigned long)esp_random()); csrfToken = token;
  if (!WiFi.softAP(apName.c_str(), apPassword.c_str(), 1, false, 2)) Serial.println("[BabyGrip] Wi-Fi AP start failed");
  if (strlen(BG_WIFI_SSID)) WiFi.begin(BG_WIFI_SSID, BG_WIFI_PASSWORD);
  const char* headers[] = {"X-BabyGrip-Token", "Content-Type"}; phone.collectHeaders(headers, 2);
  phone.on("/", HTTP_GET, []() {
    if (!localHost()) { phone.send(400, "text/plain", "Open http://192.168.4.1"); return; }
    phone.sendHeader("Cache-Control", "no-store");
    phone.sendHeader("X-Frame-Options", "DENY");
    phone.sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    // Stream from flash instead of allocating a full HTML copy in RAM.
    const char* marker = strstr(BG_PHONE_HTML, "__BG_TOKEN__");
    const size_t before = (size_t)(marker - BG_PHONE_HTML);
    const char* after = marker + strlen("__BG_TOKEN__");
    phone.setContentLength(strlen(BG_PHONE_HTML) - strlen("__BG_TOKEN__") + csrfToken.length());
    phone.send(200, "text/html; charset=utf-8", "");
    phone.sendContent_P(BG_PHONE_HTML, before);
    phone.sendContent(csrfToken);
    phone.sendContent_P(after, strlen(after));
  });
  phone.on("/api/state", HTTP_GET, []() { if (!localHost()) apiError(400, "잘못된 기기 주소입니다."); else jsonReply(200, stateJson()); });
  phone.on("/api/photos", HTTP_GET, handlePhotoList);
  phone.on("/api/control", HTTP_POST, handleControl);
  phone.on("/api/time", HTTP_POST, handleTime);
  phone.on("/api/photo", HTTP_POST, finishUpload, []() { handleUpload(false); });
  phone.on("/api/noise", HTTP_POST, finishUpload, []() { handleUpload(true); });
  phone.on("/api/delete", HTTP_POST, handleDelete);
  phone.on("/api/delete_all", HTTP_POST, handleDeleteAll);
  phone.on("/api/feeds", HTTP_GET, handleFeedList);
  phone.on("/api/feed", HTTP_POST, handleFeedAction);
  phone.onNotFound([]() { apiError(404, "기기 페이지를 찾을 수 없습니다.", "missing"); });
  phone.begin();
  Serial.printf("[BabyGrip WiFi] SSID=%s password=%s URL=http://192.168.4.1\n", apName.c_str(), apPassword.c_str());
}
#include "BabyGrip_Diagnostics.h"
}  // namespace

void BabyGrip_Begin() {
  Serial.setRxBufferSize(8192);  // room for one BGPUT data line
  // Flash writes pause the USB interrupt; with the default 100 ms the core
  // treats USB as unplugged and drops replies.
  Serial.setTxTimeoutMs(1000);
  Lvgl_EnableScreenCopy();
  prefs.begin("babygrip", false);
  const int tz = prefs.getInt("tzoff", 100000);
  if (tz != 100000) applyTimezone(tz);
  restoreClockFromRtc();
  nightMode = prefs.getBool("night", false);
  nightAuto = prefs.getBool("nightauto", true);
  nightStartMin = (int)prefs.getUInt("nightstart", 20 * 60);
  nightEndMin = (int)prefs.getUInt("nightend", 7 * 60);
  dayLight = (int)prefs.getUInt("daylight", 50);
  nightLight = (int)prefs.getUInt("nightlight", 6);
  glanceAuto = prefs.getBool("glance", true);
  slideAuto = prefs.getBool("slideauto", true);
  slideSeconds = constrain(prefs.getUInt("slidesec", 5), 3u, 300u);
  displayUpSign = prefs.getInt("upsign", -1) > 0 ? 1.0f : -1.0f;
  storedDay = prefs.getUInt("day", 0);
  persistedTotalSecs = prefs.getUInt("total", 0);
  finishedTodaySecs = 0;
  persistedActive = prefs.getBool("active", false);
  persistedStartEpoch = prefs.getULong64("start", 0);
  photos = (PhotoEntry*)heap_caps_calloc(MAX_PHOTOS, sizeof(PhotoEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  initStorage();
  pixels = (uint8_t*)heap_caps_malloc(PHOTO_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  photoStaging = (uint8_t*)heap_caps_malloc(PHOTO_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (pixels) {
    memset(pixels, 0, PHOTO_SIZE);
    picture.header.always_zero=0;
    picture.header.w=W; picture.header.h=H;
    picture.header.cf=LV_IMG_CF_TRUE_COLOR;
    picture.data_size=PHOTO_SIZE;
    picture.data=pixels;
  } else Serial.println("[BabyGrip] PSRAM allocation failed");
  scanPhotos();
  if (photoCount && !loadPhoto(0)) stepPhoto(1);
  scanTracks();
  checkCalendar();  // the RTC may already have given us the time
  loadLog();
  if (clockAvailable && nightAuto) checkNightSchedule();
  applyBacklight();
  Serial.printf("[BabyGrip] Ready: PSRAM=%u bytes, photo buffer=%s, photos=%d, volumes=%d, photo space free=%u, sounds=%d\n",
                (unsigned)ESP.getPsramSize(), pixels ? "OK" : "FAILED", photoCount, readyVolumes(), (unsigned)photoFreeBytes(), trackCount);
  audio.setVolumeSteps(255);  // fine steps for fades; levelDb() keeps the five levels as before
  applyVolume();
  crySensitivity.store(constrain((int)prefs.getUChar("cry", 0), 0, 3));
  cryTrackKey = prefs.getString("crytrack", "f:08-shush");
  if (crySensitivity.load() > 0) { cryResumeAtMs.store(millis() + 5000); ensureMicTask(); }
  soundMessage = BG_USE_HOME_ASSISTANT ? "Ready to connect" : "Tap PLAY to start";
  phoneBegin();
  showPage(slideAuto && photoCount ? FRAME : photoCount || clockAvailable ? HOME : PHONE);
}
void BabyGrip_Update() {
  // Keep decoding and all audio controls on the Arduino loop task.
  audio.loop();
  phone.handleClient();
  diagnosticPoll();
  if (uploadStarted && millis() - uploadStartedMs > 45000) {
    discardUpload(); uploadStarted = false; uploadEnded = false;
  }
  const uint32_t now=millis();
  if (strlen(BG_WIFI_SSID) && WiFi.status()==WL_CONNECTED && !ntpStarted) {
    configTzTime(BG_TIMEZONE, "pool.ntp.org", "time.google.com");
    ntpStarted=true;
  }
  if (!clockAvailable && (now - lastUiMs) >= 1000) checkCalendar();
  if (noisePlaying && (int32_t)(now - noiseEndsMs) >= 0) {
    stopNoise();
    if (page==NOISE) requestPage(NOISE);
  }
  // Soft start and the last minute's fade-out.
  static uint32_t lastFadeMs = 0;
  if (noisePlaying && !BG_USE_HOME_ASSISTANT && now - lastFadeMs >= 100) {
    lastFadeMs = now;
    if (fabsf(fadeDb(now) - appliedFadeDb) > 0.1f) applyVolume();
  }
  if (cryPending.exchange(false)) cryAction();
  if (noisePlaying && !BG_USE_HOME_ASSISTANT &&
      (now - lastAudioRetryMs > 2500)) {
    lastAudioRetryMs = now;
    if (!audio.isRunning()) {
      if (!audio.connecttoFS(trackFs(trackIndex), trackPath(trackIndex).c_str())) {
        noisePlaying = false; soundMessage = "Audio decoder could not restart";
        if (page == NOISE) requestPage(NOISE);
      } else audio.setFileLoop(true);
    }
  }
  // Photos keep changing on ALBUM (PLAY), on the day home screen and on FRAME,
  // but not while the phone is sending files.
  const bool uploading = uploadStarted || now - uploadStartedMs < 5000;
  const bool sliding = (page == ALBUM && autoplay) ||
                       (slideAuto && (page == FRAME || (page == HOME && !nightMode)));
  if (sliding && photoCount > 1 && !uploading && !screenDark && now - lastSlideMs >= slideSeconds * 1000UL) {
    lastSlideMs = now;
    stepPhoto(1);
    updateAlbumStatus();
  }
  // Two minutes without a touch: photos only. FEED and NAP stay while their timer runs.
  const bool idlePage = page == HOME || page == NOISE || page == PHONE || page == ALBUM ||
                        page == SETTINGS || page == SETTINGS2 ||
                        (page == NAP && !napRunning) || (page == FEED && runningLog < 0);
  if (slideAuto && photoCount && idlePage && !needsRebuild &&
      lv_disp_get_inactive_time(nullptr) > FRAME_IDLE_MS) requestPage(FRAME);
  checkOrientation(now);
  if (screenDark && lv_disp_get_inactive_time(nullptr) < 300) { screenDark = false; applyBacklight(); lastMotionMs = now; }
  if (page == LOOK) {
    if (now - lookTouchedMs > 60000) requestPage(HOME);
    else if (now - lookFlipMs > 700) { lookFlipMs = now; lookInvert = !lookInvert; requestPage(LOOK); }
  }
  if (needsRebuild) {
    needsRebuild=false;
    showPage(pendingPage);
  }
  if (now - lastUiMs >= 1000) {
    lastUiMs=now;
    if (clockAvailable) checkCalendar();
    checkNightSchedule();
    if (feedAutoStop() && (page == FEED || page == HOME || page == GLANCE)) requestPage(page);
    refreshLive();
    if (page==NAP) {
      char buf[40]; fmtDuration(napElapsed(),buf,sizeof(buf));
      if (elapsedLabel) lv_label_set_text(elapsedLabel,buf);
      fmtDuration(finishedTodaySecs+napElapsed(),buf,sizeof(buf));
      if (totalLabel) lv_label_set_text(totalLabel,buf);
    }
    if (page==NOISE) {
      if (noiseStatus) lv_label_set_text(noiseStatus,soundMessage.c_str());
      if (noiseVolume) {
        char vol[20]; snprintf(vol,sizeof(vol),"VOL %d / %d",
                               noiseVolumeLevel,BG_ONBOARD_VOLUME_CAP);
        lv_label_set_text(noiseVolume,vol);
      }
    }
  }
}
