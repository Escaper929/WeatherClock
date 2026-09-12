// ============================================================================
//  theme_pixel.cpp - Pixel 主题：像素艺术 / 复古游戏机
//
//  定位：精致的 8-bit 像素风桌面小物。深蓝夜幕底 + 奶油白像素 +
//        复古绿/橙黄点缀，四角括号做"掌机屏幕"边框，像素虚线做分隔。
//        所有数字一律点阵（时间 7x10、其余 5x7），中文一律 efont —— 规则
//        统一，风格协调；天气图标用 20x20 字符画点阵，与其他主题完全不同。
//
//  布局（y 自上而下）——与 Modern 居中七段、Retro 琥珀瘦七段均不同：
//    四角括号边框 + HUD 顶栏 22（左 城市 / 右 周二+点阵日期）→ 虚线 40 →
//    点阵大时间 52..102 → 虚线 114 →
//    天气区 126..166（左点阵图标 / 右上点阵温度+°C / 右下天气文字）→
//    分栏元信息 176（左 湿度 / 右 体感）→ 虚线 190 →
//    行情账本两行 204 / 224（标签|价格，单位|涨跌，左右两栏对齐）
// ============================================================================
#include <LovyanGFX.hpp>
#include "themes/ThemeShared.h"
using namespace thm;

// ---- Pixel 调色板（NES 风，克制使用：主奶油白 + 三个点缀色） ----
static const uint16_t BG   = ui::rgb565( 12,  14,  30);  // 深蓝夜幕
static const uint16_t FG   = ui::rgb565(222, 226, 208);  // 奶油白（时间/图标/温度）
static const uint16_t DIM  = ui::rgb565(106, 112, 142);  // 灰蓝（辅助信息/边框）
static const uint16_t ACC  = ui::rgb565(255, 198,  88);  // 橙黄（价格/日期点缀）
static const uint16_t UP   = ui::rgb565(110, 208, 130);  // 复古绿（涨）
static const uint16_t DOWN = ui::rgb565(230, 120, 110);  // 砖红（跌）

static const lgfx::IFont* const FONT_CN = &fonts::efontCN_12;

// ---- 布局常量 ----
static const int IN_X   = 16;                  // 左右文字边距
static const int IN_XR  = 224;                 // 右对齐基准
static const int HDR_Y  = 22;                  // HUD 顶栏文字中线
static const int CLK_Y  = 52;                  // 点阵大时间顶部
static const int CLK_CELL = 5;                 // 7x10 -> 35x50
static const int ICON_CX = 46, ICON_CY = 146;  // 40x40 点阵图标中心
static const int TEMP_TOP = 126;               // 5x7 cell=3（21 高）顶
static const int WTEXT_Y = 154;                // 天气文字（右栏第二行）
static const int META_Y  = 176;                // 湿度/体感 行中线
static const int Q1_Y = 204, Q2_Y = 224;       // 行情两行中线

// ---- 脏检测 ----
static uint32_t sEpoch  = 0;
static uint16_t sTopKey = 0xFFFF;
static int      sDigit[4] = {-1,-1,-1,-1};
static bool     sBlink  = false;
static uint32_t sWKey = 0, sQKey = 0;
static uint16_t sMetaKey = 0xFFFF;

static void resetDirty() {
  sTopKey = 0xFFFF;
  for (int i = 0; i < 4; i++) sDigit[i] = -1;
  sBlink = false;
  sWKey = 0; sQKey = 0;
  sMetaKey = 0xFFFF;
}

// 像素虚线（4 亮 4 空），Retro 双实线 / Modern 发丝线的像素语言版本
static void dashH(int y, uint16_t col) {
  for (int x = IN_X; x < IN_XR; x += 8) lcd.fillRect(x, y, 4, 1, col);
}

