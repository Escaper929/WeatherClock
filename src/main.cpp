// ============================================================================
//  main.cpp - 天气时钟主程序
//  首次配置（与 BambuHelper 一致）：
//    USB 连接设备 -> 浏览器烧录页通过 Improv WiFi 协议直接下发 WiFi 账号密码
//    -> 设备加入局域网 -> 浏览器自动跳转 http://<设备IP> 补填 API Key / 城市
//    （按住 BOOT 上电仍可进入 softAP 门户作为兜底）
//  正常运行：NTP 对时 -> 定时拉取天气 -> 屏幕显示时钟与天气
// ============================================================================
#include "BoardPins.h"
#include "AppConfig.h"
#include "WiFiTime.h"
#include "Weather.h"
#include "Screen.h"
#include "Portal.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ImprovWiFiLibrary.h>

static AppConfig   cfg;
static WeatherData weather;
static bool        weatherValid = false;
static unsigned    lastFetch    = 0;
static unsigned    lastBlink    = 0;
static bool        blinkOn      = true;
static unsigned    lastReconnect= 0;

// Improv WiFi（USB 串口配网，与浏览器 esp-web-tools 配合）
static ImprovWiFi improvSerial(&Serial);

// 状态机：provisionMode = 等待 USB 配网；normalMode = 已联网运行
static bool provisionMode   = false;
static bool normalMode      = false;
static bool postInitDone    = false;  // 联网后的一次性初始化（城市解析/首次天气）

static void fetchWeatherSafe() {
  if (cfg.qweather_key.length() == 0) {  // 还没在网页填 API Key，跳过
    lastFetch = millis();
    return;
  }
  WeatherData w;
  if (fetchWeather(cfg, w)) {
    weather = w;
    weatherValid = true;
  }
  lastFetch = millis();
}

// 联网后的一次性初始化：NTP 等待 -> 城市解析 -> 首次天气
static void postConnectInit() {
  if (postInitDone) return;
  postInitDone = true;

  // NTP 时间（最多等 20s）
  ntpBegin();
  unsigned t0 = millis();
  while (!timeIsSynced() && (millis() - t0 < 20000)) {
    renderStatus("Syncing time...");
    wifiReconnect(cfg.wifi_ssid, cfg.wifi_pass);
    delay(200);
  }

  // 城市 -> LocationID（仅当未填经纬度且未解析过时）
  if (cfg.location_id.length() == 0 && cfg.city_name.length() > 0 &&
      (cfg.lat == 0 && cfg.lon == 0)) {
    String id;
    if (geoResolveCity(cfg.qweather_key, cfg.city_name, id)) {
      cfg.location_id = id;
      saveLocationId(id);
      Serial.printf("[geo] city=%s -> id=%s\n", cfg.city_name.c_str(), id.c_str());
    } else {
      Serial.println("[geo] city resolve failed");
    }
  }

  fetchWeatherSafe();
  renderClock(nowSegments(), true);
}

// Improv 配网成功回调（此时 WiFi 已由库连接成功）
static void onImprovConnected(const char* ssid, const char* password) {
  Serial.printf("[improv] provisioned: %s\n", ssid);
  cfg.wifi_ssid = ssid;
  cfg.wifi_pass = password;
  saveConfig(cfg);   // 先持久化 WiFi；API Key/城市稍后在网页补填

  // 启动常驻配置页，浏览器收到 deviceUrl 后会自动打开
  portalServerBegin(cfg);

  provisionMode = false;
  normalMode    = true;
  postInitDone  = false;
  renderStatus("WiFi Connected", "http://" + WiFi.localIP().toString());
}

// ---- WiFi 诊断：记录最近一次断开原因（用于屏幕显示） ----
static volatile int gLastWifiReason = -1;
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    gLastWifiReason = info.wifi_sta_disconnected.reason;
    Serial.printf("[wifi] disconnected reason=%d\n", gLastWifiReason);
  }
}

// 自定义连接（替代库默认实现）：放宽安全级别 + 打印目标 AP 信息与状态迁移
static bool customConnect(const char* ssid, const char* pass) {
  WiFi.setSleep(false);
  // 新版 arduino-esp32 默认最低 WPA2，遇到 WPA/WPA2+WPA3 混合路由器会连接失败
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);

  if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(); delay(100); }

  // 扫描目标 AP：确认可见性与认证方式（浏览器占串口时同步显示到屏幕）
  renderStatus("Scanning...");
  int n = WiFi.scanNetworks(false, true, false, 300, 0, ssid);
  int targetRssi = 0, targetAuth = -1;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == ssid) { targetRssi = WiFi.RSSI(i); targetAuth = WiFi.encryptionType(i); }
  }
  WiFi.scanDelete();
  Serial.printf("[improv] target %s: rssi=%d auth=%d\n", ssid, targetRssi, targetAuth);

  gLastWifiReason = -1;
  renderStatus("Connecting WiFi...", String(ssid) + " rssi=" + targetRssi);
  WiFi.begin(ssid, pass);
  unsigned start = millis();
  wl_status_t st = WiFi.status(), last = WL_IDLE_STATUS;
  while ((st = WiFi.status()) != WL_CONNECTED && millis() - start < 25000) {
    if (st != last) {
      last = st;
      Serial.printf("[wifi] status=%d\n", st);
    }
    delay(250);
  }

  if (st != WL_CONNECTED) {
    WiFi.disconnect();
    // 浏览器占着串口看不到日志，把原因码直接显示到屏幕上
    char sub[40];
    snprintf(sub, sizeof sub, "r=%d auth=%d", gLastWifiReason, targetAuth);
    renderStatus("WiFi failed", sub);
    Serial.printf("[improv] connect failed: status=%d reason=%d\n", st, gLastWifiReason);
    delay(4000);
    renderStatus("USB Setup", "Flash page sets WiFi");
    return false;
  }
  Serial.println("[improv] connected!");
  return true;
}

