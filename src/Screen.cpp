// ============================================================================
//  Screen.cpp - ST7789 240x240 渲染实现
//  字体：LovyanGFX 内置 efontCN 全字库中文 + 矢量七段数码管时钟
//  渲染策略：擦除-重绘（每次只刷新变化区域，避免 fillScreen 闪烁）
// ============================================================================
#include "Screen.h"
#include "BoardPins.h"
#include <LovyanGFX.hpp>
#include <math.h>
#include <time.h>
#include <string.h>

static void drawWeatherIcon(int code, int cx, int cy);

// ---- 颜色（RGB565） ----
static const uint16_t COL_BG      = 0x0107;
static const uint16_t COL_PANEL   = 0x10CB;
static const uint16_t COL_CYAN    = 0x07FF;
static const uint16_t COL_WHITE   = 0xFFFF;
static const uint16_t COL_DIM     = 0x8C51;
static const uint16_t COL_ORANGE  = 0xFD20;
static const uint16_t COL_BLUE    = 0x5CD7;
static const uint16_t COL_PANELBG = 0x0849;
static const uint16_t COL_PANELBD = 0x1A5D;
static const uint16_t COL_RED     = 0xF800;

static String gCityName;
static uint16_t gLastRenderMinute = 0xFFFF;  // 上次渲染的分钟，用来顶栏脏检测
static bool     gWeatherPanelDrawn = false;  // 天气卡片是否已画过

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

static const char* WEEK_CN[] = {"周日","周一","周二","周三","周四","周五","周六"};

