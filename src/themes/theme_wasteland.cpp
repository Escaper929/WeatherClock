// ============================================================================
//  theme_wasteland.cpp - Wasteland 主题：废土蒸汽朋克终端 / 琥珀 CRT
//
//  定位：核战后地下避难所里仍在工作的老式气象终端。琥珀色磷光 CRT +
//        工业控制面板语言：黄铜边框与铆钉、仪表窗框、铭牌式日期、
//        淡扫描线、极轻微背光闪烁。
//
//  简洁版原则（二轮迭代）：可读性优先 —— 每行只说一件事，
//  装饰编号只保留一处设备铭牌，湿度并入温度仪表组，行距拉开。
//
//  布局（y 自上而下，内容窗口 12..227）：
//    黄铜边框(黄铜板+深棕聚光边+四角螺丝+边中铆钉) →
//    头部 20（日期 周几，居中）→ 分隔 30 →
//    大七段时间 40..91（琥珀高亮，单字位仪表窗框 37..94）→ 分隔 106 →
//    天气带 110..159（左 工业丝印图标 / 右 温度+°C，下 RH 湿度）→
//    天气文字行 170（城市 多云 / 体感 30°）→
//    行情行 202（标签+琥珀读数窗+涨跌）
// ============================================================================
#include <LovyanGFX.hpp>
#include "themes/ThemeShared.h"
#include "Screen.h"
using namespace thm;

// ---- 琥珀废土调色板 ----
static const uint16_t BG     = ui::rgb565(  5,   4,   3);  // 边框外纯黑
static const uint16_t CNT    = ui::rgb565( 13,  11,   8);  // CRT 内容底（暖黑）
static const uint16_t SCAN   = ui::rgb565(  8,   6,   4);  // 扫描线（极淡，只微压暗）
static const uint16_t AM_HI  = ui::rgb565(239, 203, 133);  // 亮琥珀（时间/温度/价格）
static const uint16_t AM     = ui::rgb565(214, 168,  90);  // 主琥珀 #D6A85A
static const uint16_t AM_DIM = ui::rgb565(169, 104,  42);  // 暗琥珀（辅助）
static const uint16_t BRASS  = ui::rgb565(122,  85,  40);  // 黄铜（框架/端帽）
static const uint16_t BRASS_D= ui::rgb565( 64,  46,  24);  // 暗黄铜（细线）
static const uint16_t CELL   = ui::rgb565( 26,  19,  11);  // 仪表窗框
static const uint16_t WARN   = ui::rgb565(167,  70,  50);  // 警告红（闪电图标）
static const uint16_t CWHT   = ui::rgb565(208, 199, 178);  // 冷白点缀（雪）

static const lgfx::IFont* const FONT_CN    = &fonts::efontCN_12;
static const lgfx::IFont* const FONT_PRICE = &fonts::FreeMonoBold12pt7b;
static const lgfx::IFont* const FONT_SMALL = &fonts::FreeMonoBold9pt7b;

// ---- 布局常量 ----
static const int CL_X0 = 12, CL_XR = 227;      // 内容窗口

static const int HDR_Y  = 20;                  // 头部中线
static const int RULE1_Y = 30;                 // 头部分隔
static const int CLK_Y  = 40;                  // 时间七段顶部（52 高）
static const int COLON_CX = 120;
static const int RULE2_Y = 106;                // 时间/天气分隔

// 时间七段（粗笔画仪表数字），单字位仪表窗框
static const SegGeom SEGR{28, 52, 4, 2};
static const int CLK_XS[4] = { 51, 82, 130, 161 };

static const int ICON_CX = 40, ICON_CY = 136;  // 天气图标中心（40x40）
static const int TEMP_TOP = 118;               // 七段温度顶部（26 高）
static const int RH_Y    = 150;                // 湿度读数中线（温度下方）
static const int TXT_Y   = 170;                // 城市/天气/体感行中线
static const int Q_Y     = 202;                // 行情行中线

// 温度七段（小号）
static const SegGeom SEGT{15, 26, 2, 1};

