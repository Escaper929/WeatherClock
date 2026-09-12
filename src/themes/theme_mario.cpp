// ============================================================================
//  theme_mario.cpp - Mario 主题：马里奥配色平台游戏 HUD（高饱和版）
//
//  与 Classic Platformer（theme_pixel.cpp）同一布局骨架，按视觉层级用色：
//  天空蓝大面积背景 / 绿色草地丘陵 / 棕色砖块土地 / 红色只做 HUD 条与涨价焦点 /
//  黄色做金币·温度·价格·问号砖 / 米白做文字云高光 / Ink Brown 全局描边与阴影。
//  主题 ID THEME_MARIO=4，独立于 Platformer（=2），二者可并存切换。
//
//  布局（与 Platformer 相同）：
//    HUD 顶栏 0..29（Mario Red 条：左 SAT 12 SEP / 右 城市，米白字+深棕描边）→
//    游戏计时数字 38..91（7x10 cell5 云朵白 + Ink Brown 2px 描边 + 暗红投影）→
//    天气状态卡 94..174（Cloud 外圈 + Ink 框 + Deep Blue 底：左 小天空窗像素图标 /
//      右 温度(Coin 黄)+°C(米白)，下 天气英文(米白) / 湿度·体感(米白)）→
//    场景带 174..（丘陵 / 问号砖 / 砖块 / 草地 / 土地）→
//    行情状态条 204..232（砖墙：金币+米白文字+价格(Coin 黄 描边)+涨跌）
//
//  动画：冒号方点与问号砖随 blinkColon 逐秒闪动（小脏区，无整屏刷新）。
// ============================================================================
#include <LovyanGFX.hpp>
#include "themes/ThemeShared.h"
using namespace thm;

// ---- 马里奥特征色板（13 色全部集中于此） ----
static const uint16_t SKY     = ui::rgb565( 92, 200, 255);  // #5CC8FF 天空主背景
static const uint16_t SKY_D   = ui::rgb565( 35, 135, 217);  // #2387D9 天空阴影/边界
static const uint16_t CLOUD   = ui::rgb565(255, 247, 223);  // #FFF7DF 云朵/部分文字
static const uint16_t GRASS   = ui::rgb565( 69, 200,  61);  // #45C83D 草地
static const uint16_t GRASS_D = ui::rgb565( 24, 133,  45);  // #18852D 草地阴影/描边
static const uint16_t RED     = ui::rgb565(229,  37,  33);  // #E52521 主强调/重点数字
static const uint16_t RED_D   = ui::rgb565(169,  20,  24);  // #A91418 红色阴影
static const uint16_t COIN    = ui::rgb565(255, 216,  61);  // #FFD83D 金币/重点信息
static const uint16_t COIN_D  = ui::rgb565(217, 154,   0);  // #D99A00 黄色阴影
static const uint16_t BRICK   = ui::rgb565(184,  92,  50);  // #B85C32 砖块/土地
static const uint16_t BRICK_D = ui::rgb565(113,  53,  31);  // #71351F 砖块阴影
static const uint16_t DEEP    = ui::rgb565( 20,  82, 154);  // #14529A 游戏 HUD/辅助
static const uint16_t INK     = ui::rgb565( 69,  35,  19);  // #452313 全主题统一深色描边

static const lgfx::IFont* const FONT_CN = &fonts::efontCN_12;
static const lgfx::IFont* const FONT_SM = &fonts::FreeMonoBold9pt7b;

static const char* const MON_EN[12] = {
  "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"
};

// ---- 布局常量（与 Platformer 一致） ----
static const int CLK_Y = 38;                   // 计时数字顶部（7x10 cell5 = 35x50）
static const int CLK_CELL = 5;
static const int CLK_XS[4] = { 41, 78, 129, 166 };
static const int COLON_CX = 121;               // 冒号方点中心
static const int CARD_X = 20, CARD_Y = 94, CARD_W = 200, CARD_H = 80;
static const int ICON_CX = 46, ICON_CY = 122;  // 图标窗内图标中心（44x48 窗）
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
// 像素游戏元素（块状程序化绘制，特征色版）
// ---------------------------------------------------------------------------
// 像素云：Cloud 白 + Sky Dark 阴影；x,y 左上，s=1 小 / 2 中
static void drawCloud(int x, int y, int s) {
  int w = 12 + s * 8;
  lcd.fillRect(x, y + 4 + s, w, 6, CLOUD);
  lcd.fillRect(x + 3, y + s, w - 9, 6 + s, CLOUD);
  lcd.fillRect(x + w - 7, y + 2 + s, 5, 4 + s, CLOUD);
  lcd.fillRect(x + 1, y + 8 + 2 * s, w - 2, 2, SKY_D);
}

