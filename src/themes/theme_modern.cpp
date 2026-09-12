// ============================================================================
//  theme_modern.cpp - Modern 主题（原精修 UI 迁移）
//
//  定位：现代极简 + 轻微复古电子钟。黑底暖白，时间绝对主角，
//        天气组 2×2 网格，行情弱化为页脚状态栏。日常长期使用首选。
//
//  布局（y 自上而下）：
//    顶栏文字中线 15（左 周+日期 / 右 城市）→ 七段时间 38..96 →
//    天气组 118..178（左列图标+天气文字，右列七段温度+湿度/体感）→
//    页脚发丝线 208 → 行情行 225
//
//  常量全部来自 include/UiTheme.h；绘制原语来自 ThemeShared。
// ============================================================================
#include "UiTheme.h"
#include "themes/ThemeShared.h"

using namespace ui;
using namespace thm;

// ---- 字体（与其他主题共享同一批资源，不新增） ----
static const lgfx::IFont* const FONT_CN    = &fonts::efontCN_12;
static const lgfx::IFont* const FONT_SMALL = &fonts::FreeMonoBold9pt7b;

// ---- 脏检测状态 ----
static uint32_t sEpoch  = 0;
static uint16_t sTopKey = 0xFFFF;
static int      sDigit[4] = {-1,-1,-1,-1};
static bool     sBlink  = false;
static uint32_t sWKey   = 0;        // 天气区 key（0 = 未画过）
static uint32_t sQKey   = 0;        // 行情区 key

static void resetDirty() {
  sTopKey = 0xFFFF;
  for (int i = 0; i < 4; i++) sDigit[i] = -1;
  sBlink = false;
  sWKey  = 0;
  sQKey  = 0;
}

// 温度七段（时间同语言缩小版），右缘对齐，支持负温度
static void drawTempSegments(int val, int xRight, int yTop) {
  const SegGeom segTemp{TSEG_W, TSEG_H, TSEG_T, TSEG_GAP};
  bool neg = val < 0;
  int a = val < 0 ? -val : val;
  char buf[4];
  int n = snprintf(buf, sizeof buf, "%d", a);
  int slots = n + (neg ? 1 : 0);
  int x = xRight - (slots * TSEG_W + (slots - 1) * TDIG_GAP);
  if (neg) {
    segMinus(x, yTop, segTemp, INK);
    x += TSEG_W + TDIG_GAP;
  }
  for (int i = 0; i < n; i++) {
    segDigit(x, yTop, buf[i] - '0', segTemp, INK);
    x += TSEG_W + TDIG_GAP;
  }
  lcd.setFont(FONT_CN);
  lcd.setTextSize(1);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(INK2, BG);
  lcd.drawString("\xC2\xB0""C", xRight + DEG_DX, yTop + DEG_DY);
}

