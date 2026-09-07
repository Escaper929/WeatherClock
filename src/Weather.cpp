// ============================================================================
//  Weather.cpp - 和风天气 QWeather 客户端实现
//  免费版接口：
//    Geo 解析: GET https://geoapi.qweather.com/v2/city/lookup?location=城市&key=KEY
//    当前天气: GET https://devapi.qweather.com/v7/weather/now?location=ID&key=KEY
//  使用 WifiClientSecure + setInsecure()（不校验证书），以节省 flash/RAM。
// ============================================================================
#include "Weather.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

static const char* GEO_HOST   = "geoapi.qweather.com";
static const char* DEV_HOST   = "devapi.qweather.com";
static const int   HTTPS_PORT = 443;

// 一次 HTTPS GET，回调把响应体写入 out（截断到 maxLen）
static bool httpsGet(const char* host, const String& path, String& out, size_t maxLen = 2048) {
  WiFiClientSecure client;
  client.setInsecure();
  delay(200);
  if (!client.connect(host, HTTPS_PORT)) return false;

  String req = "GET " + path + " HTTP/1.1\r\nHost: " + host +
               "\r\nUser-Agent: WeatherClock/1.0\r\nConnection: close\r\n\r\n";
  client.print(req);

  unsigned long t0 = millis();
  bool headersDone = false;
  out = "";
  while (millis() - t0 < 8000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (!headersDone) {
        if (line.isEmpty()) headersDone = true;  // 空行 = 头结束
        continue;
      }
      out += line;
      if (out.length() > maxLen) { client.stop(); return true; }
    }
    if (headersDone && out.length() == 0) break;   // 空响应
    delay(2);
  }
  client.stop();
  return headersDone && out.length() > 0;
}

bool geoResolveCity(const String& apikey, const String& cityName, String& outId) {
  if (apikey.length() == 0 || cityName.length() == 0) return false;
  String body;
  String path = "/v2/city/lookup?location=" + cityName + "&key=" + apikey;
  if (!httpsGet(GEO_HOST, path, body)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  const char* code = doc["code"] | "";
  if (String(code) != "200") return false;
  JsonArray loc = doc["location"].as<JsonArray>();
  if (loc.size() == 0) return false;
  const char* id = loc[0]["id"] | "";
  if (strlen(id) == 0) return false;
  outId = String(id);
  return true;
}

bool fetchWeather(const AppConfig& cfg, WeatherData& out) {
  out = WeatherData();
  if (cfg.qweather_key.length() == 0) return false;

  String loc;
  if (cfg.lat != 0 || cfg.lon != 0) {
    loc = String(cfg.lon, 2) + "," + String(cfg.lat, 2);  // 和风要求经度,纬度
  } else {
    loc = cfg.location_id;
  }
  if (loc.length() == 0) return false;

  String body;
  String path = "/v7/weather/now?location=" + loc + "&key=" + cfg.qweather_key;
  if (!httpsGet(DEV_HOST, path, body)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  out.code = atoi(doc["code"] | "0");
  if (out.code != 200) return false;

  JsonObject now = doc["now"].as<JsonObject>();
  if (now.isNull()) return false;

  out.temp     = (const char*)now["temp"] ? atof(now["temp"]) : 0;
  out.feels    = (const char*)now["feelsLike"] ? atof(now["feelsLike"]) : 0;
  out.humidity = (const char*)now["humidity"] ? atoi(now["humidity"]) : 0;
  out.icon     = (const char*)now["icon"] ? atoi(now["icon"]) : 0;
  out.text     = now["text"] | "未知";
  out.ok       = true;
  return true;
}

const char* weatherTextShort(int iconCode) {
  // 100 晴；101~103 多云/阴；300 段 雨；400 段 雪；500 段 雾/霾
  int g = iconCode / 100;
  if (iconCode == 100) return "Sunny";
  if (iconCode == 150) return "Clear ngt";
  if (iconCode >= 101 && iconCode <= 103) return "Cloud";
  if (iconCode == 104) return "Overcast";
  if (g == 3) return "Rain";
  if (g == 4) return "Snow";
  if (g == 5) return "Fog";
  if (iconCode >= 300 && iconCode <= 303) return "Shower";
  if (iconCode >= 304 && iconCode <= 313) return "Storm";
  if (iconCode >= 314 && iconCode <= 399) return "Thunder";
  return "Weather";
}