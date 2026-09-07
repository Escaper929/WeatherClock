// ============================================================================
//  Screen.cpp - ST7789 240x240 渲染实现
// ============================================================================
#include "Screen.h"
#include "BoardPins.h"
#include <LovyanGFX.hpp>
#include <math.h>
#include <time.h>

static void drawWeatherIcon(int code, int cx, int cy);

// ---- 颜色 ----
static const uint16_t COL_BG      = 0x0107; // 深蓝黑
static const uint16_t COL_PANEL   = 0x10CB; // 面板浅蓝
static const uint16_t COL_CYAN    = 0x07FF;
static const uint16_t COL_WHITE   = 0xFFFF;
static const uint16_t COL_DIM     = 0x7BEF;
static const uint16_t COL_ORANGE  = 0xFD20;
static const uint16_t COL_BLUE    = 0x041F;

class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;

  LGFX(void) {
    { auto cfg = _bus.config();
      cfg.spi_host  = SPI2_HOST;      // FSPI
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

// 用 LEDC 手动控制背光（绕开 LovyanGFX 的 ILight 接口，C3 上更省事）
static void backlightSetup() {
#if (defined(PIN_BL) && PIN_BL >= 0)
  ledcSetup(0, 12000, 8);
  ledcAttachPin(PIN_BL, 0);
  ledcWrite(0, SCREEN_BRIGHTNESS);
#endif
}

static const char* WEEK[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

bool screenInit() {
  lcd.init();
  lcd.display();
  backlightSetup();
  lcd.setRotation(0);
  lcd.fillScreen(COL_BG);
  return true;
}

void screenSetBrightness(uint8_t v) {
#if (defined(PIN_BL) && PIN_BL >= 0)
  ledcWrite(0, v);
#endif
}

static void clearWeatherPanel() {
  lcd.fillRect(12, 156, 216, 74, COL_BG);
  // 底色如上，天气区本身是深色背景的一部分；若想加面板框可打开下面注释
  // lcd.fillRoundRect(12, 156, 216, 74, 8, COL_PANEL);
}

void renderClock(unsigned long t, bool blinkColon) {
  struct tm tm;
  if (t == 0 || !gmtime_r((time_t*)&t, &tm)) return;

  int hh = tm.tm_hour, mm = tm.tm_min;
  char buf[16];
  snprintf(buf, sizeof buf, "%02d%c%02d", hh, blinkColon ? ':' : ' ', mm);

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextSize(4);              // 8x16 放大 4x => 32x64
  lcd.setTextColor(COL_WHITE, COL_BG);
  lcd.drawString(buf, 120, 62);
  lcd.setTextSize(1);

  char dat[32];
  const char* wd = WEEK[tm.tm_wday];
  snprintf(dat, sizeof dat, "%s  %04d-%02d-%02d", wd,
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_DIM, COL_BG);
  lcd.drawString(dat, 120, 110);

  // 分隔线
  lcd.drawFastHLine(20, 142, 200, COL_PANEL);
}

void renderWeather(const WeatherData& w) {
  clearWeatherPanel();
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(middle_left);
  lcd.setTextSize(1);
  lcd.setTextColor(COL_DIM, COL_BG);

  // 天气文字（英文短名）
  lcd.drawString(weatherTextShort(w.icon), 108, 204);

  // 温度
  char tbuf[12];
  snprintf(tbuf, sizeof tbuf, "%d", (int)(w.temp + 0.5f));
  lcd.setFont(&fonts::Font2);
  lcd.setTextSize(4);
  lcd.setTextDatum(middle_right);
  lcd.setTextColor(COL_ORANGE, COL_BG);
  lcd.drawString(tbuf, 196, 168);
  lcd.fillCircle(202, 156, 4, COL_ORANGE);   // 温度上标点
  lcd.setTextSize(1);

  // 湿度
  char hbuf[16];
  snprintf(hbuf, sizeof hbuf, "Hum %d%%", w.humidity);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_BLUE, COL_BG);
  lcd.drawString(hbuf, 108, 222);

  // 天气图标（左侧）
  drawWeatherIcon(w.icon, 52, 186);
}

// ---- 简单天气图标：sun / cloud / rain / snow / fog / thunder ----
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

static void drawCloudShape(int cx, int cy) {
  lcd.fillCircle(cx - 14, cy, 8, COL_WHITE);
  lcd.fillCircle(cx, cy - 6, 10, COL_WHITE);
  lcd.fillCircle(cx + 14, cy, 8, COL_WHITE);
  lcd.fillRect(cx - 20, cy, 40, 8, COL_WHITE);
}

static void drawWeatherIcon(int code, int cx, int cy) {
  // 图标绘制在一个约 56x56 区域
  if (code == 100 || code == 150) {
    drawSun(cx, cy, 12);
  } else if (code >= 300 && code <= 399) {
    // 雨：云 + 雨滴
    drawCloudShape(cx, cy - 8);
    for (int i = -1; i <= 1; i++) {
      int dx = i * 10;
      lcd.fillRect(cx + dx - 2, cy + 8, 2, 8, COL_BLUE);
      lcd.fillTriangle(cx + dx - 2, cy + 8 + 8, cx + dx + 4, cy + 8 + 8, cx + dx + 4 - 4, cy + 8 + 16, COL_BLUE);
    }
  } else if (code >= 400 && code <= 499) {
    // 雪：云 + 雪花点
    drawCloudShape(cx, cy - 8);
    for (int i = -1; i <= 1; i++) {
      lcd.drawCircle(cx + i * 8, cy + 10, 3, COL_WHITE);
    }
  } else if (code >= 500 && code <= 599) {
    // 雾：横线
    for (int i = -1; i <= 2; i++) {
      int dx = (i % 2) * 6;
      lcd.drawFastHLine(cx - 20 + dx, cy - i * 6, 40 - 2 * dx, COL_DIM);
    }
  } else if (code >= 300 && code <= 399 && (code == 302 || code == 313 || code >= 314)) {
    // 雷暴：云 + 闪电
    drawCloudShape(cx, cy - 10);
    lcd.fillTriangle(cx - 2, cy - 2, cx + 6, cy - 8, cx - 12, cy + 10, COL_ORANGE);
  } else {
    // 多云 / 阴：云
    drawCloudShape(cx, cy);
  }
}

void renderStatus(const char* msg, const String& sub) {
  lcd.fillScreen(COL_BG);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_WHITE, COL_BG);
  lcd.drawString(msg, 120, 112);
  if (sub.length()) {
    lcd.setTextColor(COL_DIM, COL_BG);
    lcd.drawString(sub.c_str(), 120, 140);
  }
}