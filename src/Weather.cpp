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
#include <rom/miniz.h>   // ROM 自带 miniz：tinfl_decompress_mem_to_mem 解压 gzip

static const char* GEO_HOST   = "geoapi.qweather.com";
static const char* DEV_HOST   = "devapi.qweather.com";
static const int   HTTPS_PORT = 443;

// 最小 URL 编码：query 值里的中文等字符需要百分号编码
static String urlEncode(const String& s) {
  static const char* kHex = "0123456789ABCDEF";
  String out; out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%'; out += kHex[c >> 4]; out += kHex[c & 0xF];
    }
  }
  return out;
}

// gzip(RFC1952) 解压：ROM miniz 只做 raw inflate，这里手动跳过 gzip 头/尾
// 注意：响应体必须是原始字节流读取得到的，按行读会破坏二进制
static bool gzipInflate(const String& in, String& out) {
  const uint8_t* p = (const uint8_t*)in.c_str();
  size_t n = in.length();
  if (n < 18) return false;
  if (p[0] != 0x1F || p[1] != 0x8B || p[2] != 0x08) return false;
  uint8_t flg = p[3];
  size_t off = 10;                                  // 固定头 10 字节
  if (flg & 0x04) {                                 // FEXTRA
    if (off + 2 > n) return false;
    uint16_t xlen = p[off] | (p[off + 1] << 8);
    off += 2 + xlen;
  }
  if (flg & 0x08) { while (off < n && p[off]) off++; off++; }   // FNAME
  if (flg & 0x10) { while (off < n && p[off]) off++; off++; }   // FCOMMENT
  if (flg & 0x02) off += 2;                                     // FHCRC
  if (off + 8 >= n) return false;                   // 至少留 8 字节 CRC32+ISIZE

  static uint8_t dbuf[16384];                       // 解压输出（天气 JSON < 4KB）
  size_t dn = tinfl_decompress_mem_to_mem(dbuf, sizeof dbuf, p + off, n - off - 8, 0);
  if (dn == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) return false;
  out = "";
  out.concat((const char*)dbuf, dn);
  return true;
}

// 一次 HTTPS GET 的实际执行（TLS 握手 + tinfl 解压都是栈大户）
// extraHeader：完整额外请求头行（如 "X-QW-Api-Key: xxx"），空则不发
static bool httpsGetInner(const char* host, const String& path, const String& extraHeader,
                          String& out) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8000);        // ms：读写超时，防止无限挂起冻住屏幕
  client.setHandshakeTimeout(10); // s：TLS 握手超时
  delay(200);
  Serial.printf("[http] GET https://%s%s\n", host, path.c_str());
  Serial.printf("[http] heap=%u\n", ESP.getFreeHeap());

  // 显式 DNS，定位卡点在解析还是连接
  IPAddress ip;
  unsigned long tDns = millis();
  if (WiFi.hostByName(host, ip) != 1) {
    Serial.println("[weather] dns failed");
    return false;
  }
  Serial.printf("[weather] dns -> %s (%lums)\n", ip.toString().c_str(), millis() - tDns);

  unsigned long t0 = millis();
  if (!client.connect(ip, HTTPS_PORT)) {
    Serial.printf("[weather] connect failed (%lums)\n", millis() - t0);
    return false;
  }
  Serial.printf("[weather] tls connected (%lums)\n", millis() - t0);

  String req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n";
  if (extraHeader.length()) req += extraHeader + "\r\n";
  req += "User-Agent: WeatherClock/1.0\r\nConnection: close\r\n\r\n";
  client.print(req);

  // 原始字节流读取（头+体），避免按行读破坏 gzip 二进制
  String raw;
  String headerLc;   // 仅头部（小写），用于取 Content-Length
  unsigned long clen = 0;
  int hdrEnd = -1;
  unsigned long lastLog = t0;
  while (millis() - t0 < 8000) {
    int avail = client.available();
    if (avail > 0) {
      uint8_t rbuf[512];
      int got = client.read(rbuf, avail < (int)sizeof rbuf ? avail : (int)sizeof rbuf);
      if (got > 0) raw.concat((const char*)rbuf, got);
    }
    if (hdrEnd < 0) {
      hdrEnd = raw.indexOf("\r\n\r\n");
      if (hdrEnd >= 0) {
        headerLc = raw.substring(0, hdrEnd);
        headerLc.toLowerCase();
        int ci = headerLc.indexOf("content-length:");
        if (ci >= 0) clen = headerLc.substring(ci + 15).toInt();
      }
    }
    if (hdrEnd >= 0) {
      // Content-Length 已收满，或服务器已关闭连接，即可提前结束
      if (clen > 0 && raw.length() >= (unsigned)(hdrEnd + 4 + clen)) break;
      if (!client.connected() && client.available() == 0) break;
    }
    if (millis() - lastLog >= 2000) {   // 进度日志：观察是否卡住
      lastLog = millis();
      Serial.printf("[weather] recv: raw=%u hdr=%d conn=%d (%lums)\n",
                    (unsigned)raw.length(), hdrEnd, (int)client.connected(), millis() - t0);
    }
    delay(2);
  }
  client.stop();

  if (hdrEnd < 0) {
    Serial.println("[weather] no header (timeout)");
    return false;
  }
  String status = raw.substring(0, raw.indexOf('\r'));
  String body = raw.substring(hdrEnd + 4);
  // gzip -> JSON；非 gzip（普通 JSON/文本）原样使用
  if (!gzipInflate(body, out)) out = body;
  Serial.printf("[http] status='%s' rawlen=%u outlen=%u t=%lums\n",
                status.c_str(), (unsigned)body.length(), (unsigned)out.length(),
                millis() - t0);
  return out.length() > 0;
}

