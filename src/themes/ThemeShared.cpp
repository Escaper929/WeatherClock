// ============================================================================
//  ThemeShared.cpp - 共享绘制原语实现（七段/剪影图标/像素图标/像素数字）
// ============================================================================
#include "ThemeShared.h"

// 显示驱动唯一实例（原 Screen.cpp 迁出，全部主题共用）
WCgfx lcd;

namespace thm {

const char* WEEK_CN[7] = {"周日","周一","周二","周三","周四","周五","周六"};
const char* WEEK_EN[7] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

const char* weatherTextCN(int code) {
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
// 七段数码管（几何与颜色参数化；只画亮段）
// ---------------------------------------------------------------------------
static const uint8_t SEG_FONT[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static inline void segBar(int x, int y, int w, int h, int t, uint16_t col) {
  lcd.fillRoundRect(x, y, w, h, t >= 5 ? 2 : 1, col);
}

void segDigit(int ox, int oy, int n, const SegGeom& g, uint16_t col) {
  const int T  = g.t;
  const int hw = g.w - 2 * (T - 1);
  const int hx = ox + T - 1;
  const int vl = g.h / 2 - T - 2 * g.gap;
  uint8_t m = SEG_FONT[n];

  if (m & 0x01) segBar(hx, oy, hw, T, T, col);                                   // a
  if (m & 0x02) segBar(ox + g.w - T, oy + T + g.gap, T, vl, T, col);             // b
  if (m & 0x04) segBar(ox + g.w - T, oy + g.h - T - g.gap - vl, T, vl, T, col);  // c
  if (m & 0x08) segBar(hx, oy + g.h - T, hw, T, T, col);                         // d
  if (m & 0x10) segBar(ox, oy + g.h - T - g.gap - vl, T, vl, T, col);            // e
  if (m & 0x20) segBar(ox, oy + T + g.gap, T, vl, T, col);                       // f
  if (m & 0x40) segBar(hx, oy + (g.h - T) / 2, hw, T, T, col);                   // g
}

void segMinus(int ox, int oy, const SegGeom& g, uint16_t col) {
  const int T  = g.t;
  const int hw = g.w - 2 * (T - 1);
  segBar(ox + T - 1, oy + (g.h - T) / 2, hw, T, T, col);
}

void segColonSquare(int cx, int oy, int h, bool on, uint16_t col) {
  if (!on) return;
  int s = h >= 48 ? 4 : 3;                      // 方点尺寸随数字大小
  lcd.fillRect(cx - s / 2, oy + h / 4 - s / 2, s, s, col);
  lcd.fillRect(cx - s / 2, oy + (h * 3) / 4 - s / 2, s, s, col);
}

void segColonRound(int cx, int oy, int h, bool on, uint16_t col) {
  if (!on) return;
  lcd.fillRoundRect(cx - 2, oy + 18, 4, 4, 1, col);
  lcd.fillRoundRect(cx - 2, oy + h - 22, 4, 4, 1, col);
}

// ---------------------------------------------------------------------------
// 剪影天气图标（40x40 逻辑边界，中心 cx,cy；颜色全参数化）
// ---------------------------------------------------------------------------
static void silCloud(int cx, int cy, uint16_t col) {
  lcd.fillCircle(cx - 12, cy + 2, 8, col);
  lcd.fillCircle(cx - 3,  cy - 5, 10, col);
  lcd.fillCircle(cx + 8,  cy + 1, 8, col);
  lcd.fillRect(cx - 19, cy + 1, 38, 9, col);
}

static void silSun(int cx, int cy, int r, uint16_t col) {
  lcd.fillCircle(cx, cy, r, col);
  for (int i = 0; i < 8; i++) {
    double a = i * M_PI / 4.0;
    int ca = (int)round(cos(a)), sa = (int)round(sin(a));
    for (int o = 0; o <= 1; o++) {
      int ox = (fabs(ca) < 0.1f) ? 0 : (sa >= 0 ? o : -o);
      int oy = (fabs(sa) < 0.1f) ? 0 : (ca >= 0 ? -o : o);
      lcd.drawLine(cx + (r + 3) * ca + ox, cy + (r + 3) * sa + oy,
                   cx + (r + 7) * ca + ox, cy + (r + 7) * sa + oy, col);
    }
  }
}

static void silMoon(int cx, int cy, int r, uint16_t col, uint16_t bg) {
  lcd.fillCircle(cx, cy, r, col);
  lcd.fillCircle(cx + 4, cy - 3, r - 1, bg);
}

static void silRain(int cx, int cy, int n, int step, uint16_t col) {
  for (int i = 0; i < n; i++) {
    int x = cx + (i - (n - 1) / 2) * step;
    int y = cy + 7;
    lcd.drawLine(x, y, x - 3, y + 8, col);
    lcd.drawLine(x + 1, y, x - 2, y + 8, col);
  }
}

static void silSnowStar(int x, int y, uint16_t col) {
  lcd.drawLine(x, y - 3, x, y + 3, col);
  lcd.drawLine(x - 3, y, x + 3, y, col);
  lcd.drawLine(x - 2, y - 2, x + 2, y + 2, col);
  lcd.drawLine(x - 2, y + 2, x + 2, y - 2, col);
}

void weatherIconSil(int code, int cx, int cy, uint16_t bg,
                    uint16_t sun, uint16_t moon, uint16_t cloud,
                    uint16_t cloudD, uint16_t rain, uint16_t snow,
                    uint16_t fog, uint16_t bolt) {
  bool night = (code >= 150 && code < 200);

  if (code == 100) {
    silSun(cx, cy, 11, sun);
  } else if (code == 150) {
    silMoon(cx, cy, 11, moon, bg);
  } else if (code == 101 || code == 102 || code == 103 ||
             code == 151 || code == 152 || code == 153) {
    if (night) silMoon(cx - 9, cy - 10, 9, moon, bg);
    else       silSun(cx - 9, cy - 10, 7, sun);
    silCloud(cx + 5, cy + 5, cloud);
  } else if (code == 104 || code == 154) {
    silCloud(cx - 5, cy - 4, cloudD);
    silCloud(cx + 4, cy + 5, cloud);
  } else if (code == 302 || code == 303 || code == 304) {
    silCloud(cx, cy - 8, cloud);
    lcd.fillTriangle(cx, cy - 1, cx + 8, cy - 1, cx - 1, cy + 11, bolt);
    lcd.fillTriangle(cx + 8, cy - 1, cx + 1, cy + 10, cx - 1, cy + 11, bolt);
    lcd.fillTriangle(cx + 6, cy + 10, cx - 2, cy + 23, cx + 3, cy + 11, bolt);
  } else if (code == 404 || code == 405) {
    silCloud(cx, cy - 8, cloud);
    silRain(cx - 10, cy, 1, 10, rain);
    silSnowStar(cx, cy + 12, snow);
    silRain(cx + 10, cy, 1, 10, rain);
  } else if (code >= 300 && code < 400) {
    silCloud(cx, cy - 6, cloud);
    silRain(cx, cy, 3, 10, rain);
  } else if (code >= 400 && code < 500) {
    silCloud(cx, cy - 8, cloud);
    silSnowStar(cx - 9, cy + 12, snow);
    silSnowStar(cx, cy + 12, snow);
    silSnowStar(cx + 9, cy + 12, snow);
  } else if (code >= 500 && code < 600) {
    lcd.fillRoundRect(cx - 17, cy - 9, 34, 3, 1, fog);
    lcd.fillRoundRect(cx - 12, cy - 1, 24, 3, 1, fog);
    lcd.fillRoundRect(cx - 16, cy + 7, 32, 3, 1, fog);
  } else {
    silCloud(cx, cy, cloud);
  }
}

// ---------------------------------------------------------------------------
// 像素天气图标：20x20 字符画（'#'=点亮），放大 cell 倍
// ---------------------------------------------------------------------------
enum PixIcon { PXI_SUN, PXI_MOON, PXI_PARTLY, PXI_PARTLY_N, PXI_OVERCAST,
               PXI_RAIN, PXI_THUNDER, PXI_SNOW, PXI_SLEET, PXI_FOG, PXI_CLOUD };

static const char* const PX_ICONS[] = {
  // PXI_SUN
  (const char*)".........##.........",
  (const char*)".........##.........",
  (const char*)".........##.........",
  (const char*)"....................",
  (const char*)"....#..........#....",
  (const char*)"......########......",
  (const char*)".....##########.....",
  (const char*)"....############....",
  (const char*)"....############....",
  (const char*)"##..############..##",
  (const char*)"##..############..##",
  (const char*)"....############....",
  (const char*)"....############....",
  (const char*)".....##########.....",
  (const char*)"......########......",
  (const char*)"....#..........#....",
  (const char*)"....................",
  (const char*)".........##.........",
  (const char*)".........##.........",
  (const char*)".........##.........",
  // PXI_MOON
  (const char*)"....................",
  (const char*)"......#####.........",
  (const char*)"....########........",
  (const char*)"...#########........",
  (const char*)"...#########........",
  (const char*)"..#########.........",
  (const char*)"..########..........",
  (const char*)"..########..........",
  (const char*)"..#######...........",
  (const char*)"..#######...........",
  (const char*)"..#######...........",
  (const char*)"..#######...........",
  (const char*)"..########..........",
  (const char*)"..########..........",
  (const char*)"..#########.........",
  (const char*)"...#########........",
  (const char*)"....########........",
  (const char*)"......#####.........",
  (const char*)"....................",
  (const char*)"....................",
  // PXI_PARTLY
  (const char*)"....................",
  (const char*)"...#.....#..........",
  (const char*)"....#######.........",
  (const char*)"...#########........",
  (const char*)".#############......",
  (const char*)".#############......",
  (const char*)"....#######.........",
  (const char*)"...#.....#..........",
  (const char*)"....................",
  (const char*)".....########.......",
  (const char*)"....##########..##..",
  (const char*)"...################.",
  (const char*)"..##################",
  (const char*)"..##################",
  (const char*)"..##################",
  (const char*)"...################.",
  (const char*)".....##########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  // PXI_PARTLY_N
  (const char*)"....................",
  (const char*)"....###.............",
  (const char*)"...#####............",
  (const char*)"...######...........",
  (const char*)"...######...........",
  (const char*)"....#####...........",
  (const char*)".....###............",
  (const char*)"....................",
  (const char*)".....########.......",
  (const char*)"....##########..##..",
  (const char*)"...###############..",
  (const char*)"..#################.",
  (const char*)"..#################.",
  (const char*)"..#################.",
  (const char*)"...###############..",
  (const char*)".....##########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  // PXI_OVERCAST
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"......#######.......",
  (const char*)".....#########......",
  (const char*)"....###########.....",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)".....###########....",
  (const char*)"....#############...",
  (const char*)"...################.",
  (const char*)".###################",
  (const char*)".###################",
  (const char*)".###################",
  (const char*)"..#################.",
  (const char*)"....###########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  // PXI_RAIN
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"......#######.......",
  (const char*)"....###########.....",
  (const char*)"...#############....",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"....###########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"...##....##....##...",
  (const char*)"..##....##....##....",
  (const char*)"..##....##....##....",
  (const char*)".##....##....##.....",
  (const char*)".##....##....##.....",
  (const char*)"....................",
  (const char*)"...##....##....##...",
  (const char*)"..##....##....##....",
  (const char*)"..##....##....##....",
  // PXI_THUNDER
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"......#######.......",
  (const char*)"....###########.....",
  (const char*)"...#############....",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"....###########.....",
  (const char*)"....................",
  (const char*)"...........####.....",
  (const char*)"..........####......",
  (const char*)".........####.......",
  (const char*)"........#####.......",
  (const char*)".......######.......",
  (const char*)"......#####.........",
  (const char*)".....######.........",
  (const char*)".....####...........",
  (const char*)".....##.............",
  (const char*)"....................",
  // PXI_SNOW
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"......#######.......",
  (const char*)"....###########.....",
  (const char*)"...#############....",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"....###########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"...#......#......#..",
  (const char*)"..###....###....###.",
  (const char*)"...#......#......#..",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  // PXI_SLEET
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"......#######.......",
  (const char*)"....###########.....",
  (const char*)"...#############....",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"....###########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"..##.......#........",
  (const char*)"...##.....###.......",
  (const char*)"...##......#........",
  (const char*)"....##..............",
  (const char*)"...........#........",
  (const char*)"....##....###.......",
  (const char*)"....##.....#........",
  (const char*)".....##.............",
  (const char*)"....................",
  // PXI_FOG
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"..###############...",
  (const char*)"..###############...",
  (const char*)"....................",
  (const char*)"......##########....",
  (const char*)"......##########....",
  (const char*)"....................",
  (const char*)".################...",
  (const char*)".################...",
  (const char*)"....................",
  (const char*)"....############....",
  (const char*)"....############....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  // PXI_CLOUD
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"......#######.......",
  (const char*)"....###########.....",
  (const char*)"...#############....",
  (const char*)"..###############...",
  (const char*)".#################..",
  (const char*)".#################..",
  (const char*)".#################..",
  (const char*)"..################..",
  (const char*)"....###########.....",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
  (const char*)"....................",
};

static PixIcon pixelIconFor(int code) {
  bool night = (code >= 150 && code < 200);
  if (code == 100) return PXI_SUN;
  if (code == 150) return PXI_MOON;
  if (code == 101 || code == 102 || code == 103 || code == 151 || code == 152 || code == 153)
    return night ? PXI_PARTLY_N : PXI_PARTLY;
  if (code == 104 || code == 154) return PXI_OVERCAST;
  if (code == 302 || code == 303 || code == 304) return PXI_THUNDER;
  if (code == 404 || code == 405) return PXI_SLEET;
  if (code >= 300 && code < 400) return PXI_RAIN;
  if (code >= 400 && code < 500) return PXI_SNOW;
  if (code >= 500 && code < 600) return PXI_FOG;
  return PXI_CLOUD;
}

void weatherIconPix(int code, int cx, int cy, uint16_t col, int cell) {
  const char* const* bmp = &PX_ICONS[pixelIconFor(code) * 20];
  const int W = 20 * cell;
  int ox = cx - W / 2, oy = cy - W / 2;
  for (int r = 0; r < 20; r++) {
    const char* row = bmp[r];
    for (int c = 0; c < 20; c++) {
      if (row[c] == '#') lcd.fillRect(ox + c * cell, oy + r * cell, cell, cell, col);
    }
  }
}

// ---------------------------------------------------------------------------
// 像素点阵数字
// ---------------------------------------------------------------------------
// 7x10 大号（时钟）
static const char* const PX_D7[10][10] = {
  {".#####.","##...##","##...##","##...##","##...##","##...##","##...##","##...##","##...##",".#####."}, // 0
  {"...##..","..###..",".####..","...##..","...##..","...##..","...##..","...##..",".######",".######"}, // 1
  {".#####.","##...##","......#",".....#.","....#..","...#...","..#....",".#.....","#######","#######"}, // 2
  {".#####.","##...##","......#",".....#.","..####.",".....#.","......#","......#","##...##",".#####."}, // 3
  {"....##.","...###.","..####.",".#..##.","##..##.","#######","#######","....##.","....##.","....##."}, // 4
  {"#######","#######","##.....","#####..",".####..",".....##","......#","......#","##...##",".#####."}, // 5
  {"..####.",".#...#.","##.....","##.....","######.","##...##","##...##","##...##","##...##",".#####."}, // 6
  {"#######","#######","......#",".....#.","....#..","...#...","...#...","...#...","...#...","...#..."}, // 7
  {".#####.","##...##","##...##","##...##",".#####.","##...##","##...##","##...##","##...##",".#####."}, // 8
  {".#####.","##...##","##...##","##...##",".######","......#","......#",".....#.",".#...#.",".####.."}, // 9
};

static void pixRow(const char* row, int ox, int oy, int cell, uint16_t col) {
  for (int c = 0; c < 7 && row[c]; c++)
    if (row[c] == '#') lcd.fillRect(ox + c * cell, oy, cell, cell, col);
}

int pixDigit7x10(int ox, int oy, int n, int cell, uint16_t col) {
  for (int r = 0; r < 10; r++) pixRow(PX_D7[n][r], ox, oy + r * cell, cell, col);
  return 7 * cell;
}

int pixNum7x10Width(const char* s, int cell) {
  int w = 0;
  for (; *s; s++) {
    if (*s == '.') w += 4 * cell;
    else           w += 8 * cell;   // 7 列 + 1 列间隙
  }
  return w;
}

int pixNum7x10(int ox, int oy, const char* s, int cell, uint16_t col) {
  for (; *s; s++) {
    if (*s == '-') {                                   // 负号：中段横线
      lcd.fillRect(ox, oy + 4 * cell, 6 * cell, cell, col);
      ox += 8 * cell;
    } else if (*s == '.') {                            // 小数点：2x2 底部
      lcd.fillRect(ox, oy + 8 * cell, 2 * cell, 2 * cell, col);
      ox += 4 * cell;
    } else if (*s >= '0' && *s <= '9') {
      pixDigit7x10(ox, oy, *s - '0', cell, col);
      ox += 8 * cell;
    }
  }
  return ox;
}

int pixColon7x10(int ox, int oy, int cell, uint16_t col) {
  lcd.fillRect(ox, oy + 2 * cell, 2 * cell, 2 * cell, col);
  lcd.fillRect(ox, oy + 6 * cell, 2 * cell, 2 * cell, col);
  return 3 * cell;   // 2 列点 + 1 列间隙
}

// 5x7 小号（温度/行情）
static const char* const PX_D5[16][7] = {
  {".###.","#...#","#..##","#.#.#","##..#","#...#",".###."},  // 0
  {"..#..",".##..","..#..","..#..","..#..","..#..",".###."},  // 1
  {".###.","#...#","....#","...#.","..#..",".#...","#####"},  // 2
  {"#####","...#.","..#..","...#.","....#","#...#",".###."},  // 3
  {"...#.","..##.",".#.#.","#..#.","#####","...#.","...#."},  // 4
  {"#####","#....","####.","....#","....#","#...#",".###."},  // 5
  {"..##.",".#...","#....","####.","#...#","#...#",".###."},  // 6
  {"#####","....#","...#.","..#..",".#...",".#...",".#..."},  // 7
  {".###.","#...#","#...#",".###.","#...#","#...#",".###."},  // 8
  {".###.","#...#","#...#",".####","....#","...#.",".##.."},  // 9
  {".....",".....",".....","#####",".....",".....","....."},  // 10 '-'
  {".....","..#..","..#..","#####","..#..","..#..","....."},  // 11 '+'
  {".....",".....",".....",".....",".....",".##..",".##.."},  // 12 '.'
  {"##..#","##..#","...#.","..#..",".#...","#..##","#..##"},  // 13 '%'
};

static int pixChar5(int ox, int oy, int idx, int cell, uint16_t col) {
  if (idx == 12) {  // '.'：2 列实点，占位 3 列
    lcd.fillRect(ox, oy + 5 * cell, 2 * cell, 2 * cell, col);
    return 3 * cell;
  }
  for (int r = 0; r < 7; r++) {
    const char* row = PX_D5[idx][r];
    for (int c = 0; c < 5; c++)
      if (row[c] == '#') lcd.fillRect(ox + c * cell, oy + r * cell, cell, cell, col);
  }
  return 6 * cell;  // 5 列 + 1 列间隙
}

static int pixCharIdx(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch == '-') return 10;
  if (ch == '+') return 11;
  if (ch == '.') return 12;
  if (ch == '%') return 13;
  return -1;
}

int pixNum5x7Width(const char* s, int cell) {
  int w = 0;
  for (; *s; s++) {
    int i = pixCharIdx(*s);
    if (i < 0) continue;
    w += (i == 12 ? 3 : 6) * cell;
  }
  return w;
}

int pixNum5x7(int ox, int oy, const char* s, int cell, uint16_t col) {
  for (; *s; s++) {
    int i = pixCharIdx(*s);
    if (i < 0) continue;
    ox += pixChar5(ox, oy, i, cell, col);
  }
  return ox;
}

// ---------------------------------------------------------------------------
// 超长文字防御性截断（中文按字符截断，尾部补省略号）
// ---------------------------------------------------------------------------
int drawTextClamped(const char* s, int x, int y, int maxW) {
  String t(s);
  int fullW = lcd.textWidth(t);
  if (fullW <= maxW) {                 // 完整放得下：整串绘制，绝不截断
    lcd.drawString(t, x, y);
    return fullW;
  }
  // 超宽：从头找"最长字符前缀 + …"能放进 maxW 的位置
  int n = t.length();
  int end = n;
  while (end > 0) {
    int step = 1;
    while (end - step > 0 && (s[end - step] & 0xC0) == 0x80) step++;  // UTF-8 按字符回退
    end -= step;
    if (lcd.textWidth(t.substring(0, end)) + lcd.textWidth("…") <= maxW) break;
  }
  String out = t.substring(0, end) + "…";
  lcd.drawString(out, x, y);
  return lcd.textWidth(out);
}

}  // namespace thm
