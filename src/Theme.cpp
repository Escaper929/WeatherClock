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
void marioTick (const UiData& d, bool blinkColon);

// 各主题的行情上滑动画帧
void modernQuoteAnim(const UiData& d);
void retroQuoteAnim (const UiData& d);
void pixelQuoteAnim (const UiData& d);
void wastelandQuoteAnim(const UiData& d);
void marioQuoteAnim (const UiData& d);

// 各主题行情带动画资源释放
void modernQuoteRelease(void);
void retroQuoteRelease (void);
void pixelQuoteRelease (void);
void wastelandQuoteRelease(void);
void marioQuoteRelease (void);

// ---- 行情上滑过渡状态机（双向滚动：旧上滑滑出、新从下方滑入） ----
static bool            sQAnim   = false;     // 一次上滑进行中
static uint32_t        sQAnimT0 = 0;
static QuoteData       sPrevQ;               // 上一条正在显示的行情（作上滑滑出的内容）
static bool            sPrevValid = false;
static const uint32_t QANIM_MS = 450;    // 过渡时长

const char* themeName(DisplayTheme t) {
  switch (t) {
    case THEME_RETRO:     return "Retro";
    case THEME_PIXEL:     return "Platformer";
    case THEME_WASTELAND: return "Wasteland";
    case THEME_MARIO:     return "Mario";
    default:              return "Modern";
  }
}

const char* themeDesc(DisplayTheme t) {
  switch (t) {
    case THEME_RETRO:     return "辐射终端，Pip-Boy 磷光绿废土风";
    case THEME_PIXEL:     return "经典平台游戏 HUD，蓝天像素冒险场景";
    case THEME_WASTELAND: return "废土终端，琥珀 CRT 工业避难所面板";
    case THEME_MARIO:     return "马里奥配色平台游戏，高饱和经典特征色";
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
    case THEME_MARIO:     marioTick(d, blinkColon);     break;
    default:              modernTick(d, blinkColon);    break;
  }
}

void themeQuoteAnimStart(const QuoteData& prev) {
  sQAnim     = true;
  sQAnimT0   = millis();
  sPrevQ     = prev;
  sPrevValid = true;
}

const QuoteData* themeQuotePrev() {
  return (sQAnim && sPrevValid) ? &sPrevQ : nullptr;
}

bool themeQuoteAnimActive() { return sQAnim; }

float themeQuoteAnimProgress() {
  if (!sQAnim) return 0.f;
  uint32_t e = millis() - sQAnimT0;
  if (e >= QANIM_MS) { sQAnim = false; return 0.f; }
  // 余弦 ease-in-out：1 → 0，两端速度为零、中段匀速，避免旧 ease-out 首帧即跳 85% 造成的"闪烁"感。
  // 整体行程=QANIM_K*0.86（余弦最小到离 0 还有半像素），配合 ~450ms 周期，形成持续上移的滚动观感。
  float t = (float)e / (float)QANIM_MS;
  return 0.5f + 0.5f * cosf(t * M_PI);
}

void themeQuoteAnimTick(const UiData& d) {
  if (!sQAnim) return;
  switch (sTheme) {
    case THEME_RETRO:     retroQuoteAnim(d);     break;
    case THEME_PIXEL:     pixelQuoteAnim(d);     break;
    case THEME_WASTELAND: wastelandQuoteAnim(d); break;
    case THEME_MARIO:     marioQuoteAnim(d);     break;
    default:              modernQuoteAnim(d);    break;
  }
}

// 动画结束后由 main 调用：释放各主题的离屏行情带 Sprite，避免内存滞留
void themeQuoteAnimRelease() {
  switch (sTheme) {
    case THEME_RETRO:     retroQuoteRelease();     break;
    case THEME_PIXEL:     pixelQuoteRelease();     break;
    case THEME_WASTELAND: wastelandQuoteRelease(); break;
    case THEME_MARIO:     marioQuoteRelease();     break;
    default:              modernQuoteRelease();    break;
  }
}
