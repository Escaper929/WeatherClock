// ============================================================================
//  theme_pixel.cpp - Classic Platformer 主题：经典横版平台游戏 HUD
//
//  （由原 Pixel 深蓝夜幕主题整体重做；主题 ID 不变，NVS 兼容）
//  定位：一眼联想到经典 8/16-bit 横版平台冒险 —— 明亮天空蓝场景 +
//        白字深描边的游戏计时 HUD + 金币冒号 + 像素云/丘陵/草地/砖块 +
//        像素对话框天气卡 + 游戏状态栏行情。全部代码绘制，无位图资源。
//
//  布局（y 自上而下）：
//    HUD 顶栏 0..27（深蓝条：左 SAT 12 SEP / 右 城市）→
//    游戏计时数字 38..88（7x10 cell5 白字深蓝 8 向描边，两侧像素云）→
//    天气状态卡 94..174（白框深底：左 40x40 像素图标 / 右 温度+°C，
//      下 天气英文 / 湿度·体感行）→
//    场景带 174..（丘陵 / 问号砖 / 砖块 / 草地 / 土地）→
//    行情状态条 204..232（深底白框：金币 + 标签 + 价格 + 单位 + 涨跌）
//
//  动画：金币冒号与问号砖随 blinkColon 逐秒闪动（均为小脏区，无整屏刷新）。
// ============================================================================
#include <LovyanGFX.hpp>
#include "themes/ThemeShared.h"
using namespace thm;

// ---- Platformer 调色板（明亮但不刺眼，全部集中于此） ----
static const uint16_t SKY     = ui::rgb565( 92, 146, 244);  // 天空蓝
static const uint16_t DEEP    = ui::rgb565( 18,  30,  68);  // 深蓝（HUD 条/卡底/描边）
static const uint16_t WHITE   = ui::rgb565(252, 252, 248);  // 奶油白（时间/HUD 文字）
static const uint16_t DIM_L   = ui::rgb565(150, 190, 250);  // 浅蓝（卡内辅助文字）
static const uint16_t CLOUD   = ui::rgb565(246, 250, 252);  // 云白
static const uint16_t CLOUD_D = ui::rgb565(178, 202, 240);  // 云阴影
static const uint16_t HILL    = ui::rgb565( 88, 168,  72);  // 丘陵绿
static const uint16_t HILL_D  = ui::rgb565( 56, 128,  52);  // 丘陵暗部
static const uint16_t GRASS   = ui::rgb565( 96, 184,  80);  // 草地
static const uint16_t EARTH   = ui::rgb565(150, 102,  62);  // 土地
static const uint16_t EARTH_D = ui::rgb565(110,  72,  44);  // 土地暗斑
static const uint16_t BRICK   = ui::rgb565(188, 100,  64);  // 砖红
static const uint16_t BRICK_D = ui::rgb565(136,  64,  40);  // 砖描边
static const uint16_t COIN    = ui::rgb565(252, 204,  64);  // 金币黄
static const uint16_t COIN_D  = ui::rgb565(216, 140,  32);  // 金币橙影
static const uint16_t COIN_H  = ui::rgb565(255, 240, 160);  // 金币高光
static const uint16_t RAIN_C  = ui::rgb565( 88, 148, 240);  // 雨滴蓝
static const uint16_t BOLT_C  = ui::rgb565(255, 232,  80);  // 闪电亮黄
static const uint16_t SNOW_C  = ui::rgb565(240, 248, 255);  // 雪白
static const uint16_t FOG_C   = ui::rgb565(210, 216, 224);  // 雾灰白
static const uint16_t UP      = ui::rgb565(244, 120,  80);  // 涨（红橙）
static const uint16_t DOWN    = ui::rgb565( 96, 220, 120);  // 跌（明亮绿）

static const lgfx::IFont* const FONT_CN = &fonts::efontCN_12;
static const lgfx::IFont* const FONT_SM = &fonts::FreeMonoBold9pt7b;

