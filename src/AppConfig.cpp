// ============================================================================
//  AppConfig.cpp - NVS 配置存储实现
// ============================================================================
#include "AppConfig.h"
#include <Preferences.h>

static Preferences prefs;

#define NS "wclock"

String prefsGet(const char* key, const String& dflt = "") {
  String s = prefs.getString(key, dflt);
  return s;
}

bool loadConfig(AppConfig& cfg) {
  cfg = AppConfig();
  if (!prefs.begin(NS, true)) return false;   // RO mode

  // 只要保存过 WiFi SSID 即视为已配置（API Key/城市可在网页上补填）
  cfg.valid = (prefsGet("wifi_ssid").length() > 0);

  cfg.wifi_ssid  = prefsGet("wifi_ssid");
  cfg.wifi_pass  = prefsGet("wifi_pass");
  cfg.qweather_key = prefsGet("qkey");
  cfg.city_name  = prefsGet("city");
  cfg.location_id = prefsGet("loc");
  cfg.lat = prefs.getFloat("lat", 0);
  cfg.lon = prefs.getFloat("lon", 0);

  prefs.end();
  return cfg.valid;
}

bool saveConfig(const AppConfig& cfg) {
  if (!prefs.begin(NS, false)) return false;  // RW mode
  prefs.putBool("valid", true);
  prefs.putString("wifi_ssid", cfg.wifi_ssid);
  prefs.putString("wifi_pass", cfg.wifi_pass);
  prefs.putString("qkey", cfg.qweather_key);
  prefs.putString("city", cfg.city_name);
  prefs.putString("loc", cfg.location_id);
  prefs.putFloat("lat", cfg.lat);
  prefs.putFloat("lon", cfg.lon);
  prefs.end();
  return true;
}

bool saveLocationId(const String& id) {
  if (!prefs.begin(NS, false)) return false;
  prefs.putString("loc", id);
  prefs.end();
  return true;
}

bool clearConfig() {
  if (!prefs.begin(NS, false)) return false;
  prefs.clear();
  prefs.end();
  return true;
}