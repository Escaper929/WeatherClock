// ============================================================================
//  Theme.cpp - 主题分发与激活管理
// ============================================================================
#include "Theme.h"

static DisplayTheme sTheme = THEME_MODERN;
static uint32_t     sEpoch = 1;    // 从 1 开始：0 作为"从未绘制"哨兵

// 各主题实现（src/themes/*.cpp）
void modernTick(const UiData& d, bool blinkColon);
void retroTick (const UiData& d, bool blinkColon);
void pixelTick (const UiData& d, bool blinkColon);
void wastelandTick(const UiData& d, bool blinkColon);

const char* themeName(DisplayTheme t) {
  switch (t) {
    case THEME_RETRO:     return "Retro";
    case THEME_PIXEL:     return "Pixel";
    case THEME_WASTELAND: return "Wasteland";
    default:              return "Modern";
  }
}

const char* themeDesc(DisplayTheme t) {
  switch (t) {
    case THEME_RETRO:     return "辐射终端，Pip-Boy 磷光绿废土风";
    case THEME_PIXEL:     return "像素艺术，复古游戏机风格";
    case THEME_WASTELAND: return "废土终端，琥珀 CRT 工业避难所面板";
    default:              return "现代极简，黑底暖白，适合日常使用";
  }
}

void themeApply(DisplayTheme t) {
  if ((uint8_t)t >= THEME_COUNT) t = THEME_MODERN;   // 非法 id 回退默认
  if (t == sTheme) return;                            // 同主题不折腾
  sTheme = t;
  sEpoch++;                                           // 强制各主题全屏重绘
}

DisplayTheme themeCurrent() { return sTheme; }

uint32_t themeEpoch() { return sEpoch; }

void themeInvalidate() { sEpoch++; }

void themeTick(const UiData& d, bool blinkColon) {
  switch (sTheme) {
    case THEME_RETRO:     retroTick(d, blinkColon);     break;
    case THEME_PIXEL:     pixelTick(d, blinkColon);     break;
    case THEME_WASTELAND: wastelandTick(d, blinkColon); break;
    default:              modernTick(d, blinkColon);    break;
  }
}
