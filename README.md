# WeatherClock 天气时钟

基于 **ESP32-C3 Super Mini + 1.54″ ST7789 (240×240)** 的桌面天气时钟，参考 [BambuHelper](https://github.com/Keralots/BambuHelper) 的架构实现：PlatformIO + Arduino + LovyanGFX + ArduinoJson。

**功能**
- 时间 + 日期 + 星期（NTP 同步，时区网页可选，支持夏令时）
- 当前天气：温度、体感、湿度、程序化绘制的天气图标（数据来自 [和风天气 QWeather](https://dev.qweather.com)，免费版）
- 行情显示：金价 / 沪铜 / 布伦特原油 / 自定义 JSON 数据源（红涨绿跌，10 分钟刷新）
- **5 套显示主题**，网页点击即切换，即时生效、断电记忆（NVS 持久化）：

  | 主题 | 风格 |
  |---|---|
  | Modern | 现代极简，黑底暖白七段 |
  | Retro | Pip-Boy 磷光绿辐射终端（单色 CRT） |
  | Platformer | 经典横版游戏 HUD，蓝天像素场景 |
  | Wasteland | 琥珀 CRT 废土工业避难所面板 |
  | Mario | 马里奥特征色平台游戏：红顶栏 / Coin 黄温度 / 砖墙行情条 |

- 网页配置：左右双栏布局——左侧 240×240 实时预览（切主题即时换肤，与屏幕 1:1 设计语言），右侧全部设置项；支持「测试 API」用真实 Key 拉一次数据渲染预览
- **固件在线更新（OTA）**：配置页一键检查 GitHub 最新版本并无线刷机，全程无需数据线；也可手动上传 bin。双 OTA 分区 + 固件校验，坏包自动拒绝不会变砖
- 断线自动重连、天气定时刷新（默认 15 分钟，可改）

## 快速开始：浏览器一键刷固件（无需安装 PlatformIO）

本仓库已配置 **GitHub Actions**：每次推送代码会自动编译 ESP32-C3 固件并发布到 GitHub Pages，因此你可以**只靠浏览器**刷入固件：

> 🌐 **安装页**：`https://Escaper929.github.io/WeatherClock/`

用法：
1. 用 USB 线把 **ESP32-C3 Super Mini** 连到电脑（按 Ctrl 键用 Chrome/Edge/Firefox 打开安装页，**不支持 Safari/iOS**）。
2. 点击“连接并烧录”→ 选择设备串口 → 等待烧录完成。
3. 设备自动重启并进入配置门户（连 WiFi `WeatherClock-xx` → 打开 `192.168.4.1` → 填 WiFi / API Key / 城市）。

> 首次部署：仓库 **Settings → Pages → Source 选 “GitHub Actions”**，并等一次 Actions 构建通过后，安装页才生效。

> **只有首次（或救砖）需要电脑**：设备跑起来之后，固件升级直接在配置页点「检查更新 → 下载并刷机」即可，从 GitHub（jsDelivr 镜像，国内可达）无线拉取最新固件。

## 接线

1.54″ ST7789 7 引脚模块 -> ESP32-C3 Super Mini（按丝印引脚）：

| 屏模块 | ESP32-C3 | 板子丝印 |
|---|---|---|
| VCC | 3.3V | Pin 3 |
| GND | GND | Pin 2 |
| SCL (SCLK) | GPIO21 | Pin 16 |
| SDA (MOSI) | GPIO20 | Pin 15 |
| CS | GPIO6 | Pin 10 |
| DC | GPIO7 | Pin 11 |
| RST (RES) | GPIO10 | Pin 14 |
| BL (背光) | GPIO5 | Pin 9 |

> 注意：GPIO8 是板载 LED、GPIO9 是 BOOT 按键，都不要接屏。接线定义集中在 [include/BoardPins.h](include/BoardPins.h)，如需调整改那一处即可。

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
2. 手机/电脑连接 WiFi `WeatherClock-xx`，浏览器打开 `http://192.168.4.1`。
3. 填写 WiFi、API Key、城市名（如 `北京`，或直接填经纬度），点“保存并重启”。
4. 设备连接 WiFi、同步时间、拉取天气，进入主界面。
5. **以后想改配置**：直接用浏览器访问 `http://<设备IP>`（开机时屏幕会显示 IP，或输入 `http://weatherclock.local`）；按住 BOOT 键上电也可进入软 AP 门户。
6. **以后想升级固件**：配置页左栏「固件更新」卡片 → 「检查更新」（显示当前版本与 GitHub 最新版本）→ 有新版时点「下载并刷机」，设备自动拉取写入并重启，无需连接电脑。

## 目录结构

```
tianqishizhong/
├── platformio.ini      # 构建配置（esp32c3）
├── partitions_4mb.csv  # 4MB 分区表（支持 OTA）
├── include/
│   ├── BoardPins.h     # 引脚与常量
│   ├── AppConfig.h     # 配置存储（NVS）
│   ├── WiFiTime.h      # WiFi + NTP + 时区
│   ├── Weather.h       # 和风天气客户端
│   ├── Quote.h         # 行情数据源（金价/沪铜/原油/自定义 JSON）
│   ├── Theme.h         # 多主题接口与 UiData 数据视图
│   ├── UiTheme.h       # 基础色板工具
│   └── Portal.h        # 网页配置门户
└── src/
    ├── main.cpp        # 主循环：数据层（时间/天气/行情）
    ├── Screen.cpp      # ST7789 屏幕驱动
    ├── Theme.cpp       # 主题分发与激活
    ├── Portal.cpp      # 网页配置（双栏布局 + 主题换肤预览）
    └── themes/         # 5 套主题实现 + ThemeShared 共享绘制原语
        ├── theme_modern.cpp / theme_retro.cpp / theme_pixel.cpp
        ├── theme_wasteland.cpp / theme_mario.cpp
        └── ThemeShared.h/.cpp
```

## 自定义

- 刷新间隔、亮度：改 `include/BoardPins.h`
- 时区：配置页下拉选择（POSIX TZ 串，支持夏令时），默认 UTC+8
- 天气换成经纬度定位：配置门户里填“经度,纬度”即可
- 新增主题：在 `src/themes/` 仿照现有主题新建 `theme_xxx.cpp` 实现 `xxxTick(const UiData&, bool)`，然后在 `include/Theme.h` 枚举加 id 并更新 `THEME_COUNT`、在 `src/Theme.cpp` 注册分发并补 `themeName()/themeDesc()`；各主题布局与配色完全独立（脏检测模式见 `Theme.h` 头注释），网页预览皮肤在 `Portal.cpp` 的 `applyTheme()`/CSS 加一个 class 即可