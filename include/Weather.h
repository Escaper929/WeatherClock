// ============================================================================
//  Weather.h - 和风天气 QWeather 客户端（HTTPS + JSON）
// ============================================================================
#pragma once
#include <Arduino.h>
#include "AppConfig.h"

// 天气数据（当前天气）
struct WeatherData {
  bool  ok = false;          // 本次请求是否成功/有数据
  int   code = 200;          // QWeather 业务状态码
  float temp = 0;            // 当前温度 ℃
  float feels = 0;           // 体感温度 ℃
  int   humidity = 0;        // 相对湿度 %
  int   icon = 0;            // 天气图标代码（100 晴/101 多云/...）
  String text;               // 天气文字，如 "多云"
};

// 通过城市名解析 LocationID（用于配置门户保存时校验城市是否存在）
// 成功返回 true 并回填 id；失败返回 false
bool geoResolveCity(const String& apikey, const String& cityName, String& outId);

// 拉取当前天气。地理位置：优先拿 cfg.lat/lon 非 0 则用经纬度，否则用 location_id。
bool fetchWeather(const AppConfig& cfg, WeatherData& out);

// 天气图标代码 -> 简短英文描述（用于不带中文字库时显示）
const char* weatherTextShort(int iconCode);