// httpsGet 在独立 16KB 栈任务里执行：
// ROM miniz 的 tinfl_decompress_mem_to_mem 会在调用者栈上放 ~11KB 的解压状态，
// Arduino loopTask 默认只有 8KB 栈（且核心预编译无法调整），直接跑必栈溢出。
struct HttpCtx {
  const char*  host;
  const String* path;
  const String* extraHeader;
  String*      out;
  volatile bool done;
  bool         ok;
};

static void httpsTaskFn(void* arg) {
  HttpCtx* c = (HttpCtx*)arg;
  c->ok = httpsGetInner(c->host, *c->path, *c->extraHeader, *c->out);
  c->done = true;
  vTaskDelete(nullptr);
}

bool httpsGetRaw(const char* host, const String& path, const String& extraHeader,
                 String& out) {
  HttpCtx c{host, &path, &extraHeader, &out, false, false};
  if (xTaskCreate(httpsTaskFn, "wfetch", 16384, &c, 2, nullptr) != pdPASS) {
    Serial.println("[http] task create failed");
    return false;
  }
  while (!c.done) delay(10);
  return c.ok;
}

bool geoResolveCity(const String& apikey, const String& apiHost, const String& cityName, String& outId) {
  if (apikey.length() == 0 || cityName.length() == 0) return false;
  String body;
  // 2024 后新账号使用控制台分配的专属 API Host，Geo 路径带 /geo 前缀；
  // 旧账号留空则走 geoapi.qweather.com
  const char* host = apiHost.length() > 0 ? apiHost.c_str() : GEO_HOST;
  String path = apiHost.length() > 0 ? "/geo/v2/city/lookup" : "/v2/city/lookup";
  path += "?location=" + urlEncode(cityName) + "&key=" + apikey;
  if (!httpsGetRaw(host, path, "X-QW-Api-Key: " + apikey, body)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[geo] json parse failed: %s (bodylen=%u)\n", err.c_str(), (unsigned)body.length());
    return false;
  }
  const char* code = doc["code"] | "";
  if (String(code) != "200") {
    Serial.printf("[geo] api code=%s\n", code);
    return false;
  }
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
  // 新账号走专属 API Host，旧账号走 devapi.qweather.com
  const char* host = cfg.qweather_host.length() > 0 ? cfg.qweather_host.c_str() : DEV_HOST;
  String path = "/v7/weather/now?location=" + loc + "&key=" + cfg.qweather_key;
  if (!httpsGetRaw(host, path, "X-QW-Api-Key: " + cfg.qweather_key, body)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[weather] json parse failed: %s (bodylen=%u)\n", err.c_str(), (unsigned)body.length());
    return false;
  }
  out.code = atoi(doc["code"] | "0");
  if (out.code != 200) {
    Serial.printf("[weather] api code=%d\n", out.code);
    return false;
  }

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