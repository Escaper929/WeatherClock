// ============================================================================
//  Screen.cpp - ST7789 240x240 驱动与基础服务
//
//  职责收窄（多主题重构后）：
//    - LCD 硬件初始化 / 背光 PWM
//    - renderStatus() 开机/等待/错误状态页
//  主界面全部绘制已迁移至 src/themes/（每套主题独立布局），
//  由 Theme.h 的 themeTick() 分发。共享原语见 src/themes/ThemeShared.*。
// ============================================================================
#include "Screen.h"
#include "BoardPins.h"
#include "UiTheme.h"
#include "themes/ThemeShared.h"

using namespace ui;

// ---------------------------------------------------------------------------
// 背光
// ---------------------------------------------------------------------------
static void backlightSetup() {
#if (defined(PIN_BL) && PIN_BL >= 0)
  ledcSetup(0, 12000, 8);
  ledcAttachPin(PIN_BL, 0);
  ledcWrite(0, SCREEN_BRIGHTNESS);
#endif
}

bool screenInit() {
  lcd.init();
  lcd.display();
  backlightSetup();
  lcd.setRotation(0);
  lcd.fillScreen(BG);
  return true;
}

void screenSetBrightness(uint8_t v) {
#if (defined(PIN_BL) && PIN_BL >= 0)
  ledcWrite(0, v);
#endif
}

// ---------------------------------------------------------------------------
// 状态页：开机 / 等待 / 错误。内部 fillScreen，单独使用。
// 重置主题 epoch，使下一次 themeTick 全屏重绘主界面。
// ---------------------------------------------------------------------------
void renderStatus(const char* msg, const String& sub) {
  lcd.fillScreen(ui::BG);
  lcd.setFont(&fonts::efontCN_12);
  lcd.setTextSize(1);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(ui::INK, ui::BG);
  lcd.drawString(msg, 120, 112);
  if (sub.length()) {
    lcd.setTextColor(ui::INK3, ui::BG);
    lcd.drawString(sub.c_str(), 120, 140);
  }
  themeInvalidate();
}