// ---- 脏检测 ----
static uint32_t sEpoch  = 0;
static uint16_t sTopKey = 0xFFFF;
static int      sDigit[4] = {-1,-1,-1,-1};
static bool     sBlink  = false;
static uint32_t sWKey = 0, sTKey = 0, sQKey = 0;
static bool     sFull  = false;                // 本轮需补扫描线/整体收尾
static uint32_t sNextFlick = 0;

static void resetDirty() {
  sTopKey = 0xFFFF;
  for (int i = 0; i < 4; i++) sDigit[i] = -1;
  sBlink = false;
  sWKey = 0; sTKey = 0; sQKey = 0;
  sFull = true;
}

// ---------------------------------------------------------------------------
// CRT 扫描线：内容区内每 3 行 1 条极淡暗线（脏区重绘后补盖，保持覆盖连续）
// ---------------------------------------------------------------------------
static void stampScan(int x, int y, int w, int h) {
  for (int yy = y; yy < y + h; ++yy)
    if (yy % 3 == 0) lcd.drawFastHLine(x, yy, w, SCAN);
}

// ---------------------------------------------------------------------------
// 黄铜工业边框：黄铜板 + 深棕聚光边 + 四角螺丝 + 边中铆钉
// ---------------------------------------------------------------------------
static void bezelFrame() {
  lcd.fillScreen(BG);
  lcd.fillRect(3, 3, 234, 234, BRASS);            // 黄铜外板
  lcd.fillRect(4, 4, 232, 232, CNT);              // 深棕聚光边（近黑暖调）
  lcd.fillRect(9, 9, 222, 222, CNT);              // CRT 内容窗
  lcd.drawRect(9, 9, 222, 222, BRASS_D);          // 内容窗细线
  // 四角螺丝：黄铜圆头 + 一字槽
  const int sc[4][2] = { {7,7}, {232,7}, {7,232}, {232,232} };
  for (int i = 0; i < 4; i++) {
    lcd.fillCircle(sc[i][0], sc[i][1], 2, BRASS);
    lcd.drawFastHLine(sc[i][0] - 2, sc[i][1], 5, SCAN);
  }
  // 边中铆钉（上/下/左/右）
  lcd.fillRect(119, 5, 2, 2, BRASS);  lcd.fillRect(119, 233, 2, 2, BRASS);
  lcd.fillRect(5, 119, 2, 2, BRASS);  lcd.fillRect(233, 119, 2, 2, BRASS);
}

// 分隔线（暗黄铜细线）
static void rule(int y) {
  lcd.fillRect(14, y, 212, 1, BRASS_D);
}

