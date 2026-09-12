// ============================================================================
//  Theme.h - 多主题显示系统（数据层 / 主题层分离）
//
//  架构：
//    数据层（main.cpp 持有）：NTP 时间、天气、行情、城市 —— 与 UI 无关
//    主题层（src/themes/*.cpp）：每套主题独立实现布局/字体/颜色/图标，
//         只消费 UiData，不接触任何网络/配置逻辑
//    分发（src/Theme.cpp）：themeTick() 按当前主题把 UiData 交给对应实现
//
//  每套主题硬性要求：在 240x240 内完整呈现
//    时间 + 星期 + 日期 + 地点 + 天气图标 + 天气文字 + 温度 +
//    湿度 + 体感 + 行情(标签/价格/单位/涨跌)
// ============================================================================
#pragma once
#include <Arduino.h>
#include <time.h>
#include "Weather.h"
#include "Quote.h"

// 主题 ID（NVS 只存这个 id，不存样式参数）
enum DisplayTheme : uint8_t {
  THEME_MODERN    = 0,   // 现代极简 + 复古电子钟（黑底暖白，即原版精修 UI）
  THEME_RETRO     = 1,   // 辐射终端（Pip-Boy 磷光绿，废土蒸汽朋克）
  THEME_PIXEL     = 2,   // Classic Platformer（像素横版游戏 HUD，柔和色）
  THEME_WASTELAND = 3,   // 废土终端（琥珀 CRT 工业面板，避难所气象站）
  THEME_MARIO     = 4,   // 马里奥配色平台游戏（高饱和经典色）
};
constexpr int THEME_COUNT = 5;

const char*    themeName(DisplayTheme t);   // "Modern" / "Retro" / ...
const char*    themeDesc(DisplayTheme t);   // 中文一句话描述（网页用）
inline bool    themeIdValid(uint8_t id) { return id < THEME_COUNT; }

// 每帧交给主题的只读数据视图
struct UiData {
  struct tm       dt;            // 本地时间（主循环已按配置时区转换好）
  const char*     city;          // 城市名，可能为空串
  const WeatherData* weather;    // nullptr = 尚无有效数据（主题应留空该区）
  const QuoteData*   quote;      // nullptr = 未启用或暂无数据
};

// 应用主题（启动 / 网页切换时调用）。
// 只是记录目标主题并递增 epoch；实际的全屏重绘由该主题在下一次
// themeTick() 中自行完成（各主题底色不同，由主题自己 fillScreen）。
void themeApply(DisplayTheme t);

DisplayTheme themeCurrent();

// 主题激活轮次号：每次 themeApply / renderStatus 后递增。
// 主题实现用它检测"本轮需全屏重绘 + 重置脏检测状态"。
uint32_t themeEpoch();

// 主循环定时调用（约 500ms 一次）；blinkColon 为冒号闪烁相位。
// 天气/行情数据变化时由主题内部的 key 脏检测自动重绘对应区域。
void themeTick(const UiData& d, bool blinkColon);

// 兼容旧调用点：主题系统激活前用（暂无用途时保留）
void themeInvalidate();   // 递增 epoch，强制下一次 tick 全屏重绘
