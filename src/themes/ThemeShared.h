// ============================================================================
//  ThemeShared.h - 各主题共享的显示驱动与绘制原语
//
//  共享原则：只共享"工具"（七段几何、图标笔画、像素字体、周名表），
//  不共享任何布局/颜色决策 —— 布局与配色由各主题文件独立定义。
// ============================================================================
#pragma once
#include <LovyanGFX.hpp>
#include "UiTheme.h"     // ui::rgb565 等基础色板工具（Modern 布局常量也在此，无害）
#include "Theme.h"
#include "BoardPins.h"

// ---------------------------------------------------------------------------
// ST7789 显示驱动（Screen.cpp 定义唯一实例 lcd，全部主题共用同一块屏）
// ---------------------------------------------------------------------------
class WCgfx : public lgfx::LGFX_Device {
public:
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;

  WCgfx(void) {
    { auto cfg = _bus.config();
      cfg.spi_host  = SPI2_HOST;
      cfg.spi_mode  = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.pin_sclk  = PIN_SCK;
      cfg.pin_mosi  = PIN_MOSI;
      cfg.pin_miso  = -1;
      cfg.pin_dc    = PIN_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    { auto cfg = _panel.config();
      cfg.pin_cs     = PIN_CS;
      cfg.pin_rst    = PIN_RST;
      cfg.panel_width  = 240;
      cfg.panel_height = 240;
#ifdef USE_ST7789_INVERT
      cfg.invert = true;
#endif
      cfg.offset_rotation = 0;
      _panel.config(cfg);
      setPanel(&_panel);
    }
  }
};
extern WCgfx lcd;

namespace thm {

// ---------------------------------------------------------------------------
// 周名表（Retro/Minimal 用英文短周名营造排版差异，Modern/Color 用中文）
// ---------------------------------------------------------------------------
extern const char* WEEK_CN[7];
extern const char* WEEK_EN[7];

// 天气图标码 -> 中文兜底（API text 为空时）
const char* weatherTextCN(int code);

// ---------------------------------------------------------------------------
// 七段数码管原语（几何与颜色全部参数化；只画亮段，不写灭段避免 SPI 空写）
// 段映射 bit: a=顶 b=右上 c=右下 d=底 e=左下 f=左上 g=中
// ---------------------------------------------------------------------------
struct SegGeom { int w, h, t, gap; };

void segDigit(int ox, int oy, int n, const SegGeom& g, uint16_t col);
void segMinus(int ox, int oy, const SegGeom& g, uint16_t col);
// 冒号（方形点，复古电子钟语言）：cx 为水平中心，h 为数字高，oy 为数字顶
void segColonSquare(int cx, int oy, int h, bool on, uint16_t col);
// 冒号（圆点，Modern 语言）
void segColonRound(int cx, int oy, int h, bool on, uint16_t col);

// ---------------------------------------------------------------------------
// 剪影天气图标（逻辑边界 40x40，中心 cx,cy）—— 颜色全参数化，
// 供 Modern（柔和多彩剪影）、Retro/Color（单色强调染色）复用同一笔画语言
// bg 为各主题自己的底色（月亮"咬合"效果需要）
// ---------------------------------------------------------------------------
void weatherIconSil(int code, int cx, int cy, uint16_t bg,
                    uint16_t sun, uint16_t moon, uint16_t cloud,
                    uint16_t cloudD, uint16_t rain, uint16_t snow,
                    uint16_t fog, uint16_t bolt);

// ---------------------------------------------------------------------------
// 像素天气图标：20x20 点阵字符画（'#'画点），放大 cell 倍。
// cell=2 时 40x40 与剪影逻辑尺寸一致；cell=1 时 20x20（小尺寸用）
// ---------------------------------------------------------------------------
void weatherIconPix(int code, int cx, int cy, uint16_t col, int cell = 2);

// ---------------------------------------------------------------------------
// 像素点阵数字
//   PX7x10：大号（时钟），7x10 点；PX5x7：小号（温度/行情），5x7 点
//   画一串 "235" / "-5" / "532.60"，返回绘制宽度（像素）；'.' 占 3 列
// ---------------------------------------------------------------------------
int  pixDigit7x10(int ox, int oy, int n, int cell, uint16_t col);
int  pixNum7x10(int ox, int oy, const char* s, int cell, uint16_t col);   // 支持 0-9 与 '-'
int  pixColon7x10(int ox, int oy, int cell, uint16_t col);                // 冒号，返回宽度
int  pixNum7x10Width(const char* s, int cell);
int  pixNum5x7(int ox, int oy, const char* s, int cell, uint16_t col);    // 支持 0-9 . - + %
int  pixNum5x7Width(const char* s, int cell);

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------
// 文字宽度钳制绘制：超过 maxW 时逐字截断尾部并补 '…'（防御超长城市/天气文字）
// 使用当前字体与 textDatum；返回实际绘制宽度
int drawTextClamped(const char* s, int x, int y, int maxW);

}  // namespace thm