// 太阳（Coin 黄圆盘 + Coin Orange 阴影 + 8 光线），cx,cy 盘心
static void drawSunAt(int cx, int cy) {
  lcd.fillRect(cx - 1, cy - 16, 2, 5, COIN);
  lcd.fillRect(cx - 1, cy + 11, 2, 5, COIN);
  lcd.fillRect(cx - 16, cy - 1, 5, 2, COIN);
  lcd.fillRect(cx + 11, cy - 1, 5, 2, COIN);
  lcd.fillRect(cx - 11, cy - 11, 3, 3, COIN);
  lcd.fillRect(cx + 8,  cy - 11, 3, 3, COIN);
  lcd.fillRect(cx - 11, cy + 8,  3, 3, COIN);
  lcd.fillRect(cx + 8,  cy + 8,  3, 3, COIN);
  lcd.fillRect(cx - 3, cy - 6, 6, 12, COIN);
  lcd.fillRect(cx - 6, cy - 3, 12, 6, COIN);
  lcd.fillRect(cx - 5, cy - 5, 10, 10, COIN);
  lcd.fillRect(cx + 1, cy + 1, 4, 4, COIN_D);
  lcd.fillRect(cx + 3, cy - 2, 2, 3, COIN_D);
  lcd.fillRect(cx - 2, cy + 3, 3, 2, COIN_D);
}

// 像素金币 10x10：Coin 黄 + Coin Orange 阴影 + Cloud 高光
static void drawCoin(int x, int y) {
  lcd.fillRect(x + 2, y, 6, 10, COIN);
  lcd.fillRect(x, y + 2, 10, 6, COIN);
  lcd.fillRect(x + 1, y + 1, 8, 8, COIN);
  lcd.fillRect(x + 3, y + 2, 2, 6, CLOUD);
  lcd.fillRect(x + 7, y + 3, 1, 4, COIN_D);
  lcd.fillRect(x + 2, y + 7, 6, 1, COIN_D);
}

// 经典砖块 16x10：Brick + Brick Dark 缝
static void drawBrick(int x, int y) {
  lcd.fillRect(x, y, 16, 10, BRICK_D);
  lcd.fillRect(x + 1, y + 1, 14, 8, BRICK);
  lcd.drawFastHLine(x + 1, y + 4, 14, BRICK_D);
  lcd.drawFastVLine(x + 7, y + 1, 3, BRICK_D);
  lcd.drawFastVLine(x + 4, y + 6, 3, BRICK_D);
}

// 问号砖 14x14：Coin 黄底 + Ink 框 + 问号（红/棕交替闪烁）
static void drawQBlock(int x, int y, bool lit) {
  lcd.fillRect(x, y, 14, 14, INK);
  lcd.fillRect(x + 1, y + 1, 12, 12, COIN);
  lcd.fillRect(x + 3, y + 3, 8, 8, CLOUD);
  lcd.fillRect(x + 4, y + 4, 6, 6, COIN);
  lcd.setFont(FONT_SM);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(lit ? RED : INK, COIN);
  lcd.drawString("?", x + 7, y + 8);
}

// 阶梯丘陵：Grass 主体 + Grass Dark 顶缘
static void drawHill(int cx, int baseY, int w, int h) {
  int st = h / 3;
  lcd.fillRect(cx - w / 2, baseY - st, w, st, GRASS);
  lcd.fillRect(cx - w / 3, baseY - 2 * st, 2 * w / 3, st, GRASS);
  lcd.fillRect(cx - w / 6, baseY - h, w / 3, st, GRASS);
  lcd.fillRect(cx - w / 6, baseY - h, w / 3, 2, GRASS_D);
}

