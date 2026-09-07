# WeatherClock 天气时钟

基于 **ESP32-C3 Super Mini + 1.54″ ST7789 (240×240)** 的桌面天气时钟，参考 [BambuHelper](https://github.com/Keralots/BambuHelper) 的架构实现：PlatformIO + Arduino + LovyanGFX + ArduinoJson。

**功能**
- 时间 + 日期 + 星期（NTP 同步，北京时间 UTC+8）
- 当前天气：温度、天气图标、英文天气名、湿度（数据来自 [和风天气 QWeather](https://dev.qweather.com)，免费版）
- 网页配置门户：启动时未配置或按住 **BOOT 键** 上电，即可进入软 AP `WeatherClock-xx`，浏览器访问 `http://192.168.4.1` 填写 WiFi / API Key / 城市后保存重启
- 断线自动重连、天气定时刷新（默认 15 分钟，可改）

## 接线

1.54″ ST7789 7 引脚模块 -> ESP32-C3 Super Mini：

| 屏模块 | ESP32-C3 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL (SCLK) | GPIO4 |
| SDA (MOSI) | GPIO6 |
| CS | GPIO10 |
| DC | GPIO7 |
| RES | GPIO8 |
| BL (背光) | GPIO5 |

> 引脚在 [include/BoardPins.h](include/BoardPins.h) 里统一定义，按实际接线改即可。若用 GPIO8 上电不启动，把 RES 接到其它空闲引脚。

## 准备工作

1. 安装 [PlatformIO](https://platformio.org/)（或 VS Code + PlatformIO 插件）。
2. 注册 [和风天气 QWeather](https://dev.qweather.com)，创建一个**免费版（开发）应用**，拿到 API Key。

## 构建与烧录

```bash
# 构建
pio run

# 烧录
pio run -t upload

# 查看串口日志（C3 原生 USB CDC）
pio device monitor -b 115200
```

## 使用流程

1. 首次上电（未配置）会自动进入配置门户。
   以后想改配置：**按住 BOOT 键再上电**。
2. 手机/电脑连接 WiFi `WeatherClock-xx`，浏览器打开 `http://192.168.4.1`。
3. 填写 WiFi、API Key、城市名（如 `北京`，或直接填经纬度），点“保存并重启”。
4. 设备连接 WiFi、同步时间、拉取天气，进入主界面。

## 目录结构

```
tianqishizhong/
├── platformio.ini      # 构建配置（esp32c3）
├── partitions_4mb.csv  # 4MB 分区表（支持 OTA）
├── include/
│   ├── BoardPins.h     # 引脚与常量
│   ├── AppConfig.h     # 配置存储
│   ├── WiFiTime.h      # WiFi + NTP
│   ├── Weather.h       # 和风天气客户端
│   ├── Screen.h        # 屏幕渲染
│   └── Portal.h        # 网页配置门户
└── src/                # 对应实现 + main.cpp
```

## 自定义

- 刷新间隔、亮度、时区：改 `include/BoardPins.h`
- 天气换成经纬度定位：配置门户里填“经度,纬度”即可