// ============================================================================
//  AppConfig.cpp - NVS 配置存储实现
// ============================================================================
#include "AppConfig.h"
#include "Theme.h"
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
  cfg.qweather_host = prefsGet("qhost");
  cfg.city_name  = prefsGet("city");
  cfg.location_id = prefsGet("loc");
  cfg.lat = prefs.getFloat("lat", 0);
  cfg.lon = prefs.getFloat("lon", 0);
  cfg.timezone   = prefsGet("tz");
  cfg.quote_mode = prefs.getInt("qm", 0);
  cfg.quote_label = prefsGet("ql");
  cfg.quote_url   = prefsGet("qu");
  cfg.quote_path  = prefsGet("qp");
  cfg.theme = prefs.getUChar("theme", 0);
  if (!themeIdValid(cfg.theme)) cfg.theme = 0;   // 非法值（含已删除主题旧 id）回退 Modern

  Serial.printf("[cfg] loaded: ssid=%s keylen=%d host='%s' city=%s quote=%d\n",
                cfg.wifi_ssid.c_str(), cfg.qweather_key.length(),
                cfg.qweather_host.c_str(), cfg.city_name.c_str(), cfg.quote_mode);
  Serial.print("[cfg] city bytes:");
  for (unsigned i = 0; i < cfg.city_name.length(); i++) Serial.printf(" %02X", cfg.city_name[i]);
  Serial.println();

  prefs.end();
  return cfg.valid;
}

bool saveConfig(const AppConfig& cfg) {
  if (!prefs.begin(NS, false)) { Serial.println("[nvs] begin(RW) FAILED"); return false; }
  prefs.putBool("valid", true);
  prefs.putString("wifi_ssid", cfg.wifi_ssid);
  prefs.putString("wifi_pass", cfg.wifi_pass);
  prefs.putString("qkey", cfg.qweather_key);
  size_t wh = prefs.putString("qhost", cfg.qweather_host);
  prefs.putString("city", cfg.city_name);
  prefs.putString("loc", cfg.location_id);
  prefs.putFloat("lat", cfg.lat);
  prefs.putFloat("lon", cfg.lon);
  prefs.putString("tz", cfg.timezone);
  prefs.putInt("qm", cfg.quote_mode);
  prefs.putString("ql", cfg.quote_label);
  prefs.putString("qu", cfg.quote_url);
  prefs.putString("qp", cfg.quote_path);
  prefs.putUChar("theme", cfg.theme);
  String back = prefs.getString("qhost", "<err>");
  Serial.printf("[nvs] putString(qhost) -> %u bytes; readback='%s'\n", (unsigned)wh, back.c_str());
  prefs.end();
  return true;
}

bool saveLocationId(const String& id) {
  if (!prefs.begin(NS, false)) return false;
  prefs.putString("loc", id);
  prefs.end();
  return true;
}

bool saveTheme(uint8_t themeId) {
  if (!themeIdValid(themeId)) themeId = 0;
  if (!prefs.begin(NS, false)) return false;
  bool ok = prefs.putUChar("theme", themeId) != 0;
  prefs.end();
  return ok;
}

bool clearConfig() {
  if (!prefs.begin(NS, false)) return false;
  prefs.clear();
  prefs.end();
  return true;
}