// ============================================================================
//  UiTheme.h - 统一视觉主题（颜色系统 + 布局几何）
//
//  设计语言：现代极简 + 轻微复古电子钟
//    - 近黑的深蓝灰底色，暖白主文字，柔和蓝灰辅助文字
//    - 全界面只有一个暖色重点（太阳/闪电），其余信息靠明度与字号分层
//    - 温度不再使用红/蓝告警色；行情涨跌使用低饱和陶土/青灰
//
//  所有 Screen.cpp 的颜色、坐标、分区尺寸都取自此处，
//  后续微调 UI 只改本文件即可，不必动绘图逻辑。
// ============================================================================
#pragma once
#include <Arduino.h>

namespace ui {

// 888 RGB -> RGB565（编译期常量）
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                    ((uint16_t)(g & 0xFC) << 3) |
                    (uint16_t)(b >> 3));
}

// ---------------------------------------------------------------------------
// 颜色系统（深色现代桌面设备风格）
// ---------------------------------------------------------------------------
constexpr uint16_t BG      = rgb565(  0,   0,   0);   // 底色：纯黑
constexpr uint16_t INK     = rgb565(234, 228, 216);  // 一级文字（时间/温度）暖白
constexpr uint16_t INK2    = rgb565(150, 157, 168);  // 二级文字（日期/天气/标签）
constexpr uint16_t INK3    = rgb565( 96, 103, 114);  // 三级文字（体感/单位）
constexpr uint16_t HAIRLINE= rgb565( 42,  46,  54);  // 极克制的分隔线
constexpr uint16_t SEG_OFF = rgb565(  0,   0,   0);   // 七段数码管"熄灭"段（纯黑，不显示灭段）

// 天气图标：统一的自然低饱和色
constexpr uint16_t SUN     = rgb565(214, 170,  96);  // 唯一暖色重点：晴/闪电
constexpr uint16_t MOON    = rgb565(198, 188, 160);  // 夜间晴：暗暖灰新月
constexpr uint16_t CLOUD   = rgb565(166, 174, 188);  // 云：柔和蓝灰
constexpr uint16_t CLOUD_D = rgb565(112, 120, 134);  // 阴：后置暗云
constexpr uint16_t RAIN    = rgb565(124, 152, 184);  // 雨：雾钢蓝
constexpr uint16_t SNOW    = rgb565(196, 206, 218);  // 雪：冷浅灰
constexpr uint16_t FOG     = INK3;                   // 雾：与三级文字同灰

// 行情涨跌（低饱和，国内习惯红涨绿跌但不刺眼）
constexpr uint16_t UP      = rgb565(198, 134, 112);  // 涨：陶土
constexpr uint16_t DOWN    = rgb565(116, 156, 148);  // 跌：青灰绿

// ---------------------------------------------------------------------------
// 屏幕与边距
// ---------------------------------------------------------------------------
constexpr int SCR_W  = 240;
constexpr int SCR_H  = 240;
constexpr int MARGIN = 24;        // 左右统一边距
constexpr int CX     = SCR_W / 2;

// ---------------------------------------------------------------------------
// 布局分区（y 坐标自上而下，各刷新区互不重叠，见 Screen.cpp）
// ---------------------------------------------------------------------------
// 顶栏（星期/日期 左，城市 右）
constexpr int TOP_Y      = 14;
constexpr int TOP_X_L    = MARGIN;
constexpr int TOP_X_R    = SCR_W - MARGIN;

// 七段数码管时钟
constexpr int SEG_W      = 32;    // 单个数字宽
constexpr int SEG_H      = 58;    // 单个数字高
constexpr int SEG_T      = 5;     // 笔画粗细
constexpr int SEG_GAP    = 2;     // 笔画断口
constexpr int DIG_GAP    = 6;     // 同组数字间距（时-时 / 分-分）
constexpr int COL_GAP    = 12;    // 冒号占位宽
// 4 位数字总宽：4*32 + 6 + 12 + 6 = 152，水平居中
constexpr int SEG_X0     = (SCR_W - (4 * SEG_W + 2 * DIG_GAP + COL_GAP)) / 2; // 44
constexpr int SEG_Y0     = 31;
constexpr int COLON_CX   = SEG_X0 + 2 * SEG_W + DIG_GAP + COL_GAP / 2;       // 120

// 分隔线（短、细、居中，时钟与天气之间唯一的分割）
constexpr int RULE_Y     = 99;
constexpr int RULE_W     = 48;
constexpr int RULE_X     = (SCR_W - RULE_W) / 2;

// 天气组（无卡片、无边框，与时钟同属一个排版面）
constexpr int W_ICON_CX  = 56;    // 左列：图标中心
constexpr int W_ICON_CY  = 146;
constexpr int W_TEXT_Y   = 181;   // 左列：天气文字
constexpr int W_TEMP_Y   = 147;   // 右列：温度（中线）
constexpr int W_TEMP_XR  = 194;   // 右列：数字右缘
constexpr int W_META_X   = 104;   // 右列：湿度/体感起始
constexpr int W_META_Y   = W_TEXT_Y;

// 行情页脚（四级信息，整行弱化）
constexpr int Q_Y        = 222;
constexpr int Q_X_L      = MARGIN;
constexpr int Q_X_R      = SCR_W - MARGIN;

// 脏矩形刷新区（互不重叠）
struct Rect { int x, y, w, h; };
constexpr Rect ZONE_TOP     = {  0,   0, 240,  26 };
constexpr Rect ZONE_CLOCK   = { 38,  27, 164,  66 };   // y 27..93
constexpr Rect ZONE_WEATHER = {  0, 106, 240,  96 };   // y 106..202
constexpr Rect ZONE_QUOTE   = {  0, 202, 240,  38 };   // y 202..240

}  // namespace ui
