// ============================================================================
//  Screen.h - ST7789 240x240 屏幕驱动与基础服务
//  主界面绘制已迁移至多主题系统（include/Theme.h + src/themes/），
//  本头文件只保留硬件层服务。
// ============================================================================
#pragma once
#include <Arduino.h>

//初始化屏幕与背光，显示开机画面，返回是否成功
bool screenInit();

// 设置背光亮度 0-255
void screenSetBrightness(uint8_t v);

// 在中间显示临时状态文字（如“连接中…”“无 WiFi”）
// 内部 fillScreen 并递增主题 epoch（保证回到主界面时全屏重绘）
void renderStatus(const char* msg, const String& sub = String());