bool screenInit() {
  lcd.init();
  lcd.display();
  backlightSetup();
  lcd.setRotation(0);
  lcd.fillScreen(COL_BG);
  gLastRenderMinute = 0xFFFF;
  gWeatherPanelDrawn = false;
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

void renderClock(unsigned long t, bool blinkColon) {
  struct tm tm;
  if (t == 0 || !localtime_r((time_t*)&t, &tm)) return;

  int hh = tm.tm_hour, mm = tm.tm_min;

  // 顶栏只在分钟变化时重绘（擦除区只到 y=88，避让下方 y=90 的行情行）
  uint16_t newKey = (uint16_t)((tm.tm_wday << 8) | mm);
  if (newKey != gLastRenderMinute) {
    gLastRenderMinute = newKey;
    lcd.fillRect(0, 0, 240, 88, COL_BG);
    char dat[64];
    String city = gCityName.length() ? gCityName : "WeatherClock";
    snprintf(dat, sizeof dat, "%s  %s  %d月%d日", city.c_str(), WEEK_CN[tm.tm_wday],
             tm.tm_mon + 1, tm.tm_mday);
    lcd.setTextDatum(middle_center);
    lcd.setFont(&fonts::efontCN_12);
    lcd.setTextColor(COL_DIM, COL_BG);
    lcd.drawString(dat, 120, 12);
  }

  // 时钟区每次都擦一下（每 500ms，区域小不闪）
  lcd.fillRect(10, 30, 220, 56, COL_BG);

  // FreeMonoBold18pt7b 放大 2 倍（约 36px 高，5 字符约 180px 宽，居中留边距）
  lcd.setFont(&fonts::FreeMonoBold18pt7b);
  lcd.setTextSize(2);
  lcd.setTextColor(COL_WHITE, COL_BG);
  lcd.setTextDatum(middle_center);
  char buf[8];
  snprintf(buf, sizeof buf, "%02d%c%02d", hh, blinkColon ? ':' : ' ', mm);
  lcd.drawString(buf, 120, 58);
}

// ============================================================
// 天气图标
// ============================================================
static void drawSun(int cx, int cy, int r) {
  lcd.drawCircle(cx, cy, r, COL_ORANGE);
  lcd.fillCircle(cx, cy, r - 3, COL_ORANGE);
  for (int i = 0; i < 8; i++) {
    double a = i * M_PI / 4.0;
    int x1 = cx + (int)((r + 3) * cos(a));
    int y1 = cy + (int)((r + 3) * sin(a));
    int x2 = cx + (int)((r + 8) * cos(a));
    int y2 = cy + (int)((r + 8) * sin(a));
    lcd.drawLine(x1, y1, x2, y2, COL_ORANGE);
  }
}

static void drawCloudShape(int cx, int cy, uint16_t col = COL_WHITE) {
  lcd.fillCircle(cx - 14, cy, 8, col);
  lcd.fillCircle(cx, cy - 6, 10, col);
  lcd.fillCircle(cx + 14, cy, 8, col);
  lcd.fillRect(cx - 20, cy, 40, 8, col);
}

static void drawWeatherIcon(int code, int cx, int cy) {
  if (code == 100 || code == 150) {
    drawSun(cx, cy, 12);
  } else if (code == 101 || code == 102 || code == 103) {
    drawSun(cx - 8, cy - 6, 9);
    drawCloudShape(cx + 6, cy + 6, 0xFFFF);
  } else if (code >= 300 && code <= 399) {
    drawCloudShape(cx, cy - 8);
    for (int i = -1; i <= 1; i++) {
      int dx = i * 10;
      lcd.fillRect(cx + dx - 2, cy + 8, 2, 8, COL_BLUE);
    }
  } else if (code == 302 || code == 312 || code >= 313) {
    drawCloudShape(cx, cy - 10);
    lcd.fillTriangle(cx - 2, cy - 2, cx + 6, cy - 8, cx - 12, cy + 10, COL_ORANGE);
  } else if (code >= 400 && code <= 499) {
    drawCloudShape(cx, cy - 8);
    for (int i = -1; i <= 1; i++) {
      lcd.drawCircle(cx + i * 8, cy + 10, 3, COL_WHITE);
    }
  } else if (code >= 500 && code <= 599) {
    for (int i = -1; i <= 2; i++) {
      int dx = (i % 2) * 6;
      lcd.drawFastHLine(cx - 20 + dx, cy - i * 6, 40 - 2 * dx, COL_DIM);
    }
  } else {
    drawCloudShape(cx, cy, 0xBDF7);
  }
}

// 天气图标码 -> 中文
static const char* weatherTextCN(int code) {
  if (code == 100 || code == 150) return "晴";
  if (code == 101 || code == 151) return "多云";
  if (code == 102 || code == 152) return "少云";
  if (code == 103 || code == 153) return "晴间多云";
  if (code == 104 || code == 154) return "阴";
  if (code == 302 || code == 312 || code == 313) return "雷阵雨";
  if (code == 303) return "雷雹";
  if (code == 304 || code == 404 || code == 405) return "雨夹雪";
  if (code == 305) return "小雨";
  if (code == 306) return "中雨";
  if (code == 307) return "大雨";
  if (code == 308) return "暴雨";
  if (code >= 300 && code <= 399) return "雨";
  if (code == 400) return "小雪";
  if (code == 401) return "中雪";
  if (code == 402) return "大雪";
  if (code == 403) return "暴雪";
  if (code >= 400 && code <= 499) return "雪";
  if (code == 500) return "薄雾";
  if (code == 501 || code == 509 || code == 514) return "雾";
  if (code == 502 || code == 511 || code == 512 || code == 513) return "霾";
  if (code == 503) return "扬沙";
  if (code == 504) return "浮尘";
  if (code == 507 || code == 508) return "沙尘暴";
  if (code >= 500 && code <= 599) return "雾";
  return "天气";
}

void renderWeather(const WeatherData& w) {
  // 天气卡片：只在拉到新数据时重画，每次全清再绘制（最稳妥）
  lcd.fillRoundRect(12, 126, 216, 104, 10, COL_PANELBG);
  lcd.drawRoundRect(12, 126, 216, 104, 10, COL_PANELBD);

  // 天气图标（左侧）
  drawWeatherIcon(w.icon, 50, 164);

  // 天气文字（中文，卡片左侧图标下方）
  lcd.setFont(&fonts::efontCN_12);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_WHITE, COL_PANELBG);
  lcd.drawString(weatherTextCN(w.icon), 50, 212);

  // 温度（右侧，FreeMono 粗体数字 + efontCN 大字 °C 上标）
  char tbuf[8];
  snprintf(tbuf, sizeof tbuf, "%d", (int)(w.temp + 0.5f));
  int tcol = w.temp >= 28 ? COL_RED : (w.temp <= 8 ? COL_BLUE : COL_ORANGE);
  lcd.setFont(&fonts::FreeMonoBold18pt7b);
  lcd.setTextSize(2);
  lcd.setTextColor(tcol, COL_PANELBG);
  lcd.setTextDatum(middle_right);
  lcd.drawString(tbuf, 196, 162);
  // °C：efontCN 全字库含 U+00B0，12px 上标（比圆点清晰，且不额外引入大字库）
  lcd.setFont(&fonts::efontCN_12);
  lcd.setTextSize(1);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(tcol, COL_PANELBG);
  lcd.drawString("°C", 200, 144);

  // 湿度 / 体感（右侧第二行）
  char hbuf[24];
  snprintf(hbuf, sizeof hbuf, "湿度 %d%%  体感 %d", w.humidity, (int)(w.feels + 0.5f));
  lcd.setFont(&fonts::efontCN_12);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_BLUE, COL_PANELBG);
  lcd.drawString(hbuf, 118, 206);

  gWeatherPanelDrawn = true;
}

