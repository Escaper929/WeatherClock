// ============================================================================
//  AppConfig.h - 持久化配置（NVS / Preferences）
// ============================================================================
#pragma once
#include <Arduino.h>

// 远程天气配置
struct AppConfig {
  bool valid = false;           // 是否已配置（已成功连接过天气服务）
  String wifi_ssid;
  String wifi_pass;
  String qweather_key;          // 和风天气 API Key（免费版 devapi）
  String city_name;             // 城市名，如 "北京"（用于 geoapi 解析）
  String location_id;           // 解析后的 LocationID，如 "101010100"
  float  lat = 0, lon = 0;      // 可选经纬度（若留空用 location_id）
};

// 载入配置，返回是否有效
bool loadConfig(AppConfig& cfg);
// 保存 WiFi / API Key / 城市配置（不包含 location_id，portal 保存时单独写）
bool saveConfig(const AppConfig& cfg);
// 单独保存解析出的 location_id（用于城市解析成功后回写）
bool saveLocationId(const String& id);
// 清除全部配置（复位设备）
bool clearConfig();