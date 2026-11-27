#pragma once

// Edit Wi-Fi only if you want NTP-synchronized daily totals or Home Assistant.
// Empty SSID: all basic controls still work, but totals are for this power session.
static const char* BG_WIFI_SSID = "";
static const char* BG_WIFI_PASSWORD = "";
static const char* BG_TIMEZONE = "PST8PDT,M3.2.0,M11.1.0"; // America/Los_Angeles

// Default: uses the Waveshare onboard speaker with /white_noise.mp3 on microSD.
// For a separate nursery speaker, configure two Home Assistant scripts and
// a media_player entity. The scripts should handle the speaker's own playlist.
static const bool BG_USE_HOME_ASSISTANT = false;
static const char* BG_HA_BASE_URL = "http://homeassistant.local:8123";
static const char* BG_HA_BEARER_TOKEN = "";
static const char* BG_HA_PLAY_SCRIPT = "script.baby_white_noise_on";
static const char* BG_HA_STOP_SCRIPT = "script.baby_white_noise_off";
static const char* BG_HA_MEDIA_PLAYER = "media_player.nursery";
static const float BG_HA_VOLUME_CAP = 0.25f;  // absolute cap 0..1, not a dBA guarantee

static const unsigned BG_SLIDE_INTERVAL_MS = 5000;
static const int BG_ONBOARD_VOLUME_CAP = 5; // device library accepts 0..21