void wastelandTick(const UiData& d, bool blinkColon) {
  uint32_t ep = themeEpoch();
  if (ep != sEpoch) {
    sEpoch = ep;
    screenSetBrightness(SCREEN_BRIGHTNESS);       // 确保离开闪烁态
    bezelFrame();
    resetDirty();
  }

  const struct tm& tm = d.dt;

  // ---- 头部：日期 周几，居中（每分钟重绘，日期随之更新） ----
  uint16_t topKey = (uint16_t)((tm.tm_wday << 8) | tm.tm_min);
  if (topKey != sTopKey) {
    sTopKey = topKey;
    lcd.fillRect(CL_X0, 13, 216, 14, CNT);        // 13..26，不触碰分隔线 y=30
    lcd.setFont(FONT_SMALL);
    lcd.setTextSize(1);
    char db[24];
    snprintf(db, sizeof db, "%04d.%02d.%02d %s",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, WEEK_EN[tm.tm_wday]);
    lcd.setTextDatum(middle_center);
    lcd.setTextColor(AM_DIM, CNT);
    lcd.drawString(db, 120, HDR_Y);
    stampScan(CL_X0, 13, 216, 14);
  }

  // ---- 大七段时间（琥珀高亮，仪表窗框，逐位脏检测） ----
  {
    int digits[4] = { tm.tm_hour / 10, tm.tm_hour % 10, tm.tm_min / 10, tm.tm_min % 10 };
    for (int i = 0; i < 4; i++) {
      if (digits[i] != sDigit[i]) {
        lcd.fillRect(CLK_XS[i], CLK_Y, 28, 52, CNT);
        segDigit(CLK_XS[i], CLK_Y, digits[i], SEGR, AM_HI);
        stampScan(CLK_XS[i], CLK_Y, 28, 52);
        sDigit[i] = digits[i];
      }
    }
  }
  if (blinkColon != sBlink) {
    lcd.fillRect(COLON_CX - 4, CLK_Y + 6, 8, 40, CNT);
    segColonSquare(COLON_CX, CLK_Y, 52, blinkColon, AM);
    stampScan(COLON_CX - 4, CLK_Y + 6, 8, 40);
  }
  if (sFull) {
    for (int i = 0; i < 4; i++) lcd.drawRect(CLK_XS[i] - 3, CLK_Y - 3, 34, 58, CELL);
  }

  rule(RULE1_Y);
  rule(RULE2_Y);

  // ---- 天气带：工业丝印图标 + 温度/湿度仪表组 ----
  uint32_t wk = 0;
  if (d.weather && d.weather->ok) {
    const WeatherData& w = *d.weather;
    wk = ((uint32_t)(w.icon & 0xFF) << 20) ^ ((uint32_t)((int)(w.temp * 10) & 0x3FF) << 10) ^
         ((uint32_t)(w.humidity & 0x7F)) ^ 1;
  }
  if (wk != sWKey) {
    sWKey = wk;
    lcd.fillRect(14, 110, 216, 50, CNT);          // 110..159，不触碰分隔线
    if (wk) {
      const WeatherData& w = *d.weather;
      // 单色丝印：琥珀系 + 闪电警告红 + 雪冷白
      weatherIconSil(w.icon, ICON_CX, ICON_CY, CNT,
                     AM_HI, AM_HI, AM, AM_DIM, AM, CWHT, AM_DIM, WARN);

      lcd.setFont(FONT_CN);                       // °C 用 efont（FreeMono ° 字形不可靠）
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(AM_DIM, CNT);
      lcd.drawString("\xC2\xB0""C", 198, TEMP_TOP + 13);

      int tv = (int)lroundf(w.temp);
      bool neg = tv < 0;
      int a = neg ? -tv : tv;
      char tb[5];
      snprintf(tb, sizeof tb, "%d", a);
      int tw = strlen(tb) * 17;                   // 每位 15+2 步进
      int x  = 194 - tw - (neg ? 17 : 0);
      if (neg) { segMinus(x, TEMP_TOP, SEGT, AM_HI); x += 17; }
      for (const char* p = tb; *p; p++) {
        segDigit(x, TEMP_TOP, *p - '0', SEGT, AM_HI);
        x += 17;
      }

      // 温度下方：RH 湿度读数（仪表组第二行）
      char rb[12];
      snprintf(rb, sizeof rb, "RH %d%%", w.humidity);
      lcd.setFont(FONT_SMALL);
      lcd.setTextDatum(middle_right);
      lcd.setTextColor(AM_DIM, CNT);
      lcd.drawString(rb, CL_XR, RH_Y);
    }
    stampScan(14, 110, 216, 50);
  }

  // ---- 城市 / 天气 / 体感行 ----
  {
    char fb[48] = "";
    uint32_t tk = 0;
    if (d.weather && d.weather->ok) {
      const WeatherData& w = *d.weather;
      const char* desc = w.text.length() ? w.text.c_str() : weatherTextCN(w.icon);
      snprintf(fb, sizeof fb, "%s / \xE4\xBD\x93\xE6\x84\x9F %d\xC2\xB0",
               desc, (int)lroundf(w.feels));
      tk = ((uint32_t)((int)(w.feels * 10)) << 8) ^ ((uint32_t)w.text.length() << 2) ^
           ((uint32_t)(d.city ? strlen(d.city) : 0) << 18) ^ 1;
    }
    if (tk != sTKey) {
      sTKey = tk;
      lcd.fillRect(CL_X0, TXT_Y - 6, 216, 13, CNT);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      int x = CL_X0 + 2;
      if (d.city && d.city[0]) {
        lcd.setTextColor(AM, CNT);
        x += drawTextClamped(d.city, x, TXT_Y, 64) + 2;
      }
      if (tk) {
        lcd.setTextColor(AM_DIM, CNT);
        drawTextClamped(fb, x, TXT_Y, CL_XR - 2 - x);
      }
      stampScan(CL_X0, TXT_Y - 6, 216, 13);
    }
  }

  // ---- 行情行：标签 + 琥珀读数窗 + 单位 | 涨跌 ----
  uint32_t qk = 0;
  if (d.quote && d.quote->ok) {
    const QuoteData& q = *d.quote;
    qk = (uint32_t)(q.price * 100) ^ ((uint32_t)((int)(q.changePct * 100)) << 3) ^
         ((uint32_t)q.label.length() << 20) ^ ((uint32_t)q.unit.length() << 24) ^
         ((uint32_t)q.decimals << 28) ^ 1;
  }
  if (qk != sQKey) {
    sQKey = qk;
    lcd.fillRect(CL_X0, Q_Y - 12, 216, 25, CNT);  // 184..208，不触碰状态栏
    if (qk) {
      const QuoteData& q = *d.quote;
      char pbuf[24];
      snprintf(pbuf, sizeof pbuf, "%.*f", q.decimals, q.price);

      bool hasPct = (q.changePct > 0.005f || q.changePct < -0.005f);
      char cbuf[16];
      int pctW = 0;
      lcd.setFont(FONT_SMALL);
      if (hasPct) {
        snprintf(cbuf, sizeof cbuf, "%+.2f%%", q.changePct);
        pctW = lcd.textWidth(cbuf);
      }

      lcd.setFont(FONT_CN);
      int labelW = q.label.length() ? lcd.textWidth(q.label) : 0;
      int unitW  = q.unit.length()  ? lcd.textWidth(q.unit)  : 0;
      lcd.setFont(FONT_PRICE);
      int priceW = lcd.textWidth(pbuf);
      int limit  = hasPct ? (CL_XR - pctW - 12) : CL_XR;
      int gaps   = (labelW > 0 ? 1 : 0) + (unitW > 0 ? 1 : 0);
      int gap = 6;
      if (gaps > 0) {
        int avail = (limit - CL_X0) - (labelW + priceW + unitW + 10);
        gap = constrain(avail / gaps, 3, 8);
      }

      int x = CL_X0 + 2;
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      if (labelW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(AM_DIM, CNT);
        drawTextClamped(q.label.c_str(), x, Q_Y, 72);
        x += labelW + gap;
      }
      // 仪表读数窗：暗黄铜描边 + 亮琥珀数字
      lcd.drawRect(x - 5, Q_Y - 10, priceW + 10, 20, BRASS_D);
      lcd.setFont(FONT_PRICE);
      lcd.setTextColor(AM_HI, CNT);
      lcd.drawString(pbuf, x, Q_Y + 1);
      x += priceW + 10 + (unitW > 0 ? gap : 0);
      if (unitW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(AM_DIM, CNT);
        drawTextClamped(q.unit.c_str(), x, Q_Y, limit - x);
      }
      if (hasPct) {
        lcd.setFont(FONT_SMALL);
        lcd.setTextDatum(middle_right);
        lcd.setTextColor(q.changePct > 0 ? AM_HI : AM_DIM, CNT);
        lcd.drawString(cbuf, CL_XR, Q_Y + 1);
      }
    }
    stampScan(CL_X0, Q_Y - 12, 216, 25);
  }

  // ---- 整体重绘收尾：全内容区扫描线 ----
  if (sFull) {
    sFull = false;
    stampScan(CL_X0, 12, 216, 216);
  }

  // ---- 极轻微 CRT 闪烁：数秒一次、一拍即回的背光微降 ----
  {
    uint32_t now = millis();
    if (sNextFlick == 0) sNextFlick = now + 3000 + random(0, 3000);
    if (now >= sNextFlick) {
      sNextFlick = now + 4000 + random(0, 6000);
      screenSetBrightness(SCREEN_BRIGHTNESS - 16);
      delay(60 + random(0, 40));
      screenSetBrightness(SCREEN_BRIGHTNESS);
    }
  }
}
