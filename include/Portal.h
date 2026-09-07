// ============================================================================
//  Portal.h - 网页配置（softAP 首次配置 + STA 常驻配置页）
//  两种方式访问配置页：
//   1. 首次烧录 / 按住 BOOT 上电：softAP 门户，访问 http://192.168.4.1
//   2. 连接 WiFi 后：常驻配置页，访问 http://<设备IP> 或 http://weatherclock.local
// ============================================================================
#pragma once
#include <Arduino.h>
#include "AppConfig.h"

// 交互式配置门户（softAP，阻塞运行），直到用户保存配置（随后自动重启）。
// 仅用于首次配置或按住 BOOT 强制进入。
void portalEnter(const AppConfig& current);

// 启动常驻配置服务器（STA 模式，非阻塞）。WiFi 连接成功后调用。
// 之后可通过 http://<IP> 或 http://weatherclock.local 访问配置页。
void portalServerBegin(const AppConfig& current);

// 在 loop() 中轮询配置服务器。
void portalServerLoop();
