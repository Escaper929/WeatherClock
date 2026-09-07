// ============================================================================
//  main.cpp - 天气时钟主程序
//  流程：
//    setup:  初始化屏幕 -> 读取配置 -> 若未配置或按住 BOOT 上电则进配置门户
//            -> 连接 WiFi -> 同步 NTP 时间 -> 解析城市 -> 拉取天气
//    loop:   刷新时钟、定期拉取天气、断线重连
// ============================================================================
#include "BoardPins.h"
#include "AppConfig.h"
#include "WiFiTime.h"
#include "Weather.h"
#include "Screen.h"
#include "Portal.h"
#include <Arduino.h>
#include <WiFi.h>

static AppConfig  cfg;
static WeatherData weather;
static bool       weatherValid = false;
static unsigned   lastFetch    = 0;
static unsigned   lastBlink    = 0;
static bool       blinkOn      = true;
static unsigned   lastReconnect= 0;

static void fetchWeatherSafe() {
  WeatherData w;
  if (fetchWeather(cfg, w)) {
    weather = w;
    weatherValid = true;
  } else if (!cfg.location_id.length() && cfg.city_name.length()) {
    Serial.println("[weather] 定位失败，尝试按经纬度或重新解析城市");
  }
  lastFetch = millis();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[main] WeatherClock starting");

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

  if (!screenInit()) {
    Serial.println("[main] 屏幕初始化失败！");
  } else {
    screenSetBrightness(SCREEN_BRIGHTNESS);
    renderStatus("WeatherClock", "ESP32-C3");
  }

  bool configured = loadConfig(cfg);

  // 按住 BOOT 上电 -> 强制进配置门户
  bool forcePortal = (digitalRead(PIN_BOOT_BTN) == LOW);
  if (forcePortal) Serial.println("[main] BOOT 按键已按下，进入配置门户");

  if (!configured || forcePortal) {
    Serial.println("[main] 需要配置，进入配置门户…");
    portalEnter(cfg);   // 阻塞，直到保存后重启
  }

  // 连接 WiFi
  renderStatus("Connecting WiFi...");
  Serial.printf("[wifi] SSID=%s\n", cfg.wifi_ssid.c_str());
  if (!wifiConnect(cfg.wifi_ssid, cfg.wifi_pass, 20000)) {
    Serial.println("[wifi] 连接失败，稍后重试");
  } else {
    // 启动常驻配置页：可通过 http://<设备IP> 或 http://weatherclock.local 访问
    portalServerBegin(cfg);
    renderStatus("Ready", "http://" + WiFi.localIP().toString());
    delay(2500);
  }

  // NTP 时间
  ntpBegin();
  // 等待 NTP 同步（超时 20s，期间不阻塞 UI，先只尝试一次）
  {
    unsigned t0 = millis();
    while (!timeIsSynced() && (millis() - t0 < 20000)) {
      // 在这里渲染“同步中”
      renderStatus("Syncing time...");
      wifiReconnect(cfg.wifi_ssid, cfg.wifi_pass);
      delay(200);
    }
  }

  // 城市 -> LocationID（仅当第一次配置且未填经纬度时解析）
  if (cfg.location_id.length() == 0 && cfg.city_name.length() > 0 &&
      (cfg.lat == 0 && cfg.lon == 0)) {
    renderStatus("Resolving city...");
    String id;
    if (geoResolveCity(cfg.qweather_key, cfg.city_name, id)) {
      cfg.location_id = id;
      saveLocationId(id);
      Serial.printf("[geo] city=%s -> id=%s\n", cfg.city_name.c_str(), id.c_str());
    } else {
      Serial.println("[geo] 城市解析失败，天气可能不可用");
    }
  }

  // 首次拉取天气
  fetchWeatherSafe();

  renderClock(nowSegments(), true);
}

void loop() {
  unsigned now = millis();

  // 1. 时钟：每 500ms 切换冒号状态并刷新
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
        Serial.println("[wifi] 已重连");
      }
    }
  }

  // 4. 常驻配置页轮询
  portalServerLoop();

  delay(20);
}