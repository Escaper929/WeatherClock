// ============================================================================
//  UiTheme.h - 统一视觉主题（颜色系统 + 布局几何）
//
//  设计语言：现代极简 + 轻微复古电子钟
//    - 纯黑底色，暖白主文字，微暖灰辅助文字（全套冷暖统一，不混蓝调）
//    - 时间与温度共用同一套自绘七段数字（同一视觉语言，大小分级）
//    - 天气组 2×2 网格：左列图标/天气文字，右列温度/湿度体感
//    - 底部行情行由一条发丝线定义为"页脚状态栏"
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
// 颜色系统（暖白 + 微暖灰，黑底低饱和）
// ---------------------------------------------------------------------------
constexpr uint16_t BG      = rgb565(  0,   0,   0);   // 底色：纯黑
constexpr uint16_t INK     = rgb565(234, 228, 216);  // 一级文字（时间/温度）暖白
constexpr uint16_t INK2    = rgb565(162, 164, 156);  // 二级文字（日期/天气/°C）暖浅灰
constexpr uint16_t INK3    = rgb565(110, 112, 118);  // 三级文字（湿度/单位/标签）
constexpr uint16_t HAIRLINE= rgb565( 44,  47,  54);   // 极克制的分隔线
constexpr uint16_t SEG_OFF = rgb565(  0,   0,   0);   // 七段数码管"熄灭"段（纯黑，不显示灭段）

// 天气图标：统一的自然低饱和色（全剪影语言）
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
constexpr int MARGIN = 24;        // 左右统一边距（顶栏/天气右列/页脚共用一套网格）
constexpr int CX     = SCR_W / 2;

// ---------------------------------------------------------------------------
// 布局分区（y 自上而下；垂直节奏 21 / 58 / 60 / 26）
// 顶栏文字 9..21 → 时间 38..96 → 天气组 118..178 → 页脚线 208 → 行情 219..231
// ---------------------------------------------------------------------------
// 顶栏：星期+日期（左，同字号同基线）+ 城市（右）
constexpr int TOP_Y    = 15;              // 文字垂直中线
constexpr int TOP_X_L  = MARGIN;          // 24
constexpr int TOP_X_R  = SCR_W - MARGIN;  // 216
constexpr int TOP_GAP  = 7;               // 星期与日期的间距

// 七段数码管时钟（主视觉，58px）
constexpr int SEG_W      = 32;    // 单个数字宽
constexpr int SEG_H      = 58;    // 单个数字高
constexpr int SEG_T      = 5;     // 笔画粗细
constexpr int SEG_GAP    = 2;     // 笔画断口
constexpr int DIG_GAP    = 6;     // 同组数字间距（时-时 / 分-分）
constexpr int COL_GAP    = 12;    // 冒号占位宽
// 4 位数字总宽：4*32 + 6 + 12 + 6 = 152，水平居中
constexpr int SEG_X0     = (SCR_W - (4 * SEG_W + 2 * DIG_GAP + COL_GAP)) / 2; // 44
constexpr int SEG_Y0     = 38;
constexpr int COLON_CX   = SEG_X0 + 2 * SEG_W + DIG_GAP + COL_GAP / 2;        // 120

// 温度七段（时间同语言的缩小版；数字右缘对齐，°C 作为上标挂在右上）
constexpr int TSEG_W   = 20;
constexpr int TSEG_H   = 36;
constexpr int TSEG_T   = 3;
constexpr int TSEG_GAP = 1;
constexpr int TDIG_GAP = 5;
constexpr int TEMP_XR  = 202;    // 温度数字右缘（°C 延伸至约 216 边距线）
constexpr int TEMP_CY  = 136;    // 数字垂直中心（y 118..154）
constexpr int DEG_DX   = 4;      // °C 相对数字右缘的水平偏移
constexpr int DEG_DY   = 3;      // °C 相对数字顶部的垂直偏移

// 天气组（2×2 网格，无卡片无边框）
// 左列：图标 + 天气文字（同中心 x）；右列：温度 + 湿度/体感（右缘同一对齐线）
constexpr int W_ICON_CX  = 56;   // 左列中心
constexpr int W_ICON_CY  = 138;  // 图标中心（逻辑 40x40）
constexpr int W_TEXT_Y   = 172;  // 左列天气文字中线
constexpr int W_META_XR  = SCR_W - MARGIN;  // 湿度/体感行右对齐线（=216）
constexpr int W_META_Y   = W_TEXT_Y;        // 与天气文字同一基线（2×2 网格下行）

// 页脚状态栏（行情）：发丝线 + 一行弱化信息，左右缘对齐边距网格
constexpr int FOOT_RULE_Y = 208;
constexpr int FOOT_RULE_X = MARGIN;
constexpr int FOOT_RULE_W = SCR_W - 2 * MARGIN;   // 192
constexpr int Q_Y         = 225;                  // 行情文字垂直中线
constexpr int Q_X_L       = MARGIN;
constexpr int Q_X_R       = SCR_W - MARGIN;

// 脏矩形刷新区（互不重叠）
struct Rect { int x, y, w, h; };
constexpr Rect ZONE_TOP     = {  0,   0, 240,  26 };
constexpr Rect ZONE_CLOCK   = { 38,  28, 164,  70 };   // y 28..97
constexpr Rect ZONE_WEATHER = {  0, 100, 240, 104 };   // y 100..203
constexpr Rect ZONE_QUOTE   = {  0, 204, 240,  36 };   // y 204..239

}  // namespace ui
