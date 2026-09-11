// ============================================================================
//  Quote.cpp - 行情数据拉取实现（东方财富预设 + 自定义 JSON 数据源）
// ============================================================================
#include "Quote.h"
#include "Weather.h"      // httpsGetRaw
#include <ArduinoJson.h>
#include <math.h>

static const char* EM_HOST = "push2.eastmoney.com";

// 预设：0=占位 1=金价(Au9999 现货，积存金价格锚定) 2=布伦特原油当月连续
//       3=占位(自定义 JSON) 4=沪铜主力连续(上期所 CUM)
struct Preset { const char* secid; const char* label; const char* unit; };
static const Preset PRESETS[] = {
  { nullptr,       nullptr, nullptr },
  { "118.AU9999",  "金价",  "元/克" },
  { "112.B00Y",    "布油",  "美元/桶" },
  { nullptr,       nullptr, nullptr },  // 3 预留：自定义模式
  { "113.CUM",     "沪铜",  "元/吨" },
};

// https://host/path?query 拆成 host 与 path
static bool splitUrl(const String& url, String& host, String& path) {
  String u = url;
  u.trim();
  int sp = u.indexOf("://");
  if (sp < 0) return false;
  String rest = u.substring(sp + 3);
  int sl = rest.indexOf('/');
  if (sl < 0) { host = rest; path = "/"; }
  else        { host = rest.substring(0, sl); path = rest.substring(sl); }
  host.trim();
  return host.length() > 0;
}

// 按点分路径从 JSON 取数字，如 "data.f43" / "price" / "result.list.0.last"
static bool jsonPathFloat(JsonDocument& doc, const String& path, float& out) {
  JsonVariant cur = doc.template as<JsonVariant>();
  int start = 0;
  while (true) {
    int dot = path.indexOf('.', start);
    String key = (dot < 0) ? path.substring(start) : path.substring(start, dot);
    key.trim();
    if (key.length() == 0) return false;
    cur = cur[key.c_str()];
    if (cur.isNull()) return false;
    if (dot < 0) break;
    start = dot + 1;
  }
  if (!cur.is<float>()) return false;
  out = cur.as<float>();
  return true;
}

bool fetchQuote(const AppConfig& cfg, QuoteData& out) {
  out = QuoteData();
  if (cfg.quote_mode <= 0) return false;

  String host, path, label, unit;
  bool emPreset = (cfg.quote_mode == 1 || cfg.quote_mode == 2 || cfg.quote_mode == 4);

  if (emPreset) {
    const Preset& p = PRESETS[cfg.quote_mode];  // 数组边界：有效预设仅 1/2/4
    host = EM_HOST;
    path = String("/api/qt/stock/get?secid=") + p.secid + "&fields=f43,f59,f170";
    label = p.label;
    unit  = p.unit;
  } else {
    if (cfg.quote_url.length() == 0 || cfg.quote_path.length() == 0) {
      Serial.println("[quote] custom mode but url/path empty");
      return false;
    }
    if (!splitUrl(cfg.quote_url, host, path)) {
      Serial.println("[quote] bad url");
      return false;
    }
    label = cfg.quote_label.length() ? cfg.quote_label : "行情";
  }

  String body;
  if (!httpsGetRaw(host.c_str(), path, "", body)) {
    Serial.println("[quote] http get failed");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[quote] json parse failed: %s\n", err.c_str());
    return false;
  }

  if (emPreset) {
    // 东财：f43=最新价(定点数) f59=小数位 f170=涨跌幅%(×100)
    JsonObject d = doc["data"].as<JsonObject>();
    if (d.isNull()) {
      Serial.println("[quote] em data null");
      return false;
    }
    long f43  = d["f43"]  | 0;
    int  f59  = d["f59"]  | 2;
    long f170 = d["f170"] | 0;
    float scale = powf(10.0f, f59);
    out.price     = f43 / scale;
    out.changePct = f170 / 100.0f;
    out.decimals  = f59;
  } else {
    float v;
    if (!jsonPathFloat(doc, cfg.quote_path, v)) {
      Serial.println("[quote] json path not found");
      return false;
    }
    out.price = v;
    out.decimals = 2;
  }

  out.label = label;
  out.unit  = unit;
  out.ok    = true;
  Serial.printf("[quote] %s %.2f %s (%+.2f%%)\n",
                label.c_str(), out.price, unit.c_str(), out.changePct);
  return true;
}