static const char* const MON_EN[12] = {
  "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"
};

// ---- 布局常量 ----
static const int CLK_Y = 38;                   // 计时数字顶部（7x10 cell5 = 35x50）
static const int CLK_CELL = 5;
static const int CLK_XS[4] = { 41, 78, 129, 166 };
static const int COLON_CX = 121;               // 冒号方点中心
static const int CARD_X = 20, CARD_Y = 94, CARD_W = 200, CARD_H = 80;
static const int ICON_CX = 46, ICON_CY = 120;  // 天气图标中心（40x40）
static const int TEMP_TOP = 100;               // 温度点阵顶部（cell4 = 28 高）
static const int WEN_Y  = 148;                 // 天气英文行中线
static const int HUM_Y  = 164;                 // 湿度/体感行中线
static const int Q_Y    = 218;                 // 行情条文字中线

// ---- 脏检测 ----
static uint32_t sEpoch = 0;
static uint16_t sTopKey = 0xFFFF;
static uint16_t sClockKey = 0xFFFF;
static bool     sBlink = false;
static uint32_t sWKey = 0, sQKey = 0;

static void resetDirty() {
  sTopKey = 0xFFFF; sClockKey = 0xFFFF;
  sBlink = false; sWKey = 0; sQKey = 0;
}

// ---------------------------------------------------------------------------
// 天气码归并 -> 游戏图标类型
// ---------------------------------------------------------------------------
enum { I_SUN, I_PARTLY, I_OVERCAST, I_RAIN, I_THUNDER, I_SNOW, I_FOG };
static int mapGameIcon(int code) {
  if (code == 100 || code == 150) return I_SUN;
  if (code == 101 || code == 151 || code == 102 || code == 152 ||
      code == 103 || code == 153) return I_PARTLY;
  if (code == 104 || code == 154) return I_OVERCAST;
  if ((code >= 302 && code <= 304) || (code >= 312 && code <= 318)) return I_THUNDER;
  if (code >= 300 && code <= 399) return I_RAIN;
  if (code >= 400 && code <= 499) return I_SNOW;
  if (code >= 500 && code <= 599) return I_FOG;
  return I_OVERCAST;
}

static const char* weatherTextEN(int code) {
  switch (mapGameIcon(code)) {
    case I_SUN:     return "SUNNY";
    case I_PARTLY:  return "PARTLY CLOUDY";
    case I_RAIN:    return "RAIN";
    case I_THUNDER: return "THUNDERSTORM";
    case I_SNOW:    return "SNOW";
    case I_FOG:     return "FOGGY";
    default:        return "CLOUDY";
  }
}

// ---------------------------------------------------------------------------
// 像素游戏元素（块状程序化绘制，无位图）
// ---------------------------------------------------------------------------
// 像素云：x,y 为左上，s=1 小 / 2 中
static void drawCloud(int x, int y, int s) {
  int w = 12 + s * 8;
  lcd.fillRect(x, y + 4 + s, w, 6, CLOUD);            // 底
  lcd.fillRect(x + 3, y + s, w - 9, 6 + s, CLOUD);    // 顶中
  lcd.fillRect(x + w - 7, y + 2 + s, 5, 4 + s, CLOUD);// 右肩
  lcd.fillRect(x + 1, y + 8 + 2 * s, w - 2, 2, CLOUD_D); // 底影
}