// ---------------------------------------------------------------------------
// 游戏像素图标（绘制在小天空窗内：SKY 底上所有元素清晰）
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
        lcd.fillRect(cx - 10 + i * 9, cy + 2 + (i % 2) * 5, 2, 6, DEEP);
      break;
    case I_THUNDER:
      drawCloud(cx - 18, cy - 16, 2);
      lcd.fillRect(cx + 1, cy + 1, 2, 4, COIN);
      lcd.fillRect(cx - 2, cy + 4, 5, 3, COIN);
      lcd.fillRect(cx, cy + 6, 2, 5, COIN);
      lcd.fillRect(cx - 2, cy + 11, 3, 2, COIN);
      break;
    case I_SNOW: {
      drawCloud(cx - 18, cy - 16, 2);
      for (int i = 0; i < 3; i++) {
        int sx = cx - 10 + i * 10, sy = cy + 6 + (i % 2) * 6;
        lcd.fillRect(sx, sy - 2, 3, 7, SKY_D);     // Sky Dark 侧影
        lcd.fillRect(sx - 1, sy - 1, 7, 3, SKY_D);
        lcd.fillRect(sx - 1, sy - 3, 3, 7, CLOUD); // Cloud 白雪花
        lcd.fillRect(sx - 3, sy - 1, 7, 3, CLOUD);
      }
      break;
    }
    case I_FOG:
      lcd.fillRect(cx - 16, cy - 10, 30, 3, CLOUD);
      lcd.fillRect(cx - 12, cy - 4, 32, 3, CLOUD);
      lcd.fillRect(cx - 16, cy + 2, 26, 3, CLOUD);
      lcd.fillRect(cx - 8, cy + 8, 28, 3, CLOUD);
      break;
  }
}

// ---------------------------------------------------------------------------
// 场景与静态框架（仅 epoch 绘制一次）
// ---------------------------------------------------------------------------
static void drawScene() {
  // 两侧静态像素云（避开计时数字重绘区 x 38..204）
  drawCloud(6, 40, 1);
  drawCloud(206, 48, 1);
  // 丘陵（草顶之上）
  drawHill(70, 192, 96, 18);
  drawHill(196, 192, 64, 12);
  // 草地 + 草齿 + 底缘 Grass Dark 描边
  lcd.fillRect(0, 192, 240, 8, GRASS);
  for (int x = 0; x < 240; x += 8) lcd.fillRect(x + ((x / 8) % 2) * 4, 189, 4, 3, GRASS);
  lcd.fillRect(0, 199, 240, 1, GRASS_D);
  // 土地 + Brick Dark 暗斑
  lcd.fillRect(0, 200, 240, 40, BRICK);
  lcd.fillRect(24, 234, 3, 2, BRICK_D);  lcd.fillRect(76, 236, 3, 2, BRICK_D);
  lcd.fillRect(160, 235, 3, 2, BRICK_D); lcd.fillRect(212, 234, 3, 2, BRICK_D);
  // 砖块装饰（丘陵两侧脚）+ 问号砖（中央天空带）
  drawBrick(14, 178);
  drawBrick(210, 178);
  drawQBlock(113, 175, false);
}

// ---------------------------------------------------------------------------
// 小工具：HUD 描边文字 / 描边点阵数字 / 砖墙行情条
// ---------------------------------------------------------------------------
// HUD 条文字：米白 + Ink Brown 描边（透明底，条底由调用方先平铺）
static void drawHudText(const char* s, int x, int y) {
  lcd.setTextColor(INK);
  lcd.drawString(s, x - 1, y);
  lcd.drawString(s, x + 1, y);
  lcd.drawString(s, x, y - 1);
  lcd.drawString(s, x, y + 1);
  lcd.setTextColor(CLOUD);
  lcd.drawString(s, x, y);
}

// 点阵数字 + Ink Brown 1px 描边（砖墙上的价格/涨跌）
static void pixNumOutlined(int x, int y, const char* s, int cell, uint16_t color) {
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
      if (dx || dy) pixNum5x7(x + dx, y + dy, s, cell, INK);
  pixNum5x7(x, y, s, cell, color);
}

// 行情砖墙条：Brick 砖 + Brick Dark 灰浆 + Cloud 外框
static void drawBrickBar() {
  lcd.fillRect(8, 204, 224, 28, BRICK_D);        // 灰浆底
  for (int row = 0; row < 3; row++) {
    int by = 205 + row * 9;
    for (int bx = 9 + (row % 2) * 8; bx < 231; bx += 17) {
      int bw = (bx + 16 <= 231) ? 16 : 231 - bx;
      lcd.fillRect(bx, by, bw, 8, BRICK);
    }
  }
  lcd.drawRect(8, 204, 224, 28, CLOUD);
}

