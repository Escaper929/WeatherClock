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

// ---------------------------------------------------------------------------
// 行情轮播列表 <-> 字符串编码
//   条目之间 '\n' 分隔；预设条目仅类型数字；自定义条目 "3;;url;;path;;label"。
// ---------------------------------------------------------------------------
String serializeQuoteSlots(const AppConfig& cfg) {
  String out;
  for (int i = 0; i < cfg.quote_slot_count && i < QUOTE_SLOTS; i++) {
    const QuoteSlot& s = cfg.quote_slots[i];
    if (s.type == 0) continue;
    if (out.length()) out += '\n';
    if (s.type == 3) {
      out += "3;;" + s.url + ";;" + s.path + ";;" + s.label;
    } else {
      out += String(s.type);
    }
  }
  return out;
}

bool parseQuoteSlots(const String& src, AppConfig& cfg) {
  cfg.quote_slot_count = 0;
  if (src.length() == 0) return true;   // 空 = 关闭行情
  int start = 0;
  while (start <= (int)src.length()) {
    int nl = src.indexOf('\n', start);
    String item = (nl < 0) ? src.substring(start) : src.substring(start, nl);
    start = (nl < 0) ? (int)src.length() + 1 : nl + 1;
    item.trim();
    if (item.length() == 0) continue;
    if (cfg.quote_slot_count >= QUOTE_SLOTS) break;

    QuoteSlot& sl = cfg.quote_slots[cfg.quote_slot_count];
    String typeS = item;
    int fs = item.indexOf(";;");
    if (fs >= 0) typeS = item.substring(0, fs);
    int t = typeS.toInt();
    if (t == 0 || t > 4) continue;             // 非法类型跳过
    sl.type = t;
    if (t == 3) {
      // 字段：url;;path;;label
      String rest = item.substring(fs + 2);
      int p1 = rest.indexOf(";;"), p2 = rest.indexOf(";;", p1 + 2);
      sl.url   = (p1 < 0) ? "" : rest.substring(0, p1);
      sl.path  = (p1 < 0) ? "" : (p2 < 0) ? rest.substring(p1 + 2) : rest.substring(p1 + 2, p2);
      sl.label = (p2 < 0) ? "" : rest.substring(p2 + 2);
      sl.url.trim(); sl.path.trim(); sl.label.trim();
      if (sl.url.length() == 0) continue;      // 自定义无 URL 视为无效条目
    }
    cfg.quote_slot_count++;
  }
  return true;
}

// 旧版单行情字段（qm/qu/qp/ql）迁移为新的轮播列表首条
static void migrateLegacyQuote(AppConfig& cfg) {
  int m = prefs.getInt("qm", 0);
  if (m <= 0) return;
  QuoteSlot& s = cfg.quote_slots[0];
  s.type = m;
  if (m == 3) {
    s.url   = prefsGet("qu");
    s.path  = prefsGet("qp");
    s.label = prefsGet("ql");
    if (s.url.length() == 0) return;           // 旧的空自定义配置不迁移
  }
  cfg.quote_slot_count = 1;
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
  cfg.theme = prefs.getUChar("theme", 0);
  if (!themeIdValid(cfg.theme)) cfg.theme = 0;   // 非法值（含已删除主题旧 id）回退 Modern

  // 行情轮播：优先读新列表，兼容旧版单行情字段
  String ql = prefsGet("qlist");
  if (ql.length() > 0) {
    parseQuoteSlots(ql, cfg);
  } else {
    migrateLegacyQuote(cfg);
  }
  cfg.quote_rotate_s = constrain(prefs.getInt("qrt", 6), 2, 60);

  Serial.printf("[cfg] loaded: ssid=%s keylen=%d host='%s' city=%s quotes=%d\n",
                cfg.wifi_ssid.c_str(), cfg.qweather_key.length(),
                cfg.qweather_host.c_str(), cfg.city_name.c_str(), cfg.quote_slot_count);
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
  prefs.putString("qlist", serializeQuoteSlots(cfg));
  prefs.putInt("qrt", cfg.quote_rotate_s);
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