// 太阳（满圆盘 + 8 光线 + 橙影），cx,cy 为盘心
static void drawSunAt(int cx, int cy) {
  lcd.fillRect(cx - 1, cy - 16, 2, 5, COIN);   // 上光线
  lcd.fillRect(cx - 1, cy + 11, 2, 5, COIN);   // 下
  lcd.fillRect(cx - 16, cy - 1, 5, 2, COIN);   // 左
  lcd.fillRect(cx + 11, cy - 1, 5, 2, COIN);   // 右
  lcd.fillRect(cx - 11, cy - 11, 3, 3, COIN);  // 对角
  lcd.fillRect(cx + 8,  cy - 11, 3, 3, COIN);
  lcd.fillRect(cx - 11, cy + 8,  3, 3, COIN);
  lcd.fillRect(cx + 8,  cy + 8,  3, 3, COIN);
  lcd.fillRect(cx - 3, cy - 6, 6, 12, COIN);   // 圆盘（十字+方块近似）
  lcd.fillRect(cx - 6, cy - 3, 12, 6, COIN);
  lcd.fillRect(cx - 5, cy - 5, 10, 10, COIN);
  lcd.fillRect(cx + 1, cy + 1, 4, 4, COIN_D);  // 右下橙影
  lcd.fillRect(cx + 3, cy - 2, 2, 3, COIN_D);
  lcd.fillRect(cx - 2, cy + 3, 3, 2, COIN_D);
}

// 像素金币 10x10，x,y 左上
static void drawCoin(int x, int y) {
  lcd.fillRect(x + 2, y, 6, 10, COIN);
  lcd.fillRect(x, y + 2, 10, 6, COIN);
  lcd.fillRect(x + 1, y + 1, 8, 8, COIN);
  lcd.fillRect(x + 3, y + 2, 2, 6, COIN_H);    // 高光竖条
  lcd.fillRect(x + 7, y + 3, 1, 4, COIN_D);    // 右缘暗
  lcd.fillRect(x + 2, y + 7, 6, 1, COIN_D);    // 底缘暗
}

// 经典砖块 16x10，x,y 左上
static void drawBrick(int x, int y) {
  lcd.fillRect(x, y, 16, 10, BRICK_D);
  lcd.fillRect(x + 1, y + 1, 14, 8, BRICK);
  lcd.drawFastHLine(x + 1, y + 4, 14, BRICK_D);  // 横缝
  lcd.drawFastVLine(x + 7, y + 1, 3, BRICK_D);   // 上排竖缝
  lcd.drawFastVLine(x + 4, y + 6, 3, BRICK_D);   // 下排竖缝错位
}

// 问号砖 14x14：黄底 + 闪烁问号
static void drawQBlock(int x, int y, bool lit) {
  lcd.fillRect(x, y, 14, 14, BRICK_D);
  lcd.fillRect(x + 1, y + 1, 12, 12, COIN);
  lcd.fillRect(x + 3, y + 3, 8, 8, COIN_H);
  lcd.fillRect(x + 4, y + 4, 6, 6, COIN);
  lcd.setFont(FONT_SM);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(lit ? WHITE : DEEP, COIN);
  lcd.drawString("?", x + 7, y + 8);
}

// 阶梯丘陵：cx 中心，baseY 底缘，w 底宽，h 高（3 级台阶）
static void drawHill(int cx, int baseY, int w, int h) {
  int st = h / 3;
  lcd.fillRect(cx - w / 2, baseY - st, w, st, HILL);
  lcd.fillRect(cx - w / 3, baseY - 2 * st, 2 * w / 3, st, HILL);
  lcd.fillRect(cx - w / 6, baseY - h, w / 3, st, HILL);
  lcd.fillRect(cx - w / 6, baseY - h, w / 3, 2, HILL_D);  // 顶缘暗部
}

