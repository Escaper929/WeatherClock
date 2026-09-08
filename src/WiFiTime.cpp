// ============================================================================
//  WiFiTime.cpp
// ============================================================================
#include "WiFiTime.h"
#include "BoardPins.h"
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>

static bool synced = false;

bool wifiConnect(const String& ssid, const String& pass, unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > timeoutMs) return false;
    delay(100);
  }
  return true;
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool wifiReconnect(const String& ssid, const String& pass) {
  // 尝试重新协商；若失败则断开重连一次
  if (wifiIsConnected()) return true;
  WiFi.disconnect();
  return wifiConnect(ssid, pass, 15000);
}

static void ntpCallback(struct timeval* tv) {
  (void)tv;
  synced = true;
}

void ntpBegin(const String& tz) {
  sntp_set_time_sync_notification_cb(ntpCallback);  // 注册回调，否则 synced 永远为 false
  if (tz.length() > 0) {
    // 用户选择的 POSIX 时区串（如 "CST-8"），支持夏令时规则
    Serial.printf("[ntp] tz='%s'\n", tz.c_str());
    configTzTime(tz.c_str(), "ntp.aliyun.com", "pool.ntp.org");
  } else {
    // 兼容旧配置：静态偏移时区
    configTime(TZ_GMTOFFSET_SEC, TZ_DSTOFFSET_SEC, "ntp.aliyun.com", "pool.ntp.org");
  }
}

bool timeIsSynced() {
  if (synced) return true;
  // 兜底：直接看系统时间是否已校准（2020+），不依赖回调
  struct tm tmi;
  if (getLocalTime(&tmi, 0) && (tmi.tm_year + 1900) >= 2020) {
    synced = true;
  }
  return synced;
}

unsigned long nowSegments() {
  if (synced) {
    struct tm tm;
    if (getLocalTime(&tm, 0)) {
      return mktime(&tm);
    }
  }
  return 0;
}

// 便于在 setup 里异步校准：注册回调并在后台同步
// 由 main.cpp 调用 settimeofday 之后，通过同步时间戳确认。
// 简单实现：暴露一个同步入口。
void ntpWaitSync(unsigned long timeoutMs) {
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);           // 重置为 1970，避免错误时间
  uint32_t t0 = millis();
  while (!synced && (millis() - t0 < timeoutMs)) {
    delay(50);
    // configTime 已注册 SNTP 回调，轮询 tm_is_valid 判定
    struct tm tmi;
    if (getLocalTime(&tmi, 0) && tmi.tm_year >= 120) {  // 2020+
      synced = true;
    }
  }
}