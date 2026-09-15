// ============================================================================
//  Quote.cpp - 行情数据拉取实现（东方财富预设 + 自定义 JSON 数据源）
// ============================================================================
#include "Quote.h"
#include "Weather.h"      // httpsGetRaw
#include <ArduinoJson.h>
#include <math.h>

static const char* EM_HOST = "push2.eastmoney.com";
static const char* JD_HOST = "api.jdjygold.com";   // 京东金融：浙商积存金
static const char* JD_PATH = "/gw2/generic/jrm/h5/m/stdLatestPrice?productSku=1961543816";

// 预设：0=占位 1=金价(浙商积存金·京东金融) 2=布伦特原油当月连续
//       3=占位(自定义 JSON) 4=沪铜主力连续(上期所 CUM)
struct Preset { const char* secid; const char* label; const char* unit; };
static const Preset PRESETS[] = {
  { nullptr,       nullptr, nullptr },
  { "118.AU9999",  "金价",  "元/克" },
  { "112.B00Y",    "布油",  "美元/桶" },
  { nullptr,       nullptr, nullptr },  // 3 预留：自定义模式
  { "113.CUM",     "沪铜",  "元/吨" },
};

// 涨跌幅字符串转 float："-0.53%" -> -0.53
static float pctStrToFloat(const char* s) {
  if (!s) return 0.0f;
  char tmp[16]; int k = 0;
  for (int i = 0; s[i] && k < 15; i++) {
    char c = s[i];
    if (c == '%' || c == ' ' || c == '\r' || c == '\n') continue;
    tmp[k++] = c;
  }
  tmp[k] = 0;
  return atof(tmp);
}

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

int quoteSlotCount(const AppConfig& cfg) {
  int n = 0;
  for (int i = 0; i < cfg.quote_slot_count && i < QUOTE_SLOTS; i++)
    if (cfg.quote_slots[i].type > 0) n++;
  return n;
}

bool fetchQuote(const AppConfig& cfg, int index, QuoteData& out) {
  out = QuoteData();
  if (index < 0 || index >= cfg.quote_slot_count || index >= QUOTE_SLOTS) return false;
  const QuoteSlot& slot = cfg.quote_slots[index];
  if (slot.type <= 0) return false;

  String host, path, label, unit;
  bool jdGold   = (slot.type == 1);                          // 金价走京东金融·浙商积存金
  bool emPreset = (slot.type == 2 || slot.type == 4);        // 布油/沪铜仍走东财

  if (jdGold) {
    host  = JD_HOST;
    path  = JD_PATH;
    label = PRESETS[1].label;
    unit  = PRESETS[1].unit;
  } else if (emPreset) {
    const Preset& p = PRESETS[slot.type];  // 数组边界：有效预设仅 1/2/4
    host = EM_HOST;
    path = String("/api/qt/stock/get?secid=") + p.secid + "&fields=f43,f59,f170";
    label = p.label;
    unit  = p.unit;
  } else {
    if (slot.url.length() == 0 || slot.path.length() == 0) {
      Serial.println("[quote] custom mode but url/path empty");
      return false;
    }
    if (!splitUrl(slot.url, host, path)) {
      Serial.println("[quote] bad url");
      return false;
    }
    label = slot.label.length() ? slot.label : "行情";
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

  if (jdGold) {
    // 京东金融·浙商积存金：success=true 且 resultData.datas 内字段均为字符串
    JsonObject datas = doc["resultData"]["datas"].as<JsonObject>();
    if (doc["success"].as<bool>() != true || datas.isNull()) {
      Serial.println("[quote] jd data null");
      return false;
    }
    out.price     = atof(datas["price"]          | "0");
    out.changePct = pctStrToFloat(datas["upAndDownRate"] | "");
    out.decimals  = 2;
  } else if (emPreset) {
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
    if (!jsonPathFloat(doc, slot.path, v)) {
      Serial.println("[quote] json path not found");
      return false;
    }
    out.price = v;
    out.decimals = 2;
  }

  out.label = label;
  out.unit  = unit;
  out.ok    = true;
  Serial.printf("[quote][%d] %s %.2f %s (%+.2f%%)\n", index,
                label.c_str(), out.price, unit.c_str(), out.changePct);
  return true;
}