// ---------------------------------------------------------------------------
// 游戏像素图标（40x40 逻辑区，cx,cy 中心）—— 与数据逻辑无关，纯视觉
// ---------------------------------------------------------------------------
static void drawGameIcon(int code, int cx, int cy) {
  switch (mapGameIcon(code)) {
    case I_SUN:
      drawSunAt(cx, cy);
      break;
    case I_PARTLY: {
      drawSunAt(cx - 7, cy - 7);
      drawCloud(cx - 14, cy + 6, 2);
      break;
    }
    case I_OVERCAST:
      drawCloud(cx - 18, cy - 14, 2);
      drawCloud(cx - 8, cy - 2, 1);
      break;
    case I_RAIN:
      drawCloud(cx - 18, cy - 16, 2);
      for (int i = 0; i < 3; i++)
        lcd.fillRect(cx - 10 + i * 9, cy + 2 + (i % 2) * 5, 2, 6, RAIN_C);
      break;
    case I_THUNDER:
      drawCloud(cx - 18, cy - 16, 2);
      lcd.fillRect(cx + 1, cy + 1, 2, 4, BOLT_C);   // Z 形闪电
      lcd.fillRect(cx - 2, cy + 4, 5, 3, BOLT_C);
      lcd.fillRect(cx, cy + 6, 2, 5, BOLT_C);
      lcd.fillRect(cx - 2, cy + 11, 3, 2, BOLT_C);
      break;
    case I_SNOW: {
      drawCloud(cx - 18, cy - 16, 2);
      for (int i = 0; i < 3; i++) {                  // 3 个十字雪花
        int sx = cx - 10 + i * 10, sy = cy + 6 + (i % 2) * 6;
        lcd.fillRect(sx - 1, sy - 3, 3, 7, SNOW_C);
        lcd.fillRect(sx - 3, sy - 1, 7, 3, SNOW_C);
      }
      break;
    }
    case I_FOG:
      lcd.fillRect(cx - 16, cy - 10, 30, 3, FOG_C);
      lcd.fillRect(cx - 12, cy - 4, 32, 3, FOG_C);
      lcd.fillRect(cx - 16, cy + 2, 26, 3, FOG_C);
      lcd.fillRect(cx - 8, cy + 8, 28, 3, FOG_C);
      break;
  }
}

// ---------------------------------------------------------------------------
// 场景与静态框架（仅 epoch 绘制一次）
// ---------------------------------------------------------------------------
static void drawScene() {
  // 两侧静态像素云（避开计时数字重绘区 x 39..203）
  drawCloud(6, 40, 1);
  drawCloud(206, 48, 1);
  // 丘陵（草顶之上）
  drawHill(70, 192, 96, 18);
  drawHill(196, 192, 64, 12);
  // 草地 + 草齿
  lcd.fillRect(0, 192, 240, 8, GRASS);
  for (int x = 0; x < 240; x += 8) lcd.fillRect(x + ((x / 8) % 2) * 4, 189, 4, 3, GRASS);
  // 土地 + 暗斑
  lcd.fillRect(0, 200, 240, 40, EARTH);
  lcd.fillRect(24, 234, 3, 2, EARTH_D);  lcd.fillRect(76, 236, 3, 2, EARTH_D);
  lcd.fillRect(160, 235, 3, 2, EARTH_D); lcd.fillRect(212, 234, 3, 2, EARTH_D);
  // 砖块装饰（丘陵两侧脚）+ 问号砖（中央天空带）
  drawBrick(14, 178);
  drawBrick(210, 178);
  drawQBlock(113, 175, false);
}

static void drawChrome() {
  // HUD 顶栏
  lcd.fillRect(0, 0, 240, 28, DEEP);
  // 天气状态卡：白 2px 像素框 + 深底（经典对话框）
  lcd.fillRect(CARD_X, CARD_Y, CARD_W, CARD_H, WHITE);
  lcd.fillRect(CARD_X + 2, CARD_Y + 2, CARD_W - 4, CARD_H - 4, DEEP);
  // 行情状态条：白 1px 框 + 深底
  lcd.fillRect(8, 204, 224, 28, DEEP);
  lcd.drawRect(8, 204, 224, 28, WHITE);
}

