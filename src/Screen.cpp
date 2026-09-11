// ============================================================================
//  Screen.cpp - ST7789 240x240 渲染实现
//
//  UI 设计语言：现代极简 + 轻微复古电子钟
//    顶栏   星期 / 日期（左）  城市（右）          三级信息
//    主区   自绘七段数码管时钟（58px，绝对主角）   一级信息
//    分割   一条 48px 细发丝线
//    天气   统一剪影图标 + 天气文字 ｜ 温度 + 湿度/体感（无卡片无边框）
//    页脚   行情一行，整体弱化为四级信息
//
//  颜色与几何全部集中在 include/UiTheme.h。
//  渲染策略：分脏矩形擦除-重绘（不 fillScreen，区域小，无可见闪烁）。
//  字体：LovyanGFX 内置 FreeMonoBold（温度/行情数字）+ efontCN（中文），
//        时间不使用任何字库——纯代码绘制七段数码管，零 flash 开销、像素级统一。
// ============================================================================
#include "Screen.h"
#include "BoardPins.h"
#include "UiTheme.h"
#include <LovyanGFX.hpp>
#include <math.h>
#include <time.h>
#include <string.h>

using namespace ui;

static void drawWeatherIcon(int code, int cx, int cy);

// ---------------------------------------------------------------------------
// 显示驱动
// ---------------------------------------------------------------------------
class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;

  LGFX(void) {
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
static LGFX lcd;

static void backlightSetup() {
#if (defined(PIN_BL) && PIN_BL >= 0)
  ledcSetup(0, 12000, 8);
  ledcAttachPin(PIN_BL, 0);
  ledcWrite(0, SCREEN_BRIGHTNESS);
#endif
}

// ---------------------------------------------------------------------------
// 字体选择（统一只允许这几种，不再散落各处）
// ---------------------------------------------------------------------------
// efontCN(U8g2font) 与 FreeMono(GFXfont) 均实现 IFont，统一用基类指针
static const lgfx::IFont* const FONT_CN    = &fonts::efontCN_12;          // 中文/辅助信息
static const lgfx::IFont* const FONT_NUM   = &fonts::FreeMonoBold24pt7b;  // 温度（二级）
static const lgfx::IFont* const FONT_SMALL = &fonts::FreeMonoBold9pt7b;   // 行情页脚

static const char* WEEK_CN[] = {"周日","周一","周二","周三","周四","周五","周六"};

static String gCityName;
static uint16_t gLastTopKey = 0xFFFF;         // 顶栏脏检测
static int      gLastDigit[4] = {-1,-1,-1,-1}; // 四位数字逐位脏检测
static bool     gLastBlinkOn = false;         // 冒号上次状态

bool screenInit() {
  lcd.init();
  lcd.display();
  backlightSetup();
  lcd.setRotation(0);
  lcd.fillScreen(BG);
  gLastTopKey = 0xFFFF;
  for (int i = 0; i < 4; i++) gLastDigit[i] = -1;
  gLastBlinkOn = false;
  return true;
}

void screenSetBrightness(uint8_t v) {
#if (defined(PIN_BL) && PIN_BL >= 0)
  ledcWrite(0, v);
#endif
}

void screenSetCity(const String& city) {
  gCityName = city;
}

// ---------------------------------------------------------------------------
// 七段数码管时钟（自绘，只画亮段，不画灭段避免 SPI 空写闪烁）
// ---------------------------------------------------------------------------
// 段映射 bit: a=顶 b=右上 c=右下 d=底 e=左下 f=左上 g=中
static const uint8_t SEG_FONT[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static void segBar(int x, int y, int w, int h, uint16_t col) {
  lcd.fillRoundRect(x, y, w, h, 2, col);
}

static void drawSegDigit(int ox, int oy, int n) {
  const int T  = SEG_T;
  const int hw = SEG_W - 2 * (T - 1);
  const int hx = ox + T - 1;
  const int vl = SEG_H / 2 - T - 2 * SEG_GAP;
  uint8_t m = SEG_FONT[n];

  // 只画亮段，灭段不画（背景已是纯黑）
  if (m & 0x01) segBar(hx, oy, hw, T, INK);                          // a
  if (m & 0x02) segBar(ox + SEG_W - T, oy + T + SEG_GAP, T, vl, INK); // b
  if (m & 0x04) segBar(ox + SEG_W - T, oy + SEG_H - T - SEG_GAP - vl, T, vl, INK); // c
  if (m & 0x08) segBar(hx, oy + SEG_H - T, hw, T, INK);            // d
  if (m & 0x10) segBar(ox, oy + SEG_H - T - SEG_GAP - vl, T, vl, INK); // e
  if (m & 0x20) segBar(ox, oy + T + SEG_GAP, T, vl, INK);          // f
  if (m & 0x40) segBar(hx, oy + (SEG_H - T) / 2, hw, T, INK);     // g
}

static void drawColon(int cx, int oy, bool on) {
  if (!on) return;  // 灭时不画，避免 SPI 空写
  lcd.fillRoundRect(cx - 2, oy + 18, 4, 4, 1, INK);
  lcd.fillRoundRect(cx - 2, oy + SEG_H - 22, 4, 4, 1, INK);
}

void renderClock(unsigned long t, bool blinkColon) {
  struct tm tm;
  if (t == 0 || !localtime_r((time_t*)&t, &tm)) return;

  int hh = tm.tm_hour, mm = tm.tm_min;

  // 顶栏：星期/日期（左）+ 城市（右），分钟变化才重绘；分隔线随之常驻
  uint16_t newKey = (uint16_t)((tm.tm_wday << 8) | mm);
  if (newKey != gLastTopKey) {
    gLastTopKey = newKey;
    lcd.fillRect(ZONE_TOP.x, ZONE_TOP.y, ZONE_TOP.w, ZONE_TOP.h, BG);

    char dat[24];
    snprintf(dat, sizeof dat, "%s  %02d/%02d", WEEK_CN[tm.tm_wday],
             tm.tm_mon + 1, tm.tm_mday);
    lcd.setTextDatum(middle_left);
    lcd.setFont(FONT_CN);
    lcd.setTextSize(1);
    lcd.setTextColor(INK2, BG);
    lcd.drawString(dat, TOP_X_L, TOP_Y);

    if (gCityName.length()) {
      lcd.setTextDatum(middle_right);
      lcd.drawString(gCityName.c_str(), TOP_X_R, TOP_Y);
    }

    lcd.fillRect(RULE_X, RULE_Y, RULE_W, 1, HAIRLINE);
  }

  // 时钟区：逐位脏检测——只有真正变化的那一位才擦除重绘（32x58 小区域），
  // 其余时间每 500ms 仅刷新冒号两个小圆点，彻底消除整区擦除造成的闪烁
  int xs[4] = { SEG_X0,
                SEG_X0 + SEG_W + DIG_GAP,
                COLON_CX + COL_GAP / 2,
                COLON_CX + COL_GAP / 2 + SEG_W + DIG_GAP };
  int digits[4] = { hh / 10, hh % 10, mm / 10, mm % 10 };
  for (int i = 0; i < 4; i++) {
    if (digits[i] != gLastDigit[i]) {
      lcd.fillRect(xs[i], SEG_Y0, SEG_W, SEG_H, BG);  // 先清旧字亮段
      drawSegDigit(xs[i], SEG_Y0, digits[i]);
      gLastDigit[i] = digits[i];
    }
  }
  if (blinkColon != gLastBlinkOn) {
    lcd.fillRect(COLON_CX - 6, SEG_Y0 + 14, 12, SEG_H - 28, BG);
    drawColon(COLON_CX, SEG_Y0, blinkColon);
    gLastBlinkOn = blinkColon;
  }
}

// ---------------------------------------------------------------------------
// 天气图标：统一的“柔和剪影”语言，线/块尺度一致，颜色取自 UiTheme
// 逻辑边界 40x40（多云/阴略宽），中心 (cx, cy)
// ---------------------------------------------------------------------------
static void drawCloud(int cx, int cy, uint16_t col) {
  lcd.fillCircle(cx - 12, cy + 2, 8, col);
  lcd.fillCircle(cx - 3,  cy - 5, 10, col);
  lcd.fillCircle(cx + 8,  cy + 1, 8, col);
  lcd.fillRect(cx - 19, cy + 1, 38, 9, col);
}

// 线框太阳：2px 圆环 + 8 道 2px 射线
static void drawSunShape(int cx, int cy, int r, uint16_t col) {
  lcd.drawCircle(cx, cy, r, col);
  lcd.drawCircle(cx, cy, r - 1, col);
  for (int i = 0; i < 8; i++) {
    double a = i * M_PI / 4.0;
    int ca = (int)round(cos(a)), sa = (int)round(sin(a));
    for (int o = 0; o <= 1; o++) {  // 法向偏移 1px 做出 2px 粗细
      int ox = (fabs(ca) < 0.1f) ? 0 : (sa >= 0 ? o : -o);
      int oy = (fabs(sa) < 0.1f) ? 0 : (ca >= 0 ? -o : o);
      lcd.drawLine(cx + (r + 3) * ca + ox, cy + (r + 3) * sa + oy,
                   cx + (r + 7) * ca + ox, cy + (r + 7) * sa + oy, col);
    }
  }
}

// 新月：暖灰圆再用底色咬掉一块
static void drawMoon(int cx, int cy, int r) {
  lcd.fillCircle(cx, cy, r, MOON);
  lcd.fillCircle(cx + 4, cy - 3, r - 1, BG);
}

static void drawRainDrops(int cx, int cy, int n, int step) {
  for (int i = 0; i < n; i++) {
    int x = cx + (i - (n - 1) / 2) * step;
    int y = cy + 7;
    lcd.drawLine(x, y, x - 3, y + 8, RAIN);
    lcd.drawLine(x + 1, y, x - 2, y + 8, RAIN);  // 2px 粗
  }
}

static void drawSnowStar(int x, int y) {
  lcd.drawLine(x, y - 3, x, y + 3, SNOW);
  lcd.drawLine(x - 3, y, x + 3, y, SNOW);
  lcd.drawLine(x - 2, y - 2, x + 2, y + 2, SNOW);
  lcd.drawLine(x - 2, y + 2, x + 2, y - 2, SNOW);
}

static void drawWeatherIcon(int code, int cx, int cy) {
  bool night = (code >= 150 && code < 200);

  if (code == 100) {
    drawSunShape(cx, cy, 11, SUN);
  } else if (code == 150) {
    drawMoon(cx, cy, 11);
  } else if (code == 101 || code == 102 || code == 103 ||
             code == 151 || code == 152 || code == 153) {
    // 多云：日月从云后探出
    if (night) drawMoon(cx - 9, cy - 10, 9);
    else       drawSunShape(cx - 9, cy - 10, 7, SUN);
    drawCloud(cx + 5, cy + 5, CLOUD);
  } else if (code == 104 || code == 154) {
    // 阴：两层暗云
    drawCloud(cx - 5, cy - 4, CLOUD_D);
    drawCloud(cx + 4, cy + 5, CLOUD);
  } else if (code == 302 || code == 303 || code == 304) {
    // 雷阵雨（必须在普通雨之前判断）
    drawCloud(cx, cy - 8, CLOUD);
    lcd.fillTriangle(cx, cy - 1, cx + 8, cy - 1, cx - 1, cy + 11, SUN);
    lcd.fillTriangle(cx + 8, cy - 1, cx + 1, cy + 10, cx - 1, cy + 11, SUN);
    lcd.fillTriangle(cx + 6, cy + 10, cx - 2, cy + 23, cx + 3, cy + 11, SUN);
  } else if (code == 404 || code == 405) {
    // 雨夹雪
    drawCloud(cx, cy - 8, CLOUD);
    drawRainDrops(cx - 10, cy, 1, 10);
    drawSnowStar(cx, cy + 12);
    drawRainDrops(cx + 10, cy, 1, 10);
  } else if (code >= 300 && code < 400) {
    drawCloud(cx, cy - 6, CLOUD);
    drawRainDrops(cx, cy, 3, 10);
  } else if (code >= 400 && code < 500) {
    drawCloud(cx, cy - 8, CLOUD);
    drawSnowStar(cx - 9, cy + 12);
    drawSnowStar(cx, cy + 12);
    drawSnowStar(cx + 9, cy + 12);
  } else if (code >= 500 && code < 600) {
    // 雾/霾：三条圆角横线
    lcd.fillRoundRect(cx - 17, cy - 9, 34, 3, 1, FOG);
    lcd.fillRoundRect(cx - 12, cy - 1, 24, 3, 1, FOG);
    lcd.fillRoundRect(cx - 16, cy + 7, 32, 3, 1, FOG);
  } else {
    drawCloud(cx, cy, CLOUD);
  }
}

// 天气图标码 -> 中文（API 未返回 text 时的兜底）
static const char* weatherTextCN(int code) {
  if (code == 100 || code == 150) return "晴";
  if (code == 101 || code == 151) return "多云";
  if (code == 102 || code == 152) return "少云";
  if (code == 103 || code == 153) return "晴间多云";
  if (code == 104 || code == 154) return "阴";
  if (code == 300) return "阵雨";
  if (code == 301) return "强阵雨";
  if (code == 302) return "雷阵雨";
  if (code == 303) return "强雷阵雨";
  if (code == 304) return "雷阵雨伴冰雹";
  if (code == 305) return "小雨";
  if (code == 306) return "中雨";
  if (code == 307) return "大雨";
  if (code == 308) return "极端降雨";
  if (code == 309) return "毛毛雨";
  if (code == 310) return "暴雨";
  if (code == 311) return "大暴雨";
  if (code == 312) return "特大暴雨";
  if (code == 313) return "冻雨";
  if (code == 314) return "小到中雨";
  if (code == 315) return "中到大雨";
  if (code == 316) return "大到暴雨";
  if (code >= 300 && code < 400) return "雨";
  if (code == 400) return "小雪";
  if (code == 401) return "中雪";
  if (code == 402) return "大雪";
  if (code == 403) return "暴雪";
  if (code == 404 || code == 405) return "雨夹雪";
  if (code >= 400 && code < 500) return "雪";
  if (code == 500) return "薄雾";
  if (code == 501 || code == 509 || code == 514) return "雾";
  if (code == 502 || code == 511 || code == 512 || code == 513) return "霾";
  if (code == 503) return "扬沙";
  if (code == 504) return "浮尘";
  if (code == 507 || code == 508) return "沙尘暴";
  if (code >= 500 && code < 600) return "雾";
  return "天气";
}

// ---------------------------------------------------------------------------
// 天气组：无卡片。左列图标+天气文字，右列温度+湿度/体感，两行基线对齐
// ---------------------------------------------------------------------------
void renderWeather(const WeatherData& w) {
  lcd.fillRect(ZONE_WEATHER.x, ZONE_WEATHER.y, ZONE_WEATHER.w, ZONE_WEATHER.h, BG);

  // 左列
  drawWeatherIcon(w.icon, W_ICON_CX, W_ICON_CY);
  const char* desc = w.text.length() ? w.text.c_str() : weatherTextCN(w.icon);
  lcd.setFont(FONT_CN);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(INK2, BG);
  lcd.drawString(desc, W_ICON_CX, W_TEXT_Y);

  // 右列：温度（统一暖白色，不再按冷热变红/蓝）
  char tbuf[8];
  snprintf(tbuf, sizeof tbuf, "%d", (int)(w.temp + 0.5f));
  lcd.setFont(FONT_NUM);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_right);
  lcd.setTextColor(INK, BG);
  lcd.drawString(tbuf, W_TEMP_XR, W_TEMP_Y);
  // °C 上标，二级色
  lcd.setFont(FONT_CN);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(INK2, BG);
  lcd.drawString("\xC2\xB0""C", W_TEMP_XR + 2, W_TEMP_Y - 15);

  // 湿度 / 体感：三级信息一行 quiet 排版
  char mbuf[40];
  snprintf(mbuf, sizeof mbuf, "湿度 %d%% / 体感 %d\xC2\xB0",
           w.humidity, (int)(w.feels + 0.5f));
  lcd.setFont(FONT_CN);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(INK3, BG);
  lcd.drawString(mbuf, W_META_X, W_META_Y);
}

// ---------------------------------------------------------------------------
// 行情页脚：四级信息。标签/单位三级灰，价格二级，涨跌低饱和陶土/青灰
// ---------------------------------------------------------------------------
void renderQuote(const QuoteData& q) {
  lcd.fillRect(ZONE_QUOTE.x, ZONE_QUOTE.y, ZONE_QUOTE.w, ZONE_QUOTE.h, BG);
  if (!q.ok) return;

  // 涨跌幅文本（先确定，便于给左段预留右侧空间）
  bool hasPct = (q.changePct > 0.005f || q.changePct < -0.005f);
  char cbuf[16];
  int pctW = 0;
  if (hasPct) {
    snprintf(cbuf, sizeof cbuf, "%+.2f%%", q.changePct);
    lcd.setFont(FONT_SMALL);
    pctW = lcd.textWidth(cbuf);
  }

  // 左段：标签 + 价格 + 单位。各部分文字宽度固定，间隙在 [2,6] 间自适应，
  // 保证单位右缘既不越过屏幕，也不会被右侧涨跌幅盖住
  char pbuf[24];
  snprintf(pbuf, sizeof pbuf, "%.*f", q.decimals, q.price);

  lcd.setFont(FONT_CN);
  int labelW = q.label.length() ? lcd.textWidth(q.label) : 0;
  int unitW  = q.unit.length()  ? lcd.textWidth(q.unit)  : 0;
  lcd.setFont(FONT_SMALL);
  int priceW = lcd.textWidth(pbuf);

  int limit = hasPct ? (Q_X_R - pctW - 10) : Q_X_R;  // 左段允许的右缘
  int gaps  = (labelW > 0 ? 1 : 0) + (unitW > 0 ? 1 : 0);
  int gap = 6;
  if (gaps > 0) {
    int avail = (limit - Q_X_L) - (labelW + priceW + unitW);
    gap = constrain(avail / gaps, 2, 6);  // 空间不足自动收紧，最小 2px
  }

  int x = Q_X_L;
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_left);

  // 标签
  if (labelW > 0) {
    lcd.setFont(FONT_CN);
    lcd.setTextColor(INK3, BG);
    lcd.drawString(q.label, x, Q_Y);
    x += labelW + gap;
  }

  // 价格
  lcd.setFont(FONT_SMALL);
  lcd.setTextColor(INK2, BG);
  lcd.drawString(pbuf, x, Q_Y + 1);
  x += priceW + (unitW > 0 ? gap : 0);

  // 单位
  if (unitW > 0) {
    lcd.setFont(FONT_CN);
    lcd.setTextColor(INK3, BG);
    lcd.drawString(q.unit, x, Q_Y);
  }

  // 涨跌幅（+/- 号本身区分方向，颜色仅作低饱和提示）
  if (hasPct) {
    lcd.setFont(FONT_SMALL);
    lcd.setTextColor(q.changePct > 0 ? UP : DOWN, BG);
    lcd.setTextDatum(middle_right);
    lcd.drawString(cbuf, Q_X_R, Q_Y + 1);
  }
}

// ---------------------------------------------------------------------------
// 状态页：开机 / 等待 / 错误。内部 fillScreen，单独使用
// ---------------------------------------------------------------------------
void renderStatus(const char* msg, const String& sub) {
  lcd.fillScreen(BG);
  lcd.setFont(FONT_CN);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(INK, BG);
  lcd.drawString(msg, 120, 112);
  if (sub.length()) {
    lcd.setTextColor(INK3, BG);
    lcd.drawString(sub.c_str(), 120, 140);
  }
  gLastTopKey = 0xFFFF;     // 下次 renderClock 重画顶栏与分隔线
  for (int i = 0; i < 4; i++) gLastDigit[i] = -1;  // 重画四位数字
  gLastBlinkOn = false;     // 重置冒号状态，强制首次全绘
}