// Improv CURRENT_STATE 状态包广播。
// esp-web-tools 烧录完只发一次 GET_CURRENT_STATE 探测；若该帧在设备启动期间到达
// 会被丢弃，客户端将停在 STOPPED（界面显示 "Wi-Fi turned off"）。
// 周期广播让客户端在 600s 等待窗口内的任意时刻都能拿到状态并切换出配网表单。
static void improvSendState(uint8_t state) {
  uint8_t pkt[11] = {'I', 'M', 'P', 'R', 'O', 'V', 0x01, 0x01, 0x01, state, 0};
  uint8_t sum = 0;
  for (int i = 0; i < 10; i++) sum += pkt[i];
  pkt[10] = sum;
  Serial.write(pkt, sizeof pkt);
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[main] WeatherClock starting");

  // Improv 设备信息（浏览器配网界面展示），deviceUrl 配网成功后自动跳转
  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3,
                             "WeatherClock", "1.0.0", "WeatherClock",
                             "http://{LOCAL_IPV4}");
  improvSerial.onImprovConnected(onImprovConnected);
  improvSerial.setCustomConnectWiFi(customConnect);
  WiFi.onEvent(onWiFiEvent);

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

  if (!screenInit()) {
    Serial.println("[main] screen init failed!");
  } else {
    screenSetBrightness(SCREEN_BRIGHTNESS);
    renderStatus("WeatherClock", "ESP32-C3");
  }

  bool hasWifi = loadConfig(cfg);

  // 按住 BOOT 上电 -> softAP 门户兜底（阻塞）
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    Serial.println("[main] BOOT held -> softAP portal");
    portalEnter(cfg);
  }

  if (!hasWifi) {
    // 未配置：进入 USB(Improv) 配网等待模式。
    // WIFI_STA 模式供 Improv 扫描周边网络与连接使用。
    provisionMode = true;
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.mode(WIFI_STA);
    renderStatus("USB Setup", "Flash page sets WiFi");
    Serial.println("[main] no WiFi config; waiting for USB Improv provisioning");
    return;
  }

  // 已有配置：连接 WiFi
  renderStatus("Connecting WiFi...");
  Serial.printf("[wifi] SSID=%s\n", cfg.wifi_ssid.c_str());
  if (wifiConnect(cfg.wifi_ssid, cfg.wifi_pass, 20000)) {
    portalServerBegin(cfg);
    renderStatus("Ready", "http://" + WiFi.localIP().toString());
    delay(2500);
    normalMode = true;
  } else {
    // 连接失败：进入 USB 配网模式，可用烧录页重新下发 WiFi
    Serial.println("[wifi] connect failed; entering USB provision mode");
    provisionMode = true;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    renderStatus("WiFi Failed", "Re-setup via USB");
  }
}

void loop() {
  unsigned now = millis();

  // 始终处理 USB Improv 配网请求（任何模式下都可重新配网）
  improvSerial.handleSerial();

  if (provisionMode) {
    // 等待浏览器通过 USB 下发 WiFi：每秒广播一次 AUTHORIZED 状态包
    static unsigned lastAnnounce = 0;
    if (now - lastAnnounce >= 1000) {
      lastAnnounce = now;
      improvSendState(ImprovTypes::State::STATE_AUTHORIZED);
    }
    delay(10);
    return;
  }

  if (!normalMode) { delay(10); return; }

  // 联网后的一次性初始化（NTP/城市/首拉天气）
  if (!postInitDone) postConnectInit();

  // 1. 时钟：每 500ms 切换冒号
  if (now - lastBlink >= 500) {
    lastBlink = now;
    blinkOn = !blinkOn;
    renderClock(nowSegments(), blinkOn);
  }

  // 2. 天气：定期拉取
  if (now - lastFetch >= WEATHER_FETCH_INTERVAL_SEC * 1000UL) {
    if (wifiIsConnected()) {
      fetchWeatherSafe();
      if (weatherValid) renderWeather(weather);
    }
  }

  // 3. WiFi 保活
  if (!wifiIsConnected()) {
    if (now - lastReconnect >= 5000) {
      lastReconnect = now;
      if (wifiReconnect(cfg.wifi_ssid, cfg.wifi_pass)) {
        ntpBegin();
        Serial.println("[wifi] reconnected");
      }
    }
  }

  // 4. 常驻配置页轮询
  portalServerLoop();

  delay(20);
}