static void drawChrome() {
  // HUD 顶栏：Mario Red 条 + 下缘暗红阴影（红色只做焦点框）
  lcd.fillRect(0, 0, 240, 28, RED);
  lcd.fillRect(0, 28, 240, 2, RED_D);
  // 天气状态卡：Cloud 外圈 + Ink 3px 框 + Deep Blue 底（游戏对话框）
  lcd.drawRect(CARD_X - 1, CARD_Y - 1, CARD_W + 2, CARD_H + 2, CLOUD);
  lcd.fillRect(CARD_X, CARD_Y, CARD_W, CARD_H, INK);
  lcd.fillRect(CARD_X + 3, CARD_Y + 3, CARD_W - 6, CARD_H - 6, DEEP);
  // 行情状态条：砖墙 + Cloud 外框
  drawBrickBar();
}

// 游戏计时数字：7x10 cell5 云朵白 + Ink Brown 2px 描边 + 暗红右下投影
static void drawClockDigits(int h, int m) {
  lcd.fillRect(38, 32, 167, 61, SKY);
  int digits[4] = { h / 10, h % 10, m / 10, m % 10 };
  for (int i = 0; i < 4; i++)                    // 暗红投影（右下 4px）
    pixDigit7x10(CLK_XS[i] + 4, CLK_Y + 4, digits[i], CLK_CELL, RED_D);
  for (int i = 0; i < 4; i++)                    // Ink Brown 2px 描边（8 向）
    for (int dy = -2; dy <= 2; dy += 2)
      for (int dx = -2; dx <= 2; dx += 2)
        if (dx || dy)
          pixDigit7x10(CLK_XS[i] + dx, CLK_Y + dy, digits[i], CLK_CELL, INK);
  for (int i = 0; i < 4; i++)                    // 云朵白本体
    pixDigit7x10(CLK_XS[i], CLK_Y, digits[i], CLK_CELL, CLOUD);
  if (sBlink) {                                  // 冒号方点（Coin 黄）
    lcd.fillRect(COLON_CX - 3, 52, 6, 6, COIN);
    lcd.fillRect(COLON_CX - 3, 74, 6, 6, COIN);
  }
}

