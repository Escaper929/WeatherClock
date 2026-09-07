// ============================================================================
//  BoardPins.h - ESP32-C3 Super Mini + ST7789 1.54" 240x240 引脚定义
//
//  1.54" ST7789 屏多为 7 引脚模块：
//     VCC GND CS DC RES SCL SDA  + 背光 BL（本工程 GPIO5）
//
//  本工程使用 C3 硬件 SPI FSPI（SPI2）与默认字体，不依赖触摸。
//  下面按 ESP32-C3 SuperMini 实际丝印的引脚配置，接线请严格对应：
// ----------------------------------------------------------------------------
//  屏模块引脚	   ->  ESP32-C3 引脚
//  -----------------------------------------------------------------------
//  SCL (SCLK)      ->  GPIO21   (板子 Pin 16)
//  SDA (MOSI)      ->  GPIO20   (板子 Pin 15)
//  CS  (片选)       ->  GPIO6    (板子 Pin 10)
//  DC  (数据/命令)  ->  GPIO7    (板子 Pin 11)
//  RES (复位)       ->  GPIO10   (板子 Pin 14)
//  BL  (背光)       ->  GPIO5    (板子 Pin 9)  —— 可用 PWM 调亮度
//  注意：GPIO8 是板载 LED、GPIO9 是 BOOT 按键，都不要接屏。
// ============================================================================
#pragma once

#ifndef PIN_SCK
#define PIN_SCK  21
#endif
#ifndef PIN_MOSI
#define PIN_MOSI 20
#endif
#ifndef PIN_CS
#define PIN_CS   6
#endif
#ifndef PIN_DC
#define PIN_DC   7
#endif
#ifndef PIN_RST
#define PIN_RST  10
#endif
#ifndef PIN_BL
#define PIN_BL   5
#endif

// 强制进入配置门户（长按）所使用的引脚：C3 Super Mini 的 BOOT 按钮是 GPIO9
#ifndef PIN_BOOT_BTN
#define PIN_BOOT_BTN 9
#endif

// 背光亮度 (0-255)
#ifndef SCREEN_BRIGHTNESS
#define SCREEN_BRIGHTNESS 200
#endif

// 天气刷新间隔（秒）——QWeather 免费版 devapi 每日 1000 次，15 分钟足够
#ifndef WEATHER_FETCH_INTERVAL_SEC
#define WEATHER_FETCH_INTERVAL_SEC (15 * 60)
#endif

// NTP 时区：北京时间 UTC+8，无夏令时
#ifndef TZ_GMTOFFSET_SEC
#define TZ_GMTOFFSET_SEC (8 * 3600)
#endif
#ifndef TZ_DSTOFFSET_SEC
#define TZ_DSTOFFSET_SEC 0
#endif