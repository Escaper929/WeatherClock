// ============================================================================
//  theme_retro.cpp - Retro 主题：辐射终端 / Pip-Boy 磷光绿
//
//  定位：Fallout Pip-Boy 式绿色磷光 CRT 终端 + 电子管读数质感。
//        磷光绿四级亮度单色系、终端边框角标、双线分隔、细笔画七段、
//        电子管行情窗 —— 绿色风格签名全部保留。
//
//  三轮迭代：UI 布局对齐 Wasteland 简洁版（每行只说一件事）——
//  居中日期头部 / 大七段时间 / 图标+温度+RH 仪表组 / 城市+天气+体感行 /
//  电子管行情窗。删除了提示符光标、STAT 进度条行。
//
//  布局（y 自上而下，内容区 x 8..231，避开边框竖臂）：
//    终端边框(1px 微光框+四角 2px 亮标) →
//    头部 20（日期 周几，居中）→ 双线 30 →
//    细七段时间 40..91（磷光绿）→ 双线 106 →
//    天气带 110..159（左 磷光图标 / 右 七段温度+°C，下 RH 湿度）→
//    天气文字行 170（城市 多云 / 体感 30°）→
//    行情行 202（标签 + 电子管价格窗 + 单位 | 涨跌）
// ============================================================================
#include <LovyanGFX.hpp>
#include "themes/ThemeShared.h"
using namespace thm;

// ---- 磷光绿调色板（单色系，仅亮度分级） ----
static const uint16_t BG        = ui::rgb565(  6,  14,   9);  // 近黑绿底
static const uint16_t PH        = ui::rgb565( 64, 220, 124);  // 主磷光
static const uint16_t PH_BRIGHT = ui::rgb565(150, 255, 195);  // 高亮（时间/温度/价格/涨）
static const uint16_t PH_DIM    = ui::rgb565( 30, 110,  62);  // 暗磷光（辅助/日期/RH/跌）
static const uint16_t PH_FAINT  = ui::rgb565( 16,  56,  32);  // 微光（框线/分隔/窗描边）

static const lgfx::IFont* const FONT_CN    = &fonts::efontCN_12;
static const lgfx::IFont* const FONT_PRICE = &fonts::FreeMonoBold12pt7b;
static const lgfx::IFont* const FONT_SMALL = &fonts::FreeMonoBold9pt7b;

// ---- 布局常量（对齐 Wasteland 简洁版） ----
static const int CL_X0 = 8,  CL_XR = 231;      // 内容清除范围（避开边框竖臂）

static const int HDR_Y  = 20;                  // 头部中线
static const int CLK_Y  = 40;                  // 时间七段顶部（52 高）
static const int COLON_CX = 120;

static const int ICON_CX = 40, ICON_CY = 136;  // 天气图标中心（40x40）
static const int TEMP_TOP = 118;               // 七段温度顶部（28 高）
static const int RH_Y    = 150;                // 湿度读数中线（温度下方）
static const int TXT_Y   = 170;                // 城市/天气/体感行中线
static const int Q_Y     = 202;                // 行情行中线

// 时间七段（细笔画，像素感），方点冒号
static const SegGeom SEGR{30, 52, 3, 2};
static const int CLK_XS[4] = { 47, 79, 131, 163 };
// 温度七段（小号）
static const SegGeom SEGT{16, 28, 2, 1};

// ---- 脏检测 ----
static uint32_t sEpoch  = 0;
static uint16_t sTopKey = 0xFFFF;
static int      sDigit[4] = {-1,-1,-1,-1};
static bool     sBlink  = false;
static uint32_t sWKey = 0, sTKey = 0, sQKey = 0;

static void resetDirty() {
  sTopKey = 0xFFFF;
  for (int i = 0; i < 4; i++) sDigit[i] = -1;
  sBlink = false;
  sWKey = 0; sTKey = 0; sQKey = 0;
}

// 终端边框：1px 微光框 + 四角 2px 亮标
static void terminalFrame() {
  lcd.drawRect(5, 5, 230, 230, PH_FAINT);
  const int m = 5, a = 12, t = 2;
  lcd.fillRect(m, m, a, t, PH);             lcd.fillRect(m, m, t, a, PH);
  lcd.fillRect(240-m-a, m, a, t, PH);       lcd.fillRect(240-m-t, m, t, a, PH);
  lcd.fillRect(m, 240-m-t, a, t, PH);       lcd.fillRect(m, 240-m-a, t, a, PH);
  lcd.fillRect(240-m-a, 240-m-t, a, t, PH); lcd.fillRect(240-m-t, 240-m-a, t, a, PH);
}

// Fallout 终端双线分隔
static void dblRule(int y) {
  lcd.fillRect(14, y, 212, 1, PH_FAINT);
  lcd.fillRect(14, y + 2, 212, 1, PH_FAINT);
}

