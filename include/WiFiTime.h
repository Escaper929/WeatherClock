// ============================================================================
//  WiFiTime.h - WiFi 连接与 NTP 时间同步
// ============================================================================
#pragma once
#include <Arduino.h>

// 连接 WiFi（带超时ms），成功返回 true
bool wifiConnect(const String& ssid, const String& pass, unsigned long timeoutMs = 20000);

// 检测 WiFi 是否断开
bool wifiIsConnected();

// 断开并按需重连（返回是否仍在线）
bool wifiReconnect(const String& ssid, const String& pass);

// 启动 NTP 时间同步（异步，需要等待一小段时间后再取时间）
// tz: POSIX 时区串（如 "CST-8"）；留空使用 BoardPins 的静态偏移
void ntpBegin(const String& tz = String());

// 返回已同步的时间戳（秒，北京时间已做偏移）；未同步返回 0
unsigned long nowSegments();

// 时间是否已同步
bool timeIsSynced();