// 游戏计时数字：7x10 cell5 白字 + 右下 2px 深蓝投影（经典 HUD 计时风）；
// 冒号为两枚亮黄像素方点（随 blink 帧）
static void drawClockDigits(int h, int m) {
  lcd.fillRect(38, 32, 166, 62, SKY);            // 整条清除（含右下投影）
  int digits[4] = { h / 10, h % 10, m / 10, m % 10 };
  for (int i = 0; i < 4; i++) {
    pixDigit7x10(CLK_XS[i] + 2, CLK_Y + 2, digits[i], CLK_CELL, DEEP);  // 投影
    pixDigit7x10(CLK_XS[i], CLK_Y, digits[i], CLK_CELL, WHITE);         // 本体
  }
  if (sBlink) {                                  // 冒号方点（亮帧）
    lcd.fillRect(COLON_CX - 3, 52, 6, 6, COIN);
    lcd.fillRect(COLON_CX - 3, 74, 6, 6, COIN);
  }
}

void pixelTick(const UiData& d, bool blinkColon) {
  uint32_t ep = themeEpoch();
  if (ep != sEpoch) {
    sEpoch = ep;
    lcd.fillScreen(SKY);
    drawScene();
    drawChrome();
    resetDirty();
  }

  const struct tm& tm = d.dt;

  // ---- HUD 顶栏：左 SAT 12 SEP / 右 城市（每分钟重绘） ----
  uint16_t topKey = (uint16_t)((tm.tm_wday << 8) | tm.tm_min);
  if (topKey != sTopKey) {
    sTopKey = topKey;
    lcd.fillRect(10, 6, 220, 16, DEEP);
    lcd.setFont(FONT_SM);
    lcd.setTextSize(1);
    char db[16];
    snprintf(db, sizeof db, "%s %d %s", WEEK_EN[tm.tm_wday], tm.tm_mday,
             MON_EN[tm.tm_mon]);
    lcd.setTextDatum(middle_left);
    lcd.setTextColor(WHITE, DEEP);
    lcd.drawString(db, 12, 14);
    lcd.setFont(FONT_CN);
    lcd.setTextDatum(middle_right);
    drawTextClamped(d.city && d.city[0] ? d.city : "--", 228, 14, 100);
  }

  // ---- 游戏计时数字（任一位变化即整条重绘，分钟粒度） ----
  {
    uint16_t ck = (uint16_t)((tm.tm_hour << 6) | tm.tm_min);
    if (ck != sClockKey) {
      sClockKey = ck;
      drawClockDigits(tm.tm_hour, tm.tm_min);
    }
  }

  // ---- 冒号方点 + 问号砖（逐秒轻闪，小脏区） ----
  if (blinkColon != sBlink) {
    sBlink = blinkColon;
    lcd.fillRect(116, 44, 10, 44, SKY);          // 冒号小区清除（避开数字与投影）
    if (blinkColon) {
      lcd.fillRect(COLON_CX - 3, 52, 6, 6, COIN);
      lcd.fillRect(COLON_CX - 3, 74, 6, 6, COIN);
    }
    drawQBlock(113, 175, blinkColon);
  }

  // ---- 天气状态卡内容：图标 + 温度 + 天气英文 + 湿度/体感 ----
  uint32_t wk = 0;
  if (d.weather && d.weather->ok) {
    const WeatherData& w = *d.weather;
    wk = ((uint32_t)(w.icon & 0xFF) << 20) ^
         ((uint32_t)((int)(w.temp * 10) & 0x3FF) << 10) ^
         ((uint32_t)(w.humidity & 0x7F) << 2) ^ 1;
  }
  if (wk != sWKey) {
    sWKey = wk;
    lcd.fillRect(CARD_X + 4, CARD_Y + 4, CARD_W - 8, CARD_H - 8, DEEP);  // 卡内清底
    if (wk) {
      const WeatherData& w = *d.weather;
      drawGameIcon(w.icon, ICON_CX, ICON_CY);

      // 温度（点阵 cell4 白字），右缘给 °C 留位
      int tv = (int)lroundf(w.temp);
      char tb[5];
      if (tv < 0) snprintf(tb, sizeof tb, "-%d", -tv);
      else        snprintf(tb, sizeof tb, "%d", tv);
      int tw = pixNum5x7Width(tb, 4);
      pixNum5x7(186 - tw, TEMP_TOP, tb, 4, WHITE);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(DIM_L, DEEP);
      lcd.drawString("\xC2\xB0""C", 190, TEMP_TOP + 14);

      // 天气英文（金币黄，HUD 标签感）
      lcd.setFont(FONT_SM);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(COIN, DEEP);
      drawTextClamped(weatherTextEN(w.icon), 84, WEN_Y, 214 - 84);

      // 底行：湿度（左）/ 体感（右），中文 efont（° 字形可靠）
      char hb[16], fb[16];
      snprintf(hb, sizeof hb, "\xE6\xB9\xBF\xE5\xBA\xA6 %d%%", w.humidity);
      snprintf(fb, sizeof fb, "\xE4\xBD\x93\xE6\x84\x9F %d\xC2\xB0", (int)lroundf(w.feels));
      lcd.setFont(FONT_CN);
      lcd.setTextColor(DIM_L, DEEP);
      lcd.setTextDatum(middle_left);
      lcd.drawString(hb, 84, HUM_Y);
      lcd.setTextDatum(middle_right);
      lcd.drawString(fb, 214, HUM_Y);
    }
  }

  // ---- 行情状态条：金币 + 标签 + 价格 + 单位 | 涨跌 ----
  uint32_t qk = 0;
  if (d.quote && d.quote->ok) {
    const QuoteData& q = *d.quote;
    qk = (uint32_t)(q.price * 100) ^ ((uint32_t)((int)(q.changePct * 100)) << 3) ^
         ((uint32_t)q.label.length() << 20) ^ ((uint32_t)q.unit.length() << 24) ^
         ((uint32_t)q.decimals << 28) ^ 1;
  }
  if (qk != sQKey) {
    sQKey = qk;
    lcd.fillRect(10, 206, 220, 24, DEEP);          // 条内清底（保留白框）
    if (qk) {
      const QuoteData& q = *d.quote;
      char pbuf[24];
      snprintf(pbuf, sizeof pbuf, "%.*f", q.decimals, q.price);

      bool hasPct = (q.changePct > 0.005f || q.changePct < -0.005f);
      char cbuf[16];
      int pctW = 0;
      lcd.setFont(FONT_SM);
      if (hasPct) {
        snprintf(cbuf, sizeof cbuf, "%+.2f%%", q.changePct);
        pctW = pixNum5x7Width(cbuf, 2);
      }

      lcd.setFont(FONT_CN);
      int labelW = q.label.length() ? lcd.textWidth(q.label) : 0;
      int unitW  = q.unit.length()  ? lcd.textWidth(q.unit)  : 0;
      int priceW = pixNum5x7Width(pbuf, 2);
      int limit  = hasPct ? (224 - pctW - 8) : 224;
      int gaps   = (labelW > 0 ? 1 : 0) + (unitW > 0 ? 1 : 0);
      int gap = 5;
      if (gaps > 0) {
        int avail = (limit - 30) - (labelW + priceW + unitW + 8);
        gap = constrain(avail / gaps, 3, 8);
      }

      drawCoin(14, Q_Y - 5);                       // 金币图标
      int x = 30;
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      if (labelW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(WHITE, DEEP);
        drawTextClamped(q.label.c_str(), x, Q_Y, 72);
        x += labelW + gap;
      }
      lcd.setFont(FONT_SM);                        // 价格：金币黄点阵
      pixNum5x7(x, Q_Y - 7, pbuf, 2, COIN);
      x += priceW + 6 + (unitW > 0 ? gap : 0);
      if (unitW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(DIM_L, DEEP);
        drawTextClamped(q.unit.c_str(), x, Q_Y, limit - x);
      }
      if (hasPct) {
        pixNum5x7(224 - pctW, Q_Y - 7, cbuf, 2, q.changePct > 0 ? UP : DOWN);
      }
    }
  }
}
