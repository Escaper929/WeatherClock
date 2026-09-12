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
  String qweather_host;         // 和风 API Host（2024 后新账号必需，如 abc123.re.qweatherapi.com；旧账号留空）
  String city_name;             // 城市名，如 "北京"（用于 geoapi 解析）
  String location_id;           // 解析后的 LocationID，如 "101010100"
  float  lat = 0, lon = 0;      // 可选经纬度（若留空用 location_id）
  String timezone;              // POSIX 时区串，如 "CST-8"（北京）；留空用 BoardPins 的静态偏移

  // ---- 行情显示（金价/油价/自定义数据源） ----
  int    quote_mode = 0;        // 0=关闭 1=积存金/金价(Au9999) 2=布伦特原油 3=自定义 URL
  String quote_label;           // 自定义时的显示标签（预设忽略，用内置名）
  String quote_url;             // 自定义数据源完整 URL（https://...）
  String quote_path;            // 自定义 JSON 价格字段路径，如 data.f43 或 price

  // ---- 显示风格（Theme.h 的 DisplayTheme；只存 id，非法值回退 Modern） ----
  uint8_t theme = 0;
};

// 载入配置，返回是否有效
bool loadConfig(AppConfig& cfg);
// 保存 WiFi / API Key / 城市配置（不包含 location_id，portal 保存时单独写）
bool saveConfig(const AppConfig& cfg);
// 单独保存解析出的 location_id（用于城市解析成功后回写）
bool saveLocationId(const String& id);
// 单独保存主题 id（网页即时切换用，不重启）
bool saveTheme(uint8_t themeId);
// 清除全部配置（复位设备）
bool clearConfig();