void retroTick(const UiData& d, bool blinkColon) {
  uint32_t ep = themeEpoch();
  if (ep != sEpoch) {
    sEpoch = ep;
    lcd.fillScreen(BG);
    terminalFrame();
    resetDirty();
  }

  const struct tm& tm = d.dt;

  // ---- 头部：日期 周几，居中（每分钟重绘，日期随之更新） ----
  uint16_t topKey = (uint16_t)((tm.tm_wday << 8) | tm.tm_min);
  if (topKey != sTopKey) {
    sTopKey = topKey;
    lcd.fillRect(CL_X0, 13, 224, 14, BG);         // 13..26，不触碰双线 y=30
    lcd.setFont(FONT_SMALL);
    lcd.setTextSize(1);
    char db[24];
    snprintf(db, sizeof db, "%04d.%02d.%02d %s",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, WEEK_EN[tm.tm_wday]);
    lcd.setTextDatum(middle_center);
    lcd.setTextColor(PH_DIM, BG);
    lcd.drawString(db, 120, HDR_Y);
  }

  // ---- 细七段时间（磷光绿，逐位脏检测） ----
  {
    int digits[4] = { tm.tm_hour / 10, tm.tm_hour % 10, tm.tm_min / 10, tm.tm_min % 10 };
    for (int i = 0; i < 4; i++) {
      if (digits[i] != sDigit[i]) {
        lcd.fillRect(CLK_XS[i], CLK_Y, 30, 52, BG);
        segDigit(CLK_XS[i], CLK_Y, digits[i], SEGR, PH_BRIGHT);
        sDigit[i] = digits[i];
      }
    }
  }
  if (blinkColon != sBlink) {
    lcd.fillRect(COLON_CX - 4, CLK_Y + 6, 8, 40, BG);
    segColonSquare(COLON_CX, CLK_Y, 52, blinkColon, PH);
  }

  dblRule(30);
  dblRule(106);

  // ---- 天气带：磷光图标 + 温度/RH 仪表组 ----
  uint32_t wk = 0;
  if (d.weather && d.weather->ok) {
    const WeatherData& w = *d.weather;
    wk = ((uint32_t)(w.icon & 0xFF) << 20) ^ ((uint32_t)((int)(w.temp * 10) & 0x3FF) << 10) ^
         ((uint32_t)(w.humidity & 0x7F)) ^ 1;
  }
  if (wk != sWKey) {
    sWKey = wk;
    lcd.fillRect(CL_X0, 110, 224, 50, BG);        // 110..159，不触碰双线
    if (wk) {
      const WeatherData& w = *d.weather;
      // 单色磷光剪影：亮部 PH、暗部 PH_DIM，同色系分层
      weatherIconSil(w.icon, ICON_CX, ICON_CY, BG,
                     PH, PH_BRIGHT, PH, PH_DIM, PH, PH, PH_DIM, PH_BRIGHT);

      lcd.setFont(FONT_CN);                       // °C 用 efont（FreeMono ° 字形不可靠）
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(PH_DIM, BG);
      lcd.drawString("\xC2\xB0""C", 198, TEMP_TOP + 13);

      int tv = (int)lroundf(w.temp);
      bool neg = tv < 0;
      int a = neg ? -tv : tv;
      char tb[5];
      snprintf(tb, sizeof tb, "%d", a);
      int tw = strlen(tb) * 18;                   // 每位 16+2 步进
      int x  = 194 - tw - (neg ? 18 : 0);
      if (neg) { segMinus(x, TEMP_TOP, SEGT, PH_BRIGHT); x += 18; }
      for (const char* p = tb; *p; p++) {
        segDigit(x, TEMP_TOP, *p - '0', SEGT, PH_BRIGHT);
        x += 18;
      }

      // 温度下方：RH 湿度读数（仪表组第二行）
      char rb[12];
      snprintf(rb, sizeof rb, "RH %d%%", w.humidity);
      lcd.setFont(FONT_SMALL);
      lcd.setTextDatum(middle_right);
      lcd.setTextColor(PH_DIM, BG);
      lcd.drawString(rb, CL_XR, RH_Y);
    }
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
      lcd.fillRect(CL_X0, TXT_Y - 6, 224, 13, BG);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      int x = CL_X0 + 4;
      if (d.city && d.city[0]) {
        lcd.setTextColor(PH, BG);
        x += drawTextClamped(d.city, x, TXT_Y, 64) + 2;
      }
      if (tk) {
        lcd.setTextColor(PH_DIM, BG);
        drawTextClamped(fb, x, TXT_Y, CL_XR - 4 - x);
      }
    }
  }

  // ---- 行情行：标签 + 电子管价格窗 + 单位 | 涨跌 ----
  uint32_t qk = 0;
  if (d.quote && d.quote->ok) {
    const QuoteData& q = *d.quote;
    qk = (uint32_t)(q.price * 100) ^ ((uint32_t)((int)(q.changePct * 100)) << 3) ^
         ((uint32_t)q.label.length() << 20) ^ ((uint32_t)q.unit.length() << 24) ^
         ((uint32_t)q.decimals << 28) ^ 1;
  }
  if (qk != sQKey) {
    sQKey = qk;
    lcd.fillRect(CL_X0, Q_Y - 12, 224, 25, BG);   // 190..214，不触碰底部边框
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

      int x = CL_X0 + 4;
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      if (labelW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(PH_DIM, BG);
        drawTextClamped(q.label.c_str(), x, Q_Y, 72);
        x += labelW + gap;
      }
      // 电子管窗口：微光描边 + 高亮磷光数字（nixie 质感）
      lcd.drawRect(x - 5, Q_Y - 10, priceW + 10, 20, PH_FAINT);
      lcd.setFont(FONT_PRICE);
      lcd.setTextColor(PH_BRIGHT, BG);
      lcd.drawString(pbuf, x, Q_Y + 1);
      x += priceW + 10 + (unitW > 0 ? gap : 0);
      if (unitW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(PH_DIM, BG);
        drawTextClamped(q.unit.c_str(), x, Q_Y, limit - x);
      }
      if (hasPct) {
        lcd.setFont(FONT_SMALL);
        lcd.setTextDatum(middle_right);
        lcd.setTextColor(q.changePct > 0 ? PH_BRIGHT : PH_DIM, BG);
        lcd.drawString(cbuf, CL_XR, Q_Y + 1);
      }
    }
  }
}