// 掌机屏幕四角括号（2px 粗、14px 臂长）
static void bezel() {
  const int a = 14, t = 2, m = 6;
  lcd.fillRect(m, m, a, t, DIM);           lcd.fillRect(m, m, t, a, DIM);
  lcd.fillRect(240-m-a, m, a, t, DIM);     lcd.fillRect(240-m-t, m, t, a, DIM);
  lcd.fillRect(m, 240-m-t, a, t, DIM);     lcd.fillRect(m, 240-m-a, t, a, DIM);
  lcd.fillRect(240-m-a, 240-m-t, a, t, DIM); lcd.fillRect(240-m-t, 240-m-a, t, a, DIM);
}

void pixelTick(const UiData& d, bool blinkColon) {
  uint32_t ep = themeEpoch();
  if (ep != sEpoch) {
    sEpoch = ep;
    lcd.fillScreen(BG);
    bezel();
    resetDirty();
  }

  const struct tm& tm = d.dt;

  // ---- HUD 顶栏：城市（左）/ 周二 + 点阵日期（右） ----
  uint16_t topKey = (uint16_t)((tm.tm_wday << 8) | tm.tm_min);
  if (topKey != sTopKey) {
    sTopKey = topKey;
    lcd.fillRect(9, 8, 222, 26, BG);   // 避开四角括号竖臂
    // 右侧：周二(efont) + 09.12(点阵 cell=2)，整体右对齐
    int dateW = pixNum5x7Width("00.00", 2);          // 54
    int wkW   = 24;                                   // 周二 efont 两个全角
    lcd.setFont(FONT_CN);
    lcd.setTextSize(1);
    lcd.setTextDatum(middle_left);
    lcd.setTextColor(DIM, BG);
    lcd.drawString(WEEK_CN[tm.tm_wday], IN_XR - dateW - 4 - wkW, HDR_Y);
    char db[8];
    snprintf(db, sizeof db, "%02d.%02d", tm.tm_mon + 1, tm.tm_mday);
    pixNum5x7(IN_XR - dateW, HDR_Y - 7, db, 2, ACC);

    // 左侧：城市（防御截断，给右侧留足空间）
    if (d.city && d.city[0]) {
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM, BG);
      drawTextClamped(d.city, IN_X, HDR_Y, 240 - 2 * IN_X - dateW - wkW - 20);
    }
  }

  // ---- 点阵大时间（居中；逐位脏检测） ----
  {
    // "HH:MM"：digit advance 40，colon advance 15，总墨宽 = 4*40+15-5 = 170
    const int x0 = (240 - 170) / 2;                  // 35
    int xs[4] = { x0, x0 + 40, x0 + 95, x0 + 135 };
    int digits[4] = { tm.tm_hour / 10, tm.tm_hour % 10, tm.tm_min / 10, tm.tm_min % 10 };
    for (int i = 0; i < 4; i++) {
      if (digits[i] != sDigit[i]) {
        // 位图数字透明底直接画会叠残影，先清 35x50 位框
        lcd.fillRect(xs[i] - 2, CLK_Y, 42, 50, BG);
        pixDigit7x10(xs[i], CLK_Y, digits[i], CLK_CELL, FG);
        sDigit[i] = digits[i];
      }
    }
    const int colX = x0 + 80;                        // 冒号墨区 115..125
    if (blinkColon != sBlink) {
      lcd.fillRect(colX, CLK_Y + 2 * CLK_CELL, 2 * CLK_CELL, 6 * CLK_CELL, BG);
      if (blinkColon) pixColon7x10(colX, CLK_Y, CLK_CELL, FG);
      sBlink = blinkColon;
    }
  }

  dashH(114, DIM);

  // ---- 天气区：点阵图标 + 点阵温度 + 天气文字 ----
  uint32_t wk = 0;
  if (d.weather && d.weather->ok) {
    const WeatherData& w = *d.weather;
    wk = ((uint32_t)w.icon << 24) ^ ((uint32_t)((int)(w.temp * 10)) << 12) ^
         ((uint32_t)w.text.length() << 2) ^ 1;
  }
  if (wk != sWKey) {
    sWKey = wk;
    lcd.fillRect(9, 120, 222, 50, BG);
    if (wk) {
      const WeatherData& w = *d.weather;
      weatherIconPix(w.icon, ICON_CX, ICON_CY, FG, 2);      // 40x40 点阵图标

      // 点阵温度（5x7 cell=3），右缘给 °C 留 24px
      int tv = (int)lroundf(w.temp);
      char tb[5];
      if (tv < 0) snprintf(tb, sizeof tb, "-%d", -tv);
      else        snprintf(tb, sizeof tb, "%d", tv);
      int tw = pixNum5x7Width(tb, 3);
      pixNum5x7(IN_XR - 24 - 4 - tw, TEMP_TOP, tb, 3, FG);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM, BG);
      lcd.drawString("\xC2\xB0""C", IN_XR - 20, TEMP_TOP + 10);

      // 天气文字（右栏第二行，右对齐 + 防御截断）
      const char* desc = w.text.length() ? w.text.c_str() : weatherTextCN(w.icon);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM, BG);
      drawTextClamped(desc, IN_XR - 80, WTEXT_Y, 80);
    }
  }

  // ---- 元信息分栏：湿度（左）/ 体感（右） ----
  {
    char hb[16] = "", fb[16] = "";
    uint16_t mk = 0;
    if (d.weather && d.weather->ok) {
      const WeatherData& w = *d.weather;
      snprintf(hb, sizeof hb, "湿度 %d%%", w.humidity);
      snprintf(fb, sizeof fb, "体感 %d\xC2\xB0", (int)lroundf(w.feels));
      mk = (uint16_t)((w.humidity << 8) ^ ((int)(w.feels * 10) & 0xFF));
    }
    if (mk != sMetaKey) {
      sMetaKey = mk;
      lcd.fillRect(9, META_Y - 9, 222, 18, BG);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM, BG);
      lcd.drawString(hb, IN_X, META_Y);
      lcd.setTextDatum(middle_right);
      lcd.drawString(fb, IN_XR, META_Y);
    }
  }

  dashH(190, DIM);

  // ---- 行情账本：标签|价格 / 单位|涨跌，左右两栏 ----
  uint32_t qk = 0;
  if (d.quote && d.quote->ok) {
    const QuoteData& q = *d.quote;
    qk = (uint32_t)(q.price * 100) ^ ((uint32_t)((int)(q.changePct * 100)) << 3) ^
         ((uint32_t)q.label.length() << 20) ^ ((uint32_t)q.unit.length() << 24) ^
         ((uint32_t)q.decimals << 28) ^ 1;
  }
  if (qk != sQKey) {
    sQKey = qk;
    lcd.fillRect(9, Q1_Y - 12, 222, 40, BG);   // 192..231，不触碰底部边框括号
    if (qk) {
      const QuoteData& q = *d.quote;
      char pbuf[24];
      snprintf(pbuf, sizeof pbuf, "%.*f", q.decimals, q.price);

      // 行1：标签（左） + 点阵价格（右）
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM, BG);
      if (q.label.length()) drawTextClamped(q.label.c_str(), IN_X, Q1_Y, 120);
      int pw = pixNum5x7Width(pbuf, 2);
      pixNum5x7(IN_XR - pw, Q1_Y - 7, pbuf, 2, ACC);

      // 行2：单位（左） + 涨跌（右，点阵，绿涨红跌）
      lcd.setFont(FONT_CN);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM, BG);
      if (q.unit.length()) drawTextClamped(q.unit.c_str(), IN_X, Q2_Y, 120);
      bool hasPct = (q.changePct > 0.005f || q.changePct < -0.005f);
      if (hasPct) {
        char cb[16];
        snprintf(cb, sizeof cb, "%+.2f%%", q.changePct);
        int cw = pixNum5x7Width(cb, 2);
        pixNum5x7(IN_XR - cw, Q2_Y - 7, cb, 2, q.changePct > 0 ? UP : DOWN);
      }
    }
  }
}