void modernTick(const UiData& d, bool blinkColon) {
  // 主题激活：全屏重置
  uint32_t ep = themeEpoch();
  if (ep != sEpoch) {
    sEpoch = ep;
    lcd.fillScreen(BG);
    resetDirty();
  }

  const struct tm& tm = d.dt;

  // ---- 顶栏：星期+日期（左）+ 城市（右），分钟变化才重绘 ----
  uint16_t topKey = (uint16_t)((tm.tm_wday << 8) | tm.tm_min);
  if (topKey != sTopKey) {
    sTopKey = topKey;
    lcd.fillRect(ZONE_TOP.x, ZONE_TOP.y, ZONE_TOP.w, ZONE_TOP.h, BG);
    lcd.setFont(FONT_CN);
    lcd.setTextSize(1);
    lcd.setTextColor(INK2, BG);
    lcd.setTextDatum(middle_left);
    const char* wk = WEEK_CN[tm.tm_wday];
    lcd.drawString(wk, TOP_X_L, TOP_Y);
    char dat[12];
    snprintf(dat, sizeof dat, "%02d/%02d", tm.tm_mon + 1, tm.tm_mday);
    lcd.drawString(dat, TOP_X_L + lcd.textWidth(wk) + TOP_GAP, TOP_Y);

    if (d.city && d.city[0]) {
      lcd.setTextDatum(middle_right);
      drawTextClamped(d.city, TOP_X_R, TOP_Y, 90);
    }
  }

  // ---- 时钟：逐位脏检测，只重绘变化位 ----
  const SegGeom segClock{SEG_W, SEG_H, SEG_T, SEG_GAP};
  int xs[4] = { SEG_X0,
                SEG_X0 + SEG_W + DIG_GAP,
                COLON_CX + COL_GAP / 2,
                COLON_CX + COL_GAP / 2 + SEG_W + DIG_GAP };
  int digits[4] = { tm.tm_hour / 10, tm.tm_hour % 10, tm.tm_min / 10, tm.tm_min % 10 };
  for (int i = 0; i < 4; i++) {
    if (digits[i] != sDigit[i]) {
      lcd.fillRect(xs[i], SEG_Y0, SEG_W, SEG_H, BG);
      segDigit(xs[i], SEG_Y0, digits[i], segClock, INK);
      sDigit[i] = digits[i];
    }
  }
  if (blinkColon != sBlink) {
    lcd.fillRect(COLON_CX - 6, SEG_Y0 + 14, 12, SEG_H - 28, BG);
    segColonRound(COLON_CX, SEG_Y0, SEG_H, blinkColon, INK);
    sBlink = blinkColon;
  }

  // ---- 天气组：key 变化才重绘 ----
  uint32_t wk = 0;
  if (d.weather && d.weather->ok) {
    const WeatherData& w = *d.weather;
    wk = ((uint32_t)w.icon << 24) ^ ((uint32_t)((int)(w.temp * 10)) << 12) ^
         ((uint32_t)w.humidity << 4) ^ (uint32_t)((int)(w.feels * 10)) ^
         ((uint32_t)w.text.length() << 2) ^ 1;
  }
  if (wk != sWKey) {
    sWKey = wk;
    lcd.fillRect(ZONE_WEATHER.x, ZONE_WEATHER.y, ZONE_WEATHER.w, ZONE_WEATHER.h, BG);
    if (wk) {
      const WeatherData& w = *d.weather;

      // 左列上行：剪影图标（Modern 多彩柔和配色）
      weatherIconSil(w.icon, W_ICON_CX, W_ICON_CY, BG,
                     SUN, MOON, CLOUD, CLOUD_D, RAIN, SNOW, FOG, SUN);

      // 左列下行：天气文字
      const char* desc = w.text.length() ? w.text.c_str() : weatherTextCN(w.icon);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_center);
      lcd.setTextColor(INK2, BG);
      int descX = W_ICON_CX;
      int descW = lcd.textWidth(desc);
      if (descX - descW / 2 < 14) descX = 14 + descW / 2;
      lcd.drawString(desc, descX, W_TEXT_Y);

      // 右列上行：温度七段
      drawTempSegments((int)lroundf(w.temp), TEMP_XR, TEMP_CY - TSEG_H / 2);

      // 右列下行：湿度 / 体感
      char mbuf[40];
      snprintf(mbuf, sizeof mbuf, "湿度 %d%% / 体感 %d\xC2\xB0",
               w.humidity, (int)lroundf(w.feels));
      lcd.setFont(FONT_CN);
      lcd.setTextDatum(middle_right);
      lcd.setTextColor(INK3, BG);
      lcd.drawString(mbuf, W_META_XR, W_META_Y);
    }
  }

  // ---- 页脚行情：key 变化才重绘 ----
  uint32_t qk = 0;
  if (d.quote && d.quote->ok) {
    const QuoteData& q = *d.quote;
    qk = (uint32_t)(q.price * 100) ^ ((uint32_t)((int)(q.changePct * 100)) << 3) ^
         ((uint32_t)q.label.length() << 20) ^ ((uint32_t)q.unit.length() << 24) ^
         ((uint32_t)q.decimals << 28) ^ 1;
  }
  if (qk != sQKey) {
    sQKey = qk;
    lcd.fillRect(ZONE_QUOTE.x, ZONE_QUOTE.y, ZONE_QUOTE.w, ZONE_QUOTE.h, BG);
    if (qk) {
      const QuoteData& q = *d.quote;
      lcd.fillRect(FOOT_RULE_X, FOOT_RULE_Y, FOOT_RULE_W, 1, HAIRLINE);

      bool hasPct = (q.changePct > 0.005f || q.changePct < -0.005f);
      char cbuf[16];
      int pctW = 0;
      if (hasPct) {
        snprintf(cbuf, sizeof cbuf, "%+.2f%%", q.changePct);
        lcd.setFont(FONT_SMALL);
        pctW = lcd.textWidth(cbuf);
      }

      char pbuf[24];
      snprintf(pbuf, sizeof pbuf, "%.*f", q.decimals, q.price);

      lcd.setFont(FONT_CN);
      int labelW = q.label.length() ? lcd.textWidth(q.label) : 0;
      int unitW  = q.unit.length()  ? lcd.textWidth(q.unit)  : 0;
      lcd.setFont(FONT_SMALL);
      int priceW = lcd.textWidth(pbuf);

      int limit = hasPct ? (Q_X_R - pctW - 10) : Q_X_R;
      int gaps  = (labelW > 0 ? 1 : 0) + (unitW > 0 ? 1 : 0);
      int gap = 6;
      if (gaps > 0) {
        int avail = (limit - Q_X_L) - (labelW + priceW + unitW);
        gap = constrain(avail / gaps, 2, 6);
      }

      int x = Q_X_L;
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);

      if (labelW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(INK3, BG);
        lcd.drawString(q.label, x, Q_Y);
        x += labelW + gap;
      }

      lcd.setFont(FONT_SMALL);
      lcd.setTextColor(INK2, BG);
      lcd.drawString(pbuf, x, Q_Y + 1);
      x += priceW + (unitW > 0 ? gap : 0);

      if (unitW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(INK3, BG);
        lcd.drawString(q.unit, x, Q_Y);
      }

      if (hasPct) {
        lcd.setFont(FONT_SMALL);
        lcd.setTextColor(q.changePct > 0 ? UP : DOWN, BG);
        lcd.setTextDatum(middle_right);
        lcd.drawString(cbuf, Q_X_R, Q_Y + 1);
      }
    }
  }
}
