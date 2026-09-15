// ============================================================================
//  AppConfig.h - 持久化配置（NVS / Preferences）
// ============================================================================
#pragma once
#include <Arduino.h>

#define QUOTE_SLOTS 6   // 屏幕底部行情轮播可同时配置的最大数据源数

// ---- 行情轮播单个条目：0=空 1=金价 2=布油 4=沪铜 3=自定义 JSON ----
struct QuoteSlot {
  int    type = 0;   // 见上，0 表示未启用
  String label;      // 自定义时显示标签（预设忽略，用内置名）
  String url;        // 自定义数据源完整 URL（https://...）
  String path;       // 自定义 JSON 价格字段路径，如 data.f43
};

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

  // ---- 行情轮播（屏幕底部一行，多个数据源按顺序自动切换显示） ----
  QuoteSlot quote_slots[QUOTE_SLOTS];
  int       quote_slot_count = 0;   // 实际启用的条数（<= QUOTE_SLOTS）
  int       quote_rotate_s = 6;     // 轮播切换间隔（秒，2..60）

  // ---- 显示风格（Theme.h 的 DisplayTheme；只存 id，非法值回退 Modern） ----
  uint8_t theme = 0;
};

// 行情轮播列表 <-> 字符串（NVS / 表单共用编码）：
//   条目之间用 '\n' 分隔；预设条目仅含类型数字，如 "1"；
//   自定义条目为 "3;;url;;path;;label"。返回的结尾不带换行。
String serializeQuoteSlots(const AppConfig& cfg);
// 解析列表字符串并填充 cfg.quote_slots / quote_slot_count；失败返回 false（保留已有字段）
bool   parseQuoteSlots(const String& s, AppConfig& cfg);

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