// ============================================================
// 行情行（时钟 y=30~86 与天气卡片 y=126 之间，y=90~122）
// 布局：左 标签+价格+单位，右 涨跌幅（红涨绿跌）
// ============================================================
void renderQuote(const QuoteData& q) {
  lcd.fillRect(0, 90, 240, 34, COL_BG);
  if (!q.ok) return;

  int x = 16;
  // 标签（中文 efontCN_12）
  lcd.setFont(&fonts::efontCN_12);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_DIM, COL_BG);
  lcd.drawString(q.label, x, 107);
  x += lcd.textWidth(q.label) + 6;

  // 价格（FreeMono 粗体平滑数字）
  char pbuf[24];
  snprintf(pbuf, sizeof pbuf, "%.*f", q.decimals, q.price);
  lcd.setFont(&fonts::FreeMonoBold9pt7b);
  lcd.setTextSize(1);
  lcd.setTextColor(COL_CYAN, COL_BG);
  lcd.drawString(pbuf, x, 109);
  x += lcd.textWidth(pbuf) + 6;

  // 单位（中文 efontCN_12）
  if (q.unit.length()) {
    lcd.setFont(&fonts::efontCN_12);
    lcd.setTextColor(COL_DIM, COL_BG);
    lcd.drawString(q.unit, x, 107);
  }

  // 涨跌幅：仅预设源有数据；红涨绿跌（国内习惯）
  if (q.changePct > 0.005f || q.changePct < -0.005f) {
    char cbuf[16];
    snprintf(cbuf, sizeof cbuf, "%+.2f%%", q.changePct);
    uint16_t ccol = q.changePct > 0 ? 0xF800 : 0x07E0;
    lcd.setFont(&fonts::FreeMonoBold9pt7b);
    lcd.setTextColor(ccol, COL_BG);
    lcd.setTextDatum(middle_right);
    lcd.drawString(cbuf, 224, 109);
  }
}

// ============================================================
// 状态页：开机、等待、错误时用。内部已经 fillScreen，单独使用
// ============================================================
void renderStatus(const char* msg, const String& sub) {
  lcd.fillScreen(COL_BG);
  lcd.setFont(&fonts::efontCN_12);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_WHITE, COL_BG);
  lcd.drawString(msg, 120, 112);
  if (sub.length()) {
    lcd.setTextColor(COL_DIM, COL_BG);
    lcd.drawString(sub.c_str(), 120, 140);
  }
  gLastRenderMinute = 0xFFFF;  // 下次 renderClock 会重画顶栏
}