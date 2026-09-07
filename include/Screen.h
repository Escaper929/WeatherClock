// ============================================================================
//  Screen.h - ST7789 240x240 屏幕渲染（LovyanGFX）
// ============================================================================
#pragma once
#include <Arduino.h>
#include "Weather.h"

// 初始化屏幕与背光，显示开机画面，返回是否成功
bool screenInit();

// 设置背光亮度 0-255
void screenSetBrightness(uint8_t v);

// 主界面：绘制时钟（时间 + 日期）与天气区
// timeSegs: 北京时间戳（目前由 nowSegments 提供）
// blinkColon: true 时冒号亮起，false 时熄灭（用于闪烁）
void renderClock(unsigned long timeSegs, bool blinkColon);

// 仅刷新天气信息区
void renderWeather(const WeatherData& w);

// 在中间显示临时状态文字（如“连接中…”“无 WiFi”）
void renderStatus(const char* msg, const String& sub = String());