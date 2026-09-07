// ============================================================================
//  Portal.cpp - 网页配置门户实现
// ============================================================================
#include "Portal.h"
#include "BoardPins.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// 生成 web 页面（单独函数便于读取）
static const char PAGE_HTML[] PROGMEM =
"<!DOCTYPE html><html lang='zh'><head>"
"<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>天气时钟配置</title><style>"
"body{font-family:system-ui,Arial,sans-serif;background:#0b1022;color:#eee;"
"display:flex;justify-content:center;margin:0;padding:20px;}"
".card{background:#1a2340;max-width:380px;width:100%;border-radius:12px;padding:20px;}"
"h1{font-size:18px;margin:0 0 4px;color:#7ff}label{display:block;margin:14px 0 4px;font-size:13px;color:#9db} "
"input{width:100%;box-sizing:border-box;padding:9px;border-radius:6px;border:1px solid #334;"
"background:#0f152b;color:#eee;font-size:14px}"
".btn{width:100%;margin-top:18px;padding:11px;border:0;border-radius:6px;"
"background:#2b8;color:#042;font-size:15px;font-weight:bold;cursor:pointer}"
".tip{font-size:12px;color:#89b;margin-top:8px;line-height:1.5}"
"small{color:#89b}</style></head><body><div class='card'>"
"<h1>☀ 天气时钟 配置</h1><small>AP: $APSSID$ · IP 192.168.4.1</small>"
"<form method='post' action='/save'>"
"<label>WiFi 名称 (SSID)</label>"
"<input name='s' value='$S$' placeholder='例如 MyWiFi'>"
"<label>WiFi 密码</label>"
"<input type='password' name='p' placeholder='WiFi 密码'>"
"<label>和风天气 API Key（免费版 devapi 应用的 key）</label>"
"<input name='k' value='$K$' placeholder='申请于 https://dev.qweather.com'>"
"<label>城市（如 北京 / 上海 广州），留空则用经纬度</label>"
"<input name='c' value='$C$' placeholder='例如 北京'>"
"<label>经纬度（可选，格式：经度,纬度，如 116.41,39.92）</label>"
"<input name='g' value='$G$' placeholder='留空则按城市名解析'>"
"<button class='btn' type='submit'>保存并重启</button>"
"<div class='tip'>城市名改为最直接的市区名即可。保存后设备会重启、连接 WiFi 并同步时间与天气。</div>"
"</form></div></body></html>";

// 页面变量替换
static String renderPage(const AppConfig& cfg, const char* apSsid) {
  String s = FPSTR(PAGE_HTML);
  String latlon;
  if (cfg.lat != 0 || cfg.lon != 0) {
    latlon = String(cfg.lon, 2) + "," + String(cfg.lat, 2);
  }
  s.replace("$APSSID$", apSsid);
  s.replace("$S$", cfg.wifi_ssid);
  s.replace("$K$", cfg.qweather_key);
  s.replace("$C$", cfg.city_name);
  s.replace("$G$", latlon);
  return s;
}

void portalEnter(const AppConfig& current) {
  // 生成 AP SSID：WeatherClock-<MAC后2字节>
  String mac = WiFi.macAddress();
  String suffix = mac.length() >= 2 ? mac.substring(mac.length() - 2) : "01";
  String apSsid = "WeatherClock-" + suffix;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str(), nullptr, 1, 0, 1);

  WebServer server(80);
  AppConfig cfg = current;

  server.on("/", HTTP_GET, [&]() {
    server.send(200, "text/html; charset=utf-8", renderPage(cfg, apSsid.c_str()));
  });

  server.on("/save", HTTP_POST, [&]() {
    cfg.valid = true;
    cfg.wifi_ssid    = server.arg("s");
    cfg.wifi_pass    = server.arg("p");
    cfg.qweather_key = server.arg("k");
    cfg.city_name    = server.arg("c");
    // 解析经纬度（若填写）
    String g = server.arg("g");
    g.trim();
    if (g.length() > 0) {
      int comma = g.indexOf(',');
      if (comma > 0) {
        cfg.lon = g.substring(0, comma).toFloat();
        cfg.lat = g.substring(comma + 1).toFloat();
      }
    }
    // 城市名和经纬度都留空则无法定位
    bool ok = cfg.qweather_key.length() > 0 &&
              cfg.city_name.length() > 0 &&
              cfg.wifi_ssid.length() > 0;
    String html;
    if (!ok) {
      html = F("<html><body style='background:#0b1022;color:#eee;font-family:sans-serif;"
               "text-align:center;padding-top:60px'><h2>❌ 信息不完整</h2>"
               "<p>SSID、API Key 与城市至少需要填写。</p>"
               "<p><a href='/' style='color:#7ff'>返回</a></p></body></html>");
      server.send(200, "text/html; charset=utf-8", html);
      return;
    }
    saveConfig(cfg);   // location_id 等到连上 WiFi 后再解析
    html = F("<html><body style='background:#0b1022;color:#eee;font-family:sans-serif;"
             "text-align:center;padding-top:60px'><h2>✔ 已保存</h2>"
             "<p>设备即将重启并连接 WiFi…</p></body></html>");
    server.send(200, "text/html; charset=utf-8", html);
    delay(600);
    ESP.restart();
  });

  server.onNotFound([&]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/html", "");
  });

  server.begin();
  Serial.println("[portal] 配置门户已开启，请在浏览器访问 http://192.168.4.1");
  while (true) {
    server.handleClient();
    delay(10);
  }
}