void marioTick(const UiData& d, bool blinkColon) {
  uint32_t ep = themeEpoch();
  if (ep != sEpoch) {
    sEpoch = ep;
    lcd.fillScreen(SKY);
    drawScene();
    drawChrome();
    resetDirty();
  }

  const struct tm& tm = d.dt;

  // ---- HUD 顶栏：左 SAT 12 SEP / 右 城市（米白字 + 深棕描边） ----
  uint16_t topKey = (uint16_t)((tm.tm_wday << 8) | tm.tm_min);
  if (topKey != sTopKey) {
    sTopKey = topKey;
    lcd.fillRect(10, 6, 220, 16, RED);
    lcd.setFont(FONT_SM);
    lcd.setTextSize(1);
    char db[16];
    snprintf(db, sizeof db, "%s %d %s", WEEK_EN[tm.tm_wday], tm.tm_mday,
             MON_EN[tm.tm_mon]);
    lcd.setTextDatum(middle_left);
    drawHudText(db, 12, 14);
    lcd.setFont(FONT_CN);
    lcd.setTextDatum(middle_right);
    const char* cty = d.city && d.city[0] ? d.city : "--";
    lcd.setTextColor(INK);                         // 描边（上下左右偏移）
    drawTextClamped(cty, 227, 14, 100);
    drawTextClamped(cty, 229, 14, 100);
    drawTextClamped(cty, 228, 13, 100);
    drawTextClamped(cty, 228, 15, 100);
    lcd.setTextColor(CLOUD);
    drawTextClamped(cty, 228, 14, 100);
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
    lcd.fillRect(117, 44, 9, 44, SKY);           // 清冒号区（保留数字描边/投影）
    if (blinkColon) {
      lcd.fillRect(COLON_CX - 3, 52, 6, 6, COIN);
      lcd.fillRect(COLON_CX - 3, 74, 6, 6, COIN);
    }
    drawQBlock(113, 175, blinkColon);
  }

  // ---- 天气状态卡内容：小天空窗图标 + 温度(红) + 英文 + 湿度/体感 ----
  uint32_t wk = 0;
  if (d.weather && d.weather->ok) {
    const WeatherData& w = *d.weather;
    wk = ((uint32_t)(w.icon & 0xFF) << 20) ^
         ((uint32_t)((int)(w.temp * 10) & 0x3FF) << 10) ^
         ((uint32_t)(w.humidity & 0x7F) << 2) ^ 1;
  }
  if (wk != sWKey) {
    sWKey = wk;
    // 卡内清底：Deep Blue 对话框底
    lcd.fillRect(CARD_X + 3, CARD_Y + 3, CARD_W - 6, CARD_H - 6, DEEP);
    if (wk) {
      const WeatherData& w = *d.weather;
      // 图标小天空窗：SKY 底 + Ink 细框（白云/白雪花在浅蓝天上清晰）
      lcd.fillRect(24, 98, 44, 48, INK);
      lcd.fillRect(25, 99, 42, 46, SKY);
      drawGameIcon(w.icon, ICON_CX, ICON_CY);

      // 温度（点阵 cell4 Coin 黄 + Ink 阴影，游戏 HUD 重点信息），°C 米白
      int tv = (int)lroundf(w.temp);
      char tb[5];
      if (tv < 0) snprintf(tb, sizeof tb, "-%d", -tv);
      else        snprintf(tb, sizeof tb, "%d", tv);
      int tw = pixNum5x7Width(tb, 4);
      pixNum5x7(186 - tw + 1, TEMP_TOP + 1, tb, 4, INK);
      pixNum5x7(186 - tw, TEMP_TOP, tb, 4, COIN);
      lcd.setFont(FONT_CN);
      lcd.setTextSize(1);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(CLOUD, DEEP);
      lcd.drawString("\xC2\xB0""C", 190, TEMP_TOP + 14);

      // 天气英文（米白，标签感）
      lcd.setFont(FONT_SM);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(CLOUD, DEEP);
      drawTextClamped(weatherTextEN(w.icon), 84, WEN_Y, 214 - 84);

      // 底行：湿度（左）/ 体感（右），中文 efont（° 字形可靠）
      char hb[16], fb[16];
      snprintf(hb, sizeof hb, "\xE6\xB9\xBF\xE5\xBA\xA6 %d%%", w.humidity);
      snprintf(fb, sizeof fb, "\xE4\xBD\x93\xE6\x84\x9F %d\xC2\xB0", (int)lroundf(w.feels));
      lcd.setFont(FONT_CN);
      lcd.setTextColor(CLOUD, DEEP);
      lcd.setTextDatum(middle_left);
      lcd.drawString(hb, 84, HUM_Y);
      lcd.setTextDatum(middle_right);
      lcd.drawString(fb, 214, HUM_Y);
    }
  }

  // ---- 行情状态条：砖墙 + 金币 + 标签 + 价格(Coin 黄 描边) + 单位 | 涨跌 ----
  uint32_t qk = 0;
  if (d.quote && d.quote->ok) {
    const QuoteData& q = *d.quote;
    qk = (uint32_t)(q.price * 100) ^ ((uint32_t)((int)(q.changePct * 100)) << 3) ^
         ((uint32_t)q.label.length() << 20) ^ ((uint32_t)q.unit.length() << 24) ^
         ((uint32_t)q.decimals << 28) ^ 1;
  }
  if (qk != sQKey) {
    sQKey = qk;
    drawBrickBar();                                // 重铺砖墙 + Cloud 框
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
        lcd.setTextColor(CLOUD);                   // 标签：米白（融进砖墙）
        drawTextClamped(q.label.c_str(), x, Q_Y, 72);
        x += labelW + gap;
      }
      lcd.setFont(FONT_SM);                        // 价格：Coin 黄 + Ink 描边
      pixNumOutlined(x, Q_Y - 7, pbuf, 2, COIN);
      x += priceW + 6 + (unitW > 0 ? gap : 0);
      if (unitW > 0) {
        lcd.setFont(FONT_CN);
        lcd.setTextColor(CLOUD);                   // 单位：米白
        drawTextClamped(q.unit.c_str(), x, Q_Y, limit - x);
      }
      if (hasPct) {
        pixNumOutlined(224 - pctW, Q_Y - 7, cbuf, 2, q.changePct > 0 ? RED : GRASS);
      }
    }
  }
}
