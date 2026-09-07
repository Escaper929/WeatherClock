// ============================================================================
//  BoardPins.h - ESP32-C3 Super Mini + ST7789 1.54" 240x240 引脚定义
//
//  1.54" ST7789 屏多为 7 引脚模块：
//     VCC GND CS(RS) DC(RES?) SCL(RCLK) SDA(MOSI?) BL
//
//  本工程使用 C3 硬件 SPI FSPI（SPI2）与默认字体，不依赖触摸。
//  下列为常用接法，请务必按你的实际接线修改！改动后重新编译烧录即可。
// ----------------------------------------------------------------------------
//  屏模块引脚	   ->  ESP32-C3 引脚
//  -----------------------------------------------------------------------
//  SCL (SCLK)      ->  GPIO4   (FSPI SCK)
//  SDA (MOSI)      ->  GPIO6   (FSPI MOSI)
//  CS  (片选)       ->  GPIO10
//  DC  (数据/命令)  ->  GPIO7
//  RES (复位)       ->  GPIO8   (C3 的 GPIO8 是 BOOT 强拉脚，仅在复位瞬间采样，
//                               正常运行时作为普通 IO 使用；若上电不启动，
//                               把 RES 换接到其它引脚即可，或用 RST_PIN=-1)
//  BL  (背光)       ->  GPIO5   (可用 PWM 调亮度；若模块背光常亮可接到 3.3V)
//  VCC/GND          ->  3.3V/GND
// ============================================================================
#pragma once

#ifndef PIN_SCK
#define PIN_SCK  4
#endif
#ifndef PIN_MOSI
#define PIN_MOSI 6
#endif
#ifndef PIN_CS
#define PIN_CS   10
#endif
#ifndef PIN_DC
#define PIN_DC   7
#endif
#ifndef PIN_RST
#define PIN_RST  8
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