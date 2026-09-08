// ============================================================================
//  Portal.cpp - 网页配置实现
//  两种模式共用同一套页面与 /save 处理：
//   - portalEnter():        softAP 门户（首次/按住 BOOT），访问 http://192.168.4.1
//   - portalServerBegin():  STA 常驻配置页，访问 http://<设备IP> / weatherclock.local
// ============================================================================
#include "Portal.h"
#include "BoardPins.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>

// 常驻配置服务器（STA 模式）
static WebServer* gServer = nullptr;
static AppConfig  gCfg;

// 生成 web 页面（$IP$ 占位替换为当前访问地址）
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
"<h1>☀ 天气时钟 配置</h1><small>$IP$</small>"
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
static String renderPage(const AppConfig& cfg, const String& ipText) {
  String s = FPSTR(PAGE_HTML);
  String latlon;
  if (cfg.lat != 0 || cfg.lon != 0) {
    latlon = String(cfg.lon, 2) + "," + String(cfg.lat, 2);
  }
  s.replace("$IP$", ipText);
  s.replace("$S$", cfg.wifi_ssid);
  s.replace("$K$", cfg.qweather_key);
  s.replace("$C$", cfg.city_name);
  s.replace("$G$", latlon);
  return s;
}

// /save 处理（两种模式共用）
static void handleSave(WebServer& server, AppConfig& cfg) {
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
}

// 注册路由（两种模式共用）；ipText 按值传入并捕获，避免悬垂引用
static void registerRoutes(WebServer& server, AppConfig& cfg, String ipText) {
  server.on("/", HTTP_GET, [&, ipText]() {
    server.send(200, "text/html; charset=utf-8", renderPage(cfg, ipText));
  });
  server.on("/save", HTTP_POST, [&]() {
    handleSave(server, cfg);
  });
  server.onNotFound([&]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/html", "");
  });
}

// ---- softAP 首次配置门户（阻塞） ----
void portalEnter(const AppConfig& current) {
  // 生成 AP SSID：WeatherClock-<MAC后4位>
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String suffix = mac.length() >= 4 ? mac.substring(mac.length() - 4) : "0001";
  String apSsid = "WeatherClock-" + suffix;

  // C3 SuperMini WiFi 初始化：AP+STA（STA 用于射频扫描诊断），关闭省电
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  // C3 SuperMini 批次天线问题：满功率发射不可见，必须限功率（见 main.cpp capWifiTxPower）
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  delay(100);

  // ---- 射频诊断：扫描周围 AP，验证 RX/天线是否工作 ----
  Serial.println("[diag] scanning nearby WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.printf("[diag] scan found %d networks:\n", n);
  for (int i = 0; i < n && i < 12; i++) {
    Serial.printf("[diag]   %d: %s  ch=%d rssi=%d\n", i,
                  WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
  }
  WiFi.scanDelete();

  // 尝试开启 softAP（重试 3 次）
  bool apOk = false;
  for (int i = 0; i < 3 && !apOk; i++) {
    apOk = WiFi.softAP(apSsid.c_str(), nullptr, 11, 0, 4);
    Serial.printf("[portal] softAP try %d: %s\n", i + 1, apOk ? "OK" : "FAIL");
    if (!apOk) delay(500);
  }
  delay(200);  // 给 RF 校准时间

  if (!apOk) {
    Serial.println("[portal] softAP 全部失败！重启...");
    delay(2000);
    ESP.restart();
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[portal] AP: %s  IP: %s  ch=%d  mac=%s\n",
                apSsid.c_str(), apIP.toString().c_str(),
                WiFi.channel(), WiFi.softAPmacAddress().c_str());
  Serial.println("[portal] 请在浏览器访问 http://192.168.4.1");

  WebServer server(80);
  AppConfig cfg = current;
  String ipText = "AP: " + apIP.toString();
  registerRoutes(server, cfg, ipText);
  server.begin();
  while (true) {
    server.handleClient();
    delay(10);
  }
}

// ---- STA 常驻配置服务器（非阻塞） ----
void portalServerBegin(const AppConfig& current) {
  gCfg = current;
  if (gServer) delete gServer;
  gServer = new WebServer(80);
  String ip = WiFi.localIP().toString();
  String ipText = "STA: " + ip + " · 修改配置保存后自动重启";
  registerRoutes(*gServer, gCfg, ipText);
  gServer->begin();
  Serial.printf("[portal] 配置页: http://%s\n", ip.c_str());
  if (MDNS.begin("weatherclock")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[portal] mDNS 配置页: http://weatherclock.local");
  }
}

void portalServerLoop() {
  if (gServer) gServer->handleClient();
}
