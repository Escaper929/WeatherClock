// ============================================================================
//  Portal.cpp - 网页配置实现
//  两种模式共用同一套页面与 /save 处理：
//   - portalEnter():        softAP 门户（首次/按住 BOOT），访问 http://192.168.4.1
//   - portalServerBegin():  STA 常驻配置页，访问 http://<设备IP> / weatherclock.local
//  页面功能：屏幕预览 + 时区选择 + 真实 API 数据测试（/api/preview）
// ============================================================================
#include "Portal.h"
#include "BoardPins.h"
#include "Weather.h"
#include "Quote.h"
#include "Theme.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "FwVersion.h"   // 由 build_genversion.py / CI 生成，提供 FW_VERSION 宏

// 固件版本串：CI 构建时注入 src/FwVersion.h（短 SHA + UTC 日期，与 web/version.txt 同源），
// 本地构建无该头文件时回退为编译时间戳。
#ifdef FW_VERSION
#define FWV_STR FW_VERSION
#else
#define FWV_STR (__DATE__ " " __TIME__)
#endif

// 常驻配置服务器（STA 模式）
static WebServer* gServer = nullptr;
static AppConfig  gCfg;

// ---- 时区选项：POSIX 时区串（esp32 configTzTime 语法，支持夏令时） ----
static const char* TZ_OPTIONS[] = {
  "CST-8|(UTC+8) 北京 / 上海 / 香港 / 台北 / 新加坡",
  "JST-9|(UTC+9) 东京 / 首尔",
  "ICT-7|(UTC+7) 曼谷 / 河内 / 雅加达",
  "IST-5:30|(UTC+5:30) 印度",
  "AST-3|(UTC+3) 莫斯科 / 伊斯坦布尔",
  "CET-1CEST,M3.5.0,M10.5.0/3|(UTC+1) 柏林 / 巴黎 / 罗马",
  "GMT0BST,M3.5.0/1,M10.5.0|(UTC+0) 伦敦 / 里斯本",
  "EST5EDT,M3.2.0,M11.1.0|(UTC-5) 纽约 / 多伦多",
  "CST6CDT,M3.2.0,M11.1.0|(UTC-6) 芝加哥 / 墨西哥城",
  "MST7MDT,M3.2.0,M11.1.0|(UTC-7) 丹佛",
  "PST8PDT,M3.2.0,M11.1.0|(UTC-8) 洛杉矶 / 温哥华",
  "ACST-9:30ACDT,M10.1.0,M4.1.0/3|(UTC+9:30) 阿德莱德",
  "NZST-12NZDT,M9.5.0,M4.1.0/3|(UTC+12) 奥克兰",
};
static const int TZ_OPT_COUNT = sizeof(TZ_OPTIONS) / sizeof(TZ_OPTIONS[0]);

static String renderTzOptions(const String& current) {
  String s = String("<option value=''>跟随默认 (UTC+8)</option>");
  for (int i = 0; i < TZ_OPT_COUNT; i++) {
    String line = FPSTR(TZ_OPTIONS[i]);
    int bar = line.indexOf('|');
    String val = line.substring(0, bar), label = line.substring(bar + 1);
    s += "<option value='" + val + "'" + (val == current ? " selected" : "") + ">" + label + "</option>";
  }
  if (current.length() > 0 && TZ_OPT_COUNT) {
    bool found = false;
    for (int i = 0; i < TZ_OPT_COUNT; i++) {
      String line = FPSTR(TZ_OPTIONS[i]);
      if (line.indexOf('|') > 0 && line.substring(0, line.indexOf('|')) == current) { found = true; break; }
    }
    if (!found) s += "<option value='" + current + "' selected>" + current + "</option>";
  }
  return s;
}

// JSON 字符串转义（最小集）
static String jsonEsc(const String& s) {
  String r;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') r += '\\';
    r += c;
  }
  return r;
}

// 行情轮播条目渲染（每条：类型下拉 + 自定义项；供网页列表 builder 初始化用）
static String renderQSlotRow(const QuoteSlot& s) {
  const char* label[] = {"不显示","金价（Au99.99 · 元/克）","布伦特原油（美元/桶）",
                         "自定义 JSON 数据源","沪铜（上期所 · 元/吨）"};
  static const int types[] = {0,1,2,3,4};
  const char* sel  = "";
  String opts;
  for (int i = 0; i < 5; i++) {
    bool on = (types[i] == s.type);
    if (on) sel = " selected";
    opts += "<option value='" + String(types[i]) + "'" + (on ? " selected" : "") +
            ">" + label[i] + "</option>";
  }
  String row = "<div class='qrow'><div class='qhead'><select onchange='qType(this)'>" +
               opts + "</select><button type='button' class='qdel' onclick='delQRow(this)'>移除</button></div>";
  bool cus = (s.type == 3);
  row += "<div class='qcus' style='display:" + String(cus ? "block" : "none") + "'>";
  String u = jsonEsc(s.url), p = jsonEsc(s.path), l = jsonEsc(s.label);
  row += "<label>数据源 URL（https://…，返回 JSON）</label>";
  row += String("<input placeholder='https://example.com/api/quote' value='") + u +
         "' oninput='saveQList()'>" + "<label>价格字段 JSON 路径（点分级，如 data.f43）</label>";
  row += String("<input placeholder='data.f43' value='") + p + "' oninput='saveQList()'>" +
         "<label>显示标签（如 金价 / 汇率 / 股价）</label>";
  row += String("<input placeholder='金价' value='") + l + "' oninput='saveQList()'>";
  row += "</div></div>";
  return row;
}

static String renderQSlots(const AppConfig& cfg) {
  String s;
  for (int i = 0; i < cfg.quote_slot_count && i < QUOTE_SLOTS; i++)
    if (cfg.quote_slots[i].type > 0) s += renderQSlotRow(cfg.quote_slots[i]);
  return s;
}

// 显示风格选项（5 主题卡片，当前项带选中态；点击即经 /api/theme 生效）
static String renderThemeOptions(uint8_t cur) {
  String s;
  for (int i = 0; i < THEME_COUNT; i++) {
    DisplayTheme t = (DisplayTheme)i;
    s += "<div class='th" + String(i == cur ? " sel" : "") + "' data-t='" + String(i) +
         "' data-n='" + themeName(t) + "' onclick='pickTheme(this)'>"
         "<b>" + themeName(t) + "</b><span class='d'>" + themeDesc(t) + "</span></div>";
  }
  return s;
}

// 生成 web 页面（占位符在 renderPage 中替换）
static const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang='zh'><head>
<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>天气时钟配置</title><style>
:root{--bg:#0b1022;--card:#151d38;--in:#0d1428;--line:#2a3a5f;--txt:#e8eefc;--dim:#8fa3c8;--acc:#33dd88}
*{box-sizing:border-box}
body{font-family:system-ui,Arial,sans-serif;background:var(--bg);color:var(--txt);display:flex;justify-content:center;margin:0;padding:16px}
/* 左右双栏：左预览列（sticky 常驻，含固件更新）/ 右设置表单；窄屏回退单列 */
.wrap{max-width:920px;width:100%;display:flex;gap:16px;align-items:flex-start}
.colL{width:302px;flex:none;position:sticky;top:16px}
.colR{flex:1;min-width:0}
@media(max-width:760px){.wrap{flex-direction:column}.colL{width:100%;position:static}}
.card{background:var(--card);border-radius:14px;padding:18px;margin-bottom:16px;border:1px solid var(--line)}
h1{font-size:17px;margin:0 0 2px;color:var(--acc)}small{color:var(--dim)}
label{display:block;margin:12px 0 4px;font-size:12px;color:var(--dim)}
input,select{width:100%;padding:9px;border-radius:7px;border:1px solid var(--line);background:var(--in);color:var(--txt);font-size:14px}
.btn{width:100%;margin-top:14px;padding:11px;border:0;border-radius:8px;background:var(--acc);color:#032;font-size:15px;font-weight:700;cursor:pointer}
.btn2{background:#1d6fff;color:#fff}
.row{display:flex;gap:8px}.row>*{flex:1}
/* 240x240 屏幕预览：与固件 UiTheme/Screen.cpp 同一套设计语言（1px=1px） */
#scr{width:240px;height:240px;background:#0f1116;border-radius:10px;border:1px solid #2a2e36;margin:14px auto 0;position:relative;overflow:hidden}
#scr .pdate{position:absolute;top:9px;left:24px;color:#a2a49c;font-size:11px;letter-spacing:1px;white-space:nowrap}
#scr .pcity{position:absolute;top:9px;right:24px;color:#a2a49c;font-size:11px;white-space:nowrap}
#scr .pclock{position:absolute;top:35px;left:0;width:100%;text-align:center;color:#eae4d8;font-size:42px;font-weight:700;font-family:Consolas,Menlo,monospace;letter-spacing:4px;font-variant-numeric:tabular-nums}
#scr .pfrule{position:absolute;top:208px;left:24px;right:24px;height:1px;background:#2c2f36}
#scr .picon{position:absolute;left:36px;top:118px;width:40px;height:40px;line-height:40px;text-align:center;font-size:26px;opacity:.92}
#scr .pwtext{position:absolute;left:24px;top:165px;width:64px;text-align:center;color:#a2a49c;font-size:11px}
#scr .ptemp{position:absolute;right:21px;top:122px;color:#eae4d8;font-size:23px;font-weight:700;font-family:Consolas,Menlo,monospace;white-space:nowrap}
#scr .ptemp .pdeg{font-size:10px;font-weight:400;color:#a2a49c;margin-left:2px}
#scr .pmeta{position:absolute;right:24px;top:165px;color:#6e7076;font-size:11px;white-space:nowrap;text-align:right}
#scr .pqrow{position:absolute;top:218px;left:24px;right:24px;display:flex;justify-content:space-between;align-items:center;font-size:11px;color:#6e7076;white-space:nowrap}
#scr .pqrow .qp{color:#969da8;font-weight:700;margin:0 4px}
#scr .pqrow .up{color:#c68670;font-weight:700}
#scr .pqrow .dn{color:#749c94;font-weight:700}
/* 行情轮播列表 builder */
.qrow{border:1px solid var(--line);border-radius:8px;padding:8px;margin-top:8px;background:var(--in)}
.qhead{display:flex;gap:6px;align-items:center}
.qhead select{flex:1}
.qdel{width:auto;flex:none;padding:7px 11px;border:0;border-radius:7px;background:#a74632;color:#fff;cursor:pointer;font-size:12px}
.qcus{margin-top:6px}
.qcus input{margin-top:6px}
.qadd{margin-top:8px;background:#3a6}
#res{display:none;margin-top:10px;font-size:12px;line-height:1.6;padding:10px;border-radius:8px;background:var(--in);border:1px solid var(--line);white-space:pre-wrap;color:var(--txt)}
#res.ok{border-color:#3a6}
.tips{font-size:11px;color:var(--dim);margin-top:12px;line-height:1.6}
details.hint{margin-top:10px;border:1px solid var(--line);border-radius:8px;background:var(--in);padding:6px 10px;font-size:12px}
details.hint summary{cursor:pointer;color:var(--acc);font-weight:600;user-select:none}
details.hint .hintb{margin-top:8px;color:var(--dim);line-height:1.7;font-size:12px}
details.hint .hintb b{color:var(--txt)}
/* 显示风格主题卡片 */
.th{display:flex;align-items:center;gap:10px;padding:9px 11px;margin-top:6px;border:1px solid var(--line);border-radius:8px;background:var(--in);cursor:pointer}
.th b{font-size:13px;min-width:58px}
.th .d{font-size:11px;color:var(--dim);flex:1}
.th.sel{border-color:var(--acc);background:#122a20}
.th.sel b{color:var(--acc)}
#thres{font-size:11px;color:var(--acc);margin-top:6px;min-height:14px}
/* Classic Platformer 预览皮肤（t=2 时启用，与固件 theme_pixel.cpp 同一设计语言） */
#scr.plat{background:#5c94fc;border-radius:0}
#scr .phud{display:none}
#scr.plat .phud{display:block;position:absolute;top:0;left:0;width:100%;height:28px;background:#12224a}
.plat .pdate{color:#fff;top:8px;left:12px;font-weight:700}
.plat .pcity{color:#fff;top:8px;right:12px;font-weight:700}
.plat .pclock{top:32px;color:#fff;font-size:36px;letter-spacing:2px;text-shadow:2px 0 0 #12224a,-2px 0 0 #12224a,0 2px 0 #12224a,0 -2px 0 #12224a,2px 2px 0 #12224a,-2px -2px 0 #12224a,2px -2px 0 #12224a,-2px 2px 0 #12224a}
#scr .pcard{display:none}
#scr.plat .pcard{display:block;position:absolute;left:20px;top:96px;width:194px;height:76px;border:3px solid #fff;background:#12224a}
.plat .picon{left:28px;top:102px;color:#ffd84c}
.plat .pwtext{left:86px;top:104px;width:130px;text-align:left;color:#ffd84c;font-weight:700}
.plat .ptemp{top:96px;right:32px;color:#fff;font-size:24px;text-shadow:2px 0 0 #12224a,-2px 0 0 #12224a,0 2px 0 #12224a,0 -2px 0 #12224a}
.plat .ptemp .pdeg{color:#9cc0f8}
.plat .pmeta{left:86px;top:128px;right:auto;text-align:left;color:#9cc0f8}
#scr .pground{display:none}
#scr.plat .pground{display:block;position:absolute;bottom:0;left:0;width:100%;height:38px;background:#96663e}
#scr .pgrass{display:none}
#scr.plat .pgrass{display:block;position:absolute;bottom:34px;left:0;width:100%;height:8px;background:#5cb83c}
#scr .pcloud{display:none}
#scr.plat .pcloud{display:block;position:absolute;width:8px;height:8px;background:#fff;box-shadow:8px 0 #fff,16px 0 #fff,24px 0 #fff,8px -8px #fff,16px -8px #fff,4px 8px #b2caf0,20px 8px #b2caf0}
#scr.plat .pcloud.c2{top:50px;right:14px;transform:scale(.8)}
.plat .pqrow{top:210px;left:12px;right:12px;background:#12224a;border:1px solid #fff;padding:3px 6px;color:#fff}
.plat .pqrow .qp{color:#ffd84c}
.plat .pqrow .up{color:#ff7a4c}
.plat .pqrow .dn{color:#4ce06a}
/* Mario 配色皮肤（t=4，plat 布局 + 视觉层级用色） */
#scr.mario .phud{background:#e52521;box-shadow:0 2px 0 #a91418}
#scr.mario .pdate,#scr.mario .pcity{color:#fff7df;text-shadow:1px 0 0 #452313,-1px 0 0 #452313,0 1px 0 #452313,0 -1px 0 #452313}
#scr.mario .pclock{color:#fff7df;text-shadow:2px 0 0 #452313,-2px 0 0 #452313,0 2px 0 #452313,0 -2px 0 #452313,2px 2px 0 #452313,-2px -2px 0 #452313,2px -2px 0 #452313,-2px 2px 0 #452313,4px 4px 0 #a91418}
#scr.mario .pcard{background:#14529a;border-color:#452313;box-shadow:0 0 0 2px #fff7df}
#scr.mario .picon{color:#ffd83d}
#scr.mario .pwtext{color:#fff7df}
#scr.mario .ptemp{color:#ffd83d;text-shadow:2px 0 0 #452313,-2px 0 0 #452313,0 2px 0 #452313,0 -2px 0 #452313}
#scr.mario .ptemp .pdeg{color:#fff7df}
#scr.mario .pmeta{color:#fff7df}
#scr.mario .pground{background:#b85c32}
#scr.mario .pgrass{background:#45c83d}
#scr.mario .pqrow{background:repeating-linear-gradient(#b85c32 0 8px,#71351f 8px 9px);border-color:#fff7df;color:#fff7df}
#scr.mario .pqrow .qp{color:#ffd83d;text-shadow:1px 0 0 #452313,-1px 0 0 #452313,0 1px 0 #452313,0 -1px 0 #452313}
#scr.mario .pqrow .up{color:#e52521;text-shadow:1px 0 0 #452313,-1px 0 0 #452313,0 1px 0 #452313,0 -1px 0 #452313}
#scr.mario .pqrow .dn{color:#45c83d;text-shadow:1px 0 0 #452313,-1px 0 0 #452313,0 1px 0 #452313,0 -1px 0 #452313}
/* Retro 磷光绿皮肤（t=1，单色 CRT） */
#scr.retro{background:#060a06}
#scr.retro .pdate,#scr.retro .pcity{color:#96ffc3}
#scr.retro .pclock{color:#96ffc3;text-shadow:0 0 8px rgba(150,255,195,.45)}
#scr.retro .pcard{background:#0a140c;border-color:#1e6e3e}
#scr.retro .picon,#scr.retro .pwtext,#scr.retro .ptemp{color:#96ffc3}
#scr.retro .ptemp .pdeg{color:#4aa96c}
#scr.retro .pmeta{color:#4aa96c}
#scr.retro .pfrule{background:#1e3a26}
#scr.retro .phud,#scr.retro .pground,#scr.retro .pgrass,#scr.retro .pcloud{display:none}
#scr.retro .pqrow{background:#0a140c;border-color:#1e6e3e;color:#96ffc3}
#scr.retro .pqrow .qp{color:#96ffc3}
#scr.retro .pqrow .up,#scr.retro .pqrow .dn{color:#4aa96c}
/* Wasteland 琥珀 CRT 皮肤（t=3，单色终端） */
#scr.wasteland{background:#0a0906}
#scr.wasteland .pdate,#scr.wasteland .pcity{color:#d6a85a}
#scr.wasteland .pclock{color:#d6a85a;text-shadow:0 0 8px rgba(214,168,90,.4)}
#scr.wasteland .pcard{background:#141008;border-color:#7a5528}
#scr.wasteland .picon,#scr.wasteland .ptemp{color:#d6a85a}
#scr.wasteland .ptemp .pdeg{color:#a9682a}
#scr.wasteland .pwtext{color:#d0c7b2}
#scr.wasteland .pmeta{color:#a9682a}
#scr.wasteland .pfrule{background:#3a2c1a}
#scr.wasteland .phud,#scr.wasteland .pground,#scr.wasteland .pgrass,#scr.wasteland .pcloud{display:none}
#scr.wasteland .pqrow{background:#141008;border-color:#7a5528;color:#d0c7b2}
#scr.wasteland .pqrow .qp{color:#d6a85a}
#scr.wasteland .pqrow .up,#scr.wasteland .pqrow .dn{color:#a9682a}
</style></head><body><div class='wrap'>
<div class='colL'><div class='card'>
<h1>☀ WeatherClock</h1><small>$IP$</small>
<label>屏幕预览（当前浏览器本地时间，保存后设备按所选时区显示）</label>
<div id='scr'>
  <div class='phud'></div>
  <div class='pcloud c1'></div><div class='pcloud c2'></div>
  <div class='pcard'></div>
  <div class='pdate' id='pv-date'>--</div>
  <div class='pcity' id='pv-city'></div>
  <div class='pclock' id='pv-clock'>--:--</div>
  <div class='pfrule'></div>
  <div class='picon' id='pv-icon'>☀</div>
  <div class='pwtext' id='pv-wtext'>晴</div>
  <div class='ptemp'><span id='pv-temp'>--</span><span class='pdeg'>°C</span></div>
  <div class='pmeta' id='pv-meta'>湿度 -- / 体感 --</div>
  <div class='pground'></div><div class='pgrass'></div>
  <div class='pqrow'><span id='pv-q'></span><span id='pv-qpct'></span></div>
</div>
<div id='res'></div>
<div class='tips'>「测试 API」会用当前填写的 Key / Host / 城市真实请求一次和风接口，并把结果渲染到左侧预览，确认无误后再点「保存并重启」。</div>
</div>
<div class='card'>
<label>固件更新 · 当前版本 $FWV$</label>
<div class='row'>
<div><button class='btn' type='button' onclick='otaCheck()'>检查更新</button></div>
<div><button class='btn btn2' type='button' id='otadl' style='display:none' onclick='otaFlash()'>下载并刷机</button></div>
</div>
<div id='ota' style='margin-top:8px;font-size:12px;color:var(--dim)'></div>
<div class='tips'>在线更新自动从 GitHub（jsDelivr 镜像，国内可达）拉取最新固件，写入后自动重启，全程无需数据线；期间保持供电与网络。</div>
<label style='margin-top:10px'>手动刷本地 bin（备用）</label>
<input type='file' id='otafile' accept='.bin'>
<button class='btn' type='button' onclick='otaStart()'>上传所选文件</button>
</div>
</div>
<div class='colR'>
<form id='cf' method='post' action='/save'>
<div class='card'>
<label>WiFi 名称 (SSID)</label>
<input name='s' value='$S$' placeholder='例如 MyWiFi'>
<label>WiFi 密码（留空则保持不变）</label>
<input type='password' name='p' placeholder='WiFi 密码'>
<details class='hint'><summary>设备换地方 / 重新连接新 WiFi 怎么操作？</summary>
<div class='hintb'>
<b>方式一（无需数据线）：</b>断开电源 → <b>按住 BOOT 键</b>不放的同时重新上电，保持约 1~2 秒松开。设备会启动热点 <b>WeatherClock-XXXX</b>，用手机连上该热点，浏览器打开 <b>http://192.168.4.1</b>，在此页填新的 WiFi 名称/密码并保存。<br>
<b>方式二（USB 串口）：</b>用数据线连接设备，打开 Web 烧录页面并接入串口，它会重新下发 WiFi 账号密码。<br>
<b>提示：</b>仅当网络临时断线时，设备会自动重连已保存的 WiFi，无需任何操作；以上两种方式用于彻底更换 WiFi 网络。
</div>
</details>
<label>和风天气 API Key（凭据选 API KEY 类型）</label>
<input name='k' value='$K$' placeholder='申请于 https://dev.qweather.com'>
<label>和风 API Host（控制台-开发者信息，如 abc123.re.qweatherapi.com）</label>
<input name='h' value='$H$' placeholder='例如 abc123.re.qweatherapi.com'>
<div class='row'>
<div><label>城市</label><input name='c' value='$C$' placeholder='如 北京'></div>
<div><label>经纬度(经,纬)</label><input name='g' value='$G$' placeholder='可选'></div>
</div>
<label>时区</label>
<select name='z'>$TZOPT$</select>
<label>行情轮播（屏幕底部一行：所选条目自动循环切换显示，10 分钟刷新数据）</label>
<div id='qslot'>$QSLOTS$</div>
<div class='row'><div style='flex:0 0 50%'><label>切换间隔（秒，2–60）</label>
<input type='number' name='qr' min='2' max='60' value='$QR$'></div></div>
<button type='button' class='btn qadd' onclick='addQSlot()'>+ 添加行情条目</button>
<input type='hidden' name='qlist' id='qlist'>
<div class='tips'>从下拉选择要轮播的行情（金价 / 布油 / 沪铜），可添加多条，保存后按顺序自动轮流显示；「自定义 JSON」需填 URL 与价格字段路径。</div>
<label>显示风格（点击即切换，屏幕立即刷新，无需重启）</label>
<input type='hidden' name='th' id='th' value='$THV$'>
<div id='themes'>$THEMES$</div>
<div id='thres'></div>
<button class='btn' type='button' onclick='testApi()'>测试 API · 刷新预览</button>
<button class='btn btn2' type='submit'>保存并重启</button>
</div>
</form>
</div>
<script>
var WICON={100:'☀',101:'🌤',102:'⛅',103:'☁',104:'☁',300:'🌦',301:'🌦',302:'⛈',303:'🌦',304:'⛈',305:'🌦',306:'🌦',307:'🌦',308:'🌧',309:'🌧',310:'🌧',311:'🌧',312:'⛈',313:'⛈',314:'⛈',315:'⛈',316:'⛈',317:'⛈',318:'⛈',399:'⛈',400:'🌨',401:'🌨',402:'🌨',403:'🌨',404:'🌨',405:'❄',406:'❄',407:'🌨',408:'❄',409:'❄',410:'❄',499:'🌨',500:'🌫',501:'🌫',502:'🌫',503:'🌫',504:'🌫',507:'🌫',508:'🌫',509:'🌫',510:'🌫',511:'🌫',512:'🌫',513:'🌫',514:'🌫',515:'🌫',999:'❓'};
function ico(c){return WICON[c]||'☁';}
var WTEXT={100:'晴',150:'晴',101:'多云',151:'多云',102:'少云',152:'少云',103:'晴间多云',153:'晴间多云',104:'阴',154:'阴',300:'阵雨',302:'雷阵雨',303:'强雷阵雨',305:'小雨',306:'中雨',307:'大雨',308:'极端降雨',310:'暴雨',399:'雨',400:'小雪',401:'中雪',402:'大雪',404:'雨夹雪',405:'雨夹雪',499:'雪',500:'薄雾',501:'雾',502:'霾',514:'雾'};
function fmt2(n){return (n<10?'0':'')+n;}
var WK=['周日','周一','周二','周三','周四','周五','周六'];
function applyTheme(t){
  var s=document.getElementById('scr');
  s.classList.toggle('plat',(t==='2'||t==='4'));
  s.classList.toggle('mario',t==='4');
  s.classList.toggle('retro',t==='1');
  s.classList.toggle('wasteland',t==='3');
}
applyTheme('$THV$');
function tick(){
  var d=new Date();
  document.getElementById('pv-date').textContent=WK[d.getDay()]+'  '+fmt2(d.getMonth()+1)+'/'+fmt2(d.getDate());
  document.getElementById('pv-clock').textContent=fmt2(d.getHours())+':'+fmt2(d.getMinutes());
}
setInterval(tick,1000);tick();
function syncCity(){document.getElementById('pv-city').textContent=q('c');}
var _ci=document.getElementById('cf').elements['c'];
if(_ci)_ci.addEventListener('input',syncCity);
syncCity();
function q(n){var e=document.getElementById('cf').elements[n];return e?e.value.trim():'';}
/* 行情轮播列表 builder */
var QTYPE=[['0','不显示'],['1','金价（Au99.99 · 元/克）'],['2','布伦特原油（美元/桶）'],['4','沪铜（上期所 · 元/吨）'],['3','自定义 JSON 数据源']];
function qRowHtml(t,url,path,label){
  var r=document.createElement('div');r.className='qrow';
  var hd=document.createElement('div');hd.className='qhead';
  var s='<select onchange="qType(this)">',x;
  for(x in QTYPE) s+="<option value='"+QTYPE[x][0]+"'"+(String(QTYPE[x][0])==String(t)?' selected':'')+">"+QTYPE[x][1]+"</option>";
  s+='</select>';
  hd.innerHTML=s+"<button type='button' class='qdel' onclick='delQRow(this)'>移除</button>";
  r.appendChild(hd);
  var cus=document.createElement('div');cus.className='qcus';cus.style.display=(String(t)==='3')?'block':'none';
  cus.innerHTML="<label>数据源 URL（https://…，返回 JSON）</label>"+
    "<input placeholder='https://example.com/api/quote' value='"+esc(url)+"' oninput='saveQList()'>"+
    "<label>价格字段 JSON 路径（点分级，如 data.f43）</label>"+
    "<input placeholder='data.f43' value='"+esc(path)+"' oninput='saveQList()'>"+
    "<label>显示标签（如 金价 / 汇率 / 股价）</label>"+
    "<input placeholder='金价' value='"+esc(label)+"' oninput='saveQList()'>";
  r.appendChild(cus);
  return r;
}
function addQSlot(t,url,path,label){
  document.getElementById('qslot').appendChild(qRowHtml(t||'0',url||'',path||'',label||''));
  saveQList();
}
function delQRow(btn){btn.closest('.qrow').remove();saveQList();}
function qType(sel){
  var r=sel.closest('.qrow');
  r.querySelector('.qcus').style.display=(sel.value==='3')?'block':'none';
  saveQList();
}
function saveQList(){
  var arr=[];document.querySelectorAll('#qslot .qrow').forEach(function(r){
    var t=r.querySelector('select').value;
    if(t==='0')return;
    var inps=r.querySelectorAll('.qcus input');
    if(t==='3')arr.push('3;;'+inps[0].value.trim()+';;'+inps[1].value.trim()+';;'+inps[2].value.trim());
    else arr.push(t);
  });
  document.getElementById('qlist').value=arr.join('\n');
}
if(!document.getElementById('qslot').children.length)addQSlot();   // 无已配置条目时默认给一空行
document.getElementById('cf').addEventListener('submit',function(){saveQList();});
function pickTheme(el){
  document.querySelectorAll('.th').forEach(function(x){x.classList.remove('sel');});
  el.classList.add('sel');
  document.getElementById('th').value=el.dataset.t;
  applyTheme(el.dataset.t);
  var res=document.getElementById('thres');
  res.textContent='正在切换到 '+el.dataset.n+' …';
  fetch('/api/theme',{method:'POST',body:'t='+el.dataset.t,headers:{'Content-Type':'application/x-www-form-urlencoded'}})
  .then(function(r){return r.json();})
  .then(function(j){res.textContent=j.ok?('✓ 已切换到 '+el.dataset.n+'，屏幕已刷新'):'✗ 切换失败';})
  .catch(function(e){res.textContent='✗ 切换失败：'+e;});
}
function testApi(){
  var res=document.getElementById('res');
  res.style.display='block';res.className='';res.textContent='正在请求设备真实拉取数据…';
  saveQList();
  var body=new URLSearchParams();
  body.append('k',q('k'));body.append('h',q('h'));body.append('c',q('c'));body.append('g',q('g'));
  body.append('qlist',document.getElementById('qlist').value);
  fetch('/api/preview',{method:'POST',body:body,headers:{'Content-Type':'application/x-www-form-urlencoded'}})
  .then(function(r){return r.json();})
  .then(function(j){
    if(j.ok===1){
      res.className='ok';
      res.textContent='✓ 天气成功\n城市='+j.city+' ('+j.id+')\n天气='+j.text+'\n温度='+j.temp+'°C  体感='+j.feels+'°C  湿度='+j.hum+'%'+(j.qok===1?'\n行情='+j.qlab+' '+j.qprice+' '+j.qunit:'');
      document.getElementById('pv-icon').textContent=ico(j.icon);
      document.getElementById('pv-wtext').textContent=j.text||WTEXT[j.icon]||'天气';
      document.getElementById('pv-temp').textContent=Math.round(j.temp);
      document.getElementById('pv-meta').textContent='湿度 '+j.hum+' / 体感 '+Math.round(j.feels)+'°';
    }else{
      res.textContent='✗ 失败：'+(j.err||'未知错误');
    }
    if(j.qok===1){
      var pc=j.qpct>0.005?'up':(j.qpct<-0.005?'dn':'');
      document.getElementById('pv-q').innerHTML=esc(j.qlab)+'<span class=qp>'+j.qprice+'</span> '+(j.qunit||'');
      document.getElementById('pv-qpct').innerHTML=pc?'<span class='+pc+'>'+(j.qpct>0?'+':'')+Number(j.qpct).toFixed(2)+'%</span>':'';
    }
  })
  .catch(function(e){res.textContent='✗ 请求设备失败：'+e;});
}
function otaStart(){
  var f=document.getElementById('otafile').files[0];
  var op=document.getElementById('ota');
  if(!f){op.textContent='请先选择 .bin 固件文件';return;}
  var x=new XMLHttpRequest();
  x.open('POST','/update');
  x.upload.onprogress=function(e){if(e.lengthComputable)op.textContent='上传中 '+Math.round(e.loaded/e.total*100)+'%…';};
  x.onload=function(){var r={};try{r=JSON.parse(x.responseText)}catch(_){}
    if(r.ok===1){op.textContent='✓ 刷机成功，设备重启中，约 10 秒后重新连接…';}
    else{op.textContent='✗ 失败：'+(r.err||('HTTP '+x.status));}};
  x.onerror=function(){op.textContent='✗ 连接中断（设备可能正在重启）';};
  var fd=new FormData();fd.append('bin',f);x.send(fd);
}
function otaCheck(){
  var o=document.getElementById('ota');
  o.textContent='正在查询 GitHub 最新版本…';
  fetch('/api/fwcheck').then(function(r){return r.json();}).then(function(j){
    if(j.ok!==1){o.textContent='✗ 检查失败：'+j.err;return;}
    if(j.hasnew){o.textContent='发现新版本 '+j.latest+'（当前 '+j.cur+'），点击右侧按钮开始更新';
      document.getElementById('otadl').style.display='';}
    else{o.textContent='✓ 已是最新版本';document.getElementById('otadl').style.display='none';}
  }).catch(function(e){o.textContent='✗ 检查失败：'+e;});
}
function otaFlash(){
  var b=document.getElementById('otadl');
  var o=document.getElementById('ota');
  b.disabled=true;
  fetch('/api/fwupdate',{method:'POST'}).then(function(r){return r.json();}).then(function(j){
    if(j.ok!==1){o.textContent='✗ 无法开始：'+j.err;b.disabled=false;return;}
    var t=setInterval(function(){
      fetch('/api/fwstatus').then(function(r){return r.json();}).then(function(s){
        if(s.state===1){o.textContent='下载中 '+s.pct+'%…';}
        else if(s.state===2){clearInterval(t);o.textContent='✓ 刷机完成，设备重启中，约 10 秒后重新连接…';}
        else if(s.state===3){clearInterval(t);o.textContent='✗ 失败：'+s.err;b.disabled=false;}
      }).catch(function(){clearInterval(t);o.textContent='连接中断（设备正在重启）';});
    },1000);
  }).catch(function(e){o.textContent='✗ 请求失败：'+e;b.disabled=false;});
}
function esc(s){return String(s).replace(/[&<>]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;'}[c];});}
</script></body></html>
)HTML";

// 页面变量替换
static String renderPage(const AppConfig& cfg, const String& ipText) {
  String s = FPSTR(PAGE_HTML);
  String latlon;
  if (cfg.lat != 0 || cfg.lon != 0) {
    latlon = String(cfg.lon, 2) + "," + String(cfg.lat, 2);
  }
  s.replace("$IP$", ipText);
  s.replace("$S$", cfg.wifi_ssid);
  s.replace("$K$", cfg.qweather_key);
  s.replace("$H$", cfg.qweather_host);
  s.replace("$C$", cfg.city_name);
  s.replace("$G$", latlon);
  s.replace("$TZOPT$", renderTzOptions(cfg.timezone));
  s.replace("$QSLOTS$", renderQSlots(cfg));
  s.replace("$QR$", String(cfg.quote_rotate_s));
  s.replace("$THEMES$", renderThemeOptions(cfg.theme));
  s.replace("$THV$", String(cfg.theme));
  s.replace("$FWV$", FWV_STR);
  return s;
}

// ---- /api/preview：用表单填写的 Key/Host/城市 真实拉取一次天气并返回 JSON ----
static void handleApiPreview(WebServer& server) {
  AppConfig t;
  t.qweather_key = server.arg("k");
  t.qweather_key.trim();
  t.qweather_host = server.arg("h");
  t.qweather_host.trim();
  t.qweather_host.replace("https://", "");
  t.qweather_host.replace("http://", "");
  while (t.qweather_host.endsWith("/")) t.qweather_host.remove(t.qweather_host.length() - 1);
  t.city_name = server.arg("c");
  t.city_name.trim();
  String g = server.arg("g");
  g.trim();
  if (g.length() > 0) {
    int co = g.indexOf(',');
    if (co > 0) {
      t.lon = g.substring(0, co).toFloat();
      t.lat = g.substring(co + 1).toFloat();
    }
  }
  // 行情参数：解析轮播列表（预览只测第一个有效条目）
  parseQuoteSlots(server.arg("qlist"), t);

  String okBody, err;
  if (t.qweather_key.length() == 0) {
    err = "缺少 API Key";
  } else if (t.city_name.length() == 0 && (t.lat == 0 && t.lon == 0)) {
    err = "缺少城市名或经纬度";
  } else {
    if (t.lat == 0 && t.lon == 0 && t.location_id.length() == 0) {
      String id;
      if (geoResolveCity(t.qweather_key, t.qweather_host, t.city_name, id)) t.location_id = id;
      else err = "城市解析失败（请核对 API Key / Host / 城市名）";
    }
    if (err.length() == 0) {
      WeatherData w;
      if (fetchWeather(t, w) && w.ok) {
        okBody = "{\"ok\":1,\"city\":\"" + jsonEsc(t.city_name) + "\",\"id\":\"" + jsonEsc(t.location_id) +
                 "\",\"temp\":" + String(w.temp, 1) + ",\"feels\":" + String(w.feels, 1) +
                 ",\"hum\":" + String(w.humidity) + ",\"icon\":" + String(w.icon) +
                 ",\"text\":\"" + jsonEsc(w.text) + "\"}";
        Serial.printf("[api] preview OK city=%s temp=%.1f text=%s\n",
                      t.city_name.c_str(), w.temp, w.text.c_str());
      } else {
        err = "天气接口返回失败（检查 API Key / Host 是否正确）";
      }
    }
  }
  // 行情测试（独立于天气，无论天气成败都尝试）
  String quoteSeg;
  if (quoteSlotCount(t) > 0) {
    QuoteData qd;
    if (fetchQuote(t, 0, qd) && qd.ok) {
      quoteSeg = ",\"qok\":1,\"qlab\":\"" + jsonEsc(qd.label) + "\",\"qprice\":\"" +
                 String(qd.price, qd.decimals) + "\",\"qunit\":\"" + jsonEsc(qd.unit) +
                 "\",\"qpct\":" + String(qd.changePct, 2);
      Serial.printf("[api] preview quote %s %.2f\n", qd.label.c_str(), qd.price);
    } else {
      quoteSeg = ",\"qok\":0";
    }
  }
  if (okBody.length() == 0) okBody = "{\"ok\":0,\"err\":\"" + jsonEsc(err) + "\"}";
  okBody = okBody.substring(0, okBody.length() - 1) + quoteSeg + "}";
  server.send(200, "application/json; charset=utf-8", okBody);
}

// ---- /api/theme：主题即时切换（保存 NVS + 立即应用，不重启） ----
static void handleApiTheme(WebServer& server, AppConfig& cfg) {
  int t = server.arg("t").toInt();
  if (!themeIdValid((uint8_t)t)) {
    server.send(400, "application/json; charset=utf-8", "{\"ok\":0,\"err\":\"bad theme id\"}");
    return;
  }
  cfg.theme = (uint8_t)t;
  saveTheme(cfg.theme);                 // 持久化，重启后保留
  themeApply((DisplayTheme)t);          // 下一次 themeTick 全屏重绘为新主题
  Serial.printf("[theme] switch -> %s (%d)\n", themeName((DisplayTheme)t), t);
  server.send(200, "application/json; charset=utf-8", "{\"ok\":1}");
}

// /save 处理（两种模式共用）
static void handleSave(WebServer& server, AppConfig& cfg) {
  cfg.valid = true;
  cfg.wifi_ssid    = server.arg("s");
  // 密码框留空则沿用已保存的密码（避免改其他配置时把 WiFi 密码清掉）
  String pass = server.arg("p");
  pass.trim();
  if (pass.length() > 0) cfg.wifi_pass = pass;
  cfg.qweather_key = server.arg("k");
  cfg.qweather_host = server.arg("h");
  cfg.qweather_host.trim();
  // 容错：去掉协议前缀和末尾斜杠，只留主机名
  cfg.qweather_host.replace("https://", "");
  cfg.qweather_host.replace("http://", "");
  while (cfg.qweather_host.endsWith("/")) cfg.qweather_host.remove(cfg.qweather_host.length() - 1);
  cfg.city_name    = server.arg("c");
  cfg.timezone     = server.arg("z");   // POSIX 时区串；空则用默认 UTC+8
  // 行情轮播配置
  parseQuoteSlots(server.arg("qlist"), cfg);
  cfg.quote_rotate_s = constrain(server.arg("qr").toInt(), 2, 60);
  // 显示风格（隐藏域随表单提交；非法值回退 Modern）
  {
    int th = server.arg("th").toInt();
    cfg.theme = themeIdValid((uint8_t)th) ? (uint8_t)th : (uint8_t)THEME_MODERN;
  }
  // 解析经纬度（若填写）
  String g = server.arg("g");
  g.trim();
  if (g.length() > 0) {
    int comma = g.indexOf(',');
    if (comma > 0) {
      cfg.lon = g.substring(0, comma).toFloat();
      cfg.lat = g.substring(comma + 1).toFloat();
    }
  }
  // 城市名和经纬度都留空则无法定位
  bool ok = cfg.qweather_key.length() > 0 &&
            cfg.city_name.length() > 0 &&
            cfg.wifi_ssid.length() > 0;
  String html;
  if (!ok) {
    html = F("<html><body style='background:#0b1022;color:#eee;font-family:sans-serif;"
             "text-align:center;padding-top:60px'><h2>❌ 信息不完整</h2>"
             "<p>SSID、API Key 与城市至少需要填写。</p>"
             "<p><a href='/' style='color:#7ff'>返回</a></p></body></html>");
    server.send(200, "text/html; charset=utf-8", html);
    return;
  }
  saveConfig(cfg);   // location_id 等到连上 WiFi 后再解析
  Serial.printf("[save] ssid=%s keylen=%d host='%s' city=%s tz='%s'\n",
                cfg.wifi_ssid.c_str(), cfg.qweather_key.length(),
                cfg.qweather_host.c_str(), cfg.city_name.c_str(), cfg.timezone.c_str());
  html = F("<html><body style='background:#0b1022;color:#eee;font-family:sans-serif;"
           "text-align:center;padding-top:60px'><h2>✔ 已保存</h2>"
           "<p>设备即将重启并连接 WiFi…正在等待，稍后自动返回配置页。</p>"
           "<p><a href='/' style='color:#7ff' onclick='return false'>若长时间无跳转，请点击这里刷新</a></p>"
           "<script>"
           "(function(){var t=0;"
           "function go(){t++;fetch('/',{cache:'no-store'}).then(function(r){"
           "if(r.ok){location.href='/';}else if(t<50)setTimeout(go,1200);"
           "}).catch(function(){if(t<50)setTimeout(go,1200);else location.href='/';});}"
           "go();})();"
           "</script></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
  delay(600);
  ESP.restart();
}

// 注册路由（两种模式共用）；ipText 按值传入并捕获，避免悬垂引用
// ---- 在线固件更新：版本比对走 /api/fwcheck，下载由后台任务执行，进度轮询 /api/fwstatus ----
// 源用 version.txt 里的短 SHA pin 住 URL（immutable，不受 CDN 缓存影响）。
// jsDelivr 主节点(cdn)在某些网络连不通，故提供多个镜像自动兜底，全部失败才报错。
static const char FW_REPO[] = "Escaper929/WeatherClock";
enum { FW_IDLE = 0, FW_DL = 1, FW_OK = 2, FW_FAIL = 3 };
static volatile int     gFwState = FW_IDLE;
static volatile int     gFwPct   = 0;
static String gFwErr;

// 镜像 base：jsDelivr 用 @ref，GitHub RAW 用 /ref；尾部统一拼 /web/xxx
static String fwBaseUrl(int i, const String& ref) {
  switch (i) {
    case 0: return String("https://cdn.jsdelivr.net/gh/") + FW_REPO + "@" + ref;
    case 1: return String("https://fastly.jsdelivr.net/gh/") + FW_REPO + "@" + ref;
    case 2: return String("https://gcore.jsdelivr.net/gh/") + FW_REPO + "@" + ref;
    case 3: return String("https://testingcf.jsdelivr.net/gh/") + FW_REPO + "@" + ref;
    default: return String("https://raw.githubusercontent.com/") + FW_REPO + "/" + ref;
  }
}
static const int FW_BASES_N = 5;   // 4 个 jsDelivr 节点 + GitHub RAW

static bool fwHttpBegin(WiFiClientSecure& cl, HTTPClient& http, const String& url) {
  cl.setInsecure();               // 与 Weather.cpp 同策略：不校验证书，省 flash/RAM
  cl.setTimeout(10);              // 秒
  return http.begin(cl, url);
}

// 拉取仓库 web/ 子目录下的文件，依次尝试各镜像，失败返回空串。
// 关键：这些文件必须拿到「最新版」。jsDelivr @main 是可变引用，CDN 可能返回旧缓存，
// 一旦命中也无法判断新旧，会误判；故优先用 GitHub Raw（直连 git、无 CDN 缓存），
// 只有它连不通时才回退到 4 个 jsDelivr 节点作国内兜底。
static String fwFetchWebFile(const char* name) {
  // 尝试顺序：先 GitHub Raw，再 jsDelivr 4 节点（镜像 base 索引 4 = raw）
  static const int ORDER[FW_BASES_N] = {4, 0, 1, 2, 3};
  for (int k = 0; k < FW_BASES_N; k++) {
    int i = ORDER[k];
    WiFiClientSecure cl;
    HTTPClient http;
    String url = fwBaseUrl(i, "main") + String("/web/") + name + "?t=" + millis();
    String out;
    if (fwHttpBegin(cl, http, url) && http.GET() == 200) {
      out = http.getString();
      out.trim();
    }
    http.end();
    if (out.length() > 0) return out;
  }
  return "";
}

// 最新版本标识：短SHA + 提交时间（与刷入固件的 FwVersion 一致，用于版本比对）
static String fwFetchLatest()   { return fwFetchWebFile("version.txt"); }
// 承载本次固件的 bin 提交 SHA（CI 写入）。固件文件存在该 SHA 对应的镜像 URL 下，
// 与 version.txt 的「源码 SHA」解耦：比对走源码标识，下载走 bin 定位，避免拉到旧固件。
static String fwFetchBinSha()   { return fwFetchWebFile("bin_sha.txt"); }

// 版本串形如 "xxxxxxx YYYY-MM-DDTHH:MM"（Git 提交时间，分钟级，兼容旧的 "xxxxxxx YYYY-MM-DD"）。
// 完全相等=同版本；否则比较提交时间串（ISO 格式字符串序即时间序），
// 支持同一天多次发布：只要线上提交时间比当前新就提示更新，不会误报降级。
static bool fwHasNew(const String& latest) {
  if (latest == String(FWV_STR)) return false;
  String ld = latest.substring(latest.indexOf(' ') + 1); ld.trim();
  String cd = String(FWV_STR); cd = cd.substring(cd.indexOf(' ') + 1); cd.trim();
  if (ld.length() == 0 || cd.length() == 0) return true;
  return ld > cd;   // "2026-09-15T08:30" 字符串比较即时间序
}

static void handleFwCheck(WebServer& server) {
  String latest = fwFetchLatest();
  if (latest.length() == 0) {
    server.send(200, "application/json", "{\"ok\":0,\"err\":\"无法连接固件镜像（已尝试 jsDelivr 与 GitHub Raw）\"}");
    return;
  }
  bool hasnew = fwHasNew(latest);
  server.send(200, "application/json",
              String("{\"ok\":1,\"cur\":\"") + FWV_STR + "\",\"latest\":\"" + latest +
              "\",\"hasnew\":" + (hasnew ? "1" : "0") + "}");
}

// 后台下载任务：读 Content-Length 定长流式写入 OTA 分区，完成后 4s 重启。
// 下载用 bin_sha.txt 里的 bin 提交 SHA 定位固件（不可变 URL，且该提交正好承载本次构建固件，
// 不会像 version.txt 的源码 SHA 那样命中旧 bin）。
static void fwTask(void*) {
  gFwState = FW_DL; gFwPct = 0; gFwErr = "";
  String sha = fwFetchBinSha();
  sha.trim();
  bool ok = false;
  if (sha.length() < 7) {
    gFwErr = "固件版本信息获取失败";
  } else {
    // 依次尝试各镜像下载固件（定长流式写 OTA 分区）
    for (int i = 0; i < FW_BASES_N && !ok; i++) {
      WiFiClientSecure cl;
      HTTPClient http;
      if (!fwHttpBegin(cl, http, fwBaseUrl(i, sha) + "/web/firmware.bin") || http.GET() != 200) {
        http.end();
        continue;   // 该镜像连不通/非 200，换下一个
      }
      int len = http.getSize();
      if (len <= 0) {
        gFwErr = "固件大小未知";
        http.end();
        continue;
      }
      if (!Update.begin(len, U_FLASH)) {
        gFwErr = Update.errorString();
        http.end();
        continue;
      }
      Stream* s = http.getStreamPtr();
      uint8_t buf[1024];
      int got = 0;
      while (got < len) {
        int n = s->readBytes(buf, (size_t)min(len - got, (int)sizeof(buf)));
        if (n <= 0) { gFwErr = "下载中断"; break; }
        if (Update.write(buf, n) != (size_t)n) { gFwErr = "写入失败"; break; }
        got += n;
        gFwPct = got * 100 / len;
      }
      if (got >= len && Update.end(true)) ok = true;   // true = 校验并设为启动分区
      else if (gFwErr.length() == 0) gFwErr = Update.errorString();
      http.end();
      if (!ok) Update.abort();   // 该镜像下载未成功，重置 OTA 状态再试下一个
    }
  }
  if (ok) {
    gFwState = FW_OK;
    vTaskDelay(pdMS_TO_TICKS(4000));   // 让浏览器轮询到完成状态
    ESP.restart();
  } else {
    Update.abort();
    gFwState = FW_FAIL;
  }
  vTaskDelete(NULL);
}

static void handleFwUpdate(WebServer& server) {
  if (gFwState == FW_DL) {
    server.send(200, "application/json", "{\"ok\":0,\"err\":\"已有更新在进行\"}");
    return;
  }
  if (xTaskCreate(fwTask, "fwota", 8192, NULL, 1, NULL) != pdPASS) {
    server.send(200, "application/json", "{\"ok\":0,\"err\":\"任务创建失败\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":1}");
}

static void handleFwStatus(WebServer& server) {
  server.send(200, "application/json",
              String("{\"state\":") + gFwState + ",\"pct\":" + gFwPct +
              ",\"err\":\"" + gFwErr + "\"}");
}

// ---- /update：网页 OTA 刷机（流式写入 OTA 分区，校验通过后自动重启） ----
static void handleOtaData(WebServer& server) {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[ota] start: %s\n", up.filename.c_str());
    Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    bool ok = Update.end(true);   // true = 校验后设为启动分区
    Serial.printf("[ota] end: %s (%u bytes)\n", ok ? "OK" : Update.errorString(), up.totalSize);
  }
}

static void handleOtaUpload(WebServer& server) {
  if (Update.hasError()) {
    String err = Update.errorString();
    Update.abort();
    server.send(500, "application/json", "{\"ok\":0,\"err\":\"" + err + "\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":1}");
  delay(1200);          // 让响应冲刷到浏览器
  ESP.restart();        // 切换到新固件
}

static void registerRoutes(WebServer& server, AppConfig& cfg, String ipText) {
  server.on("/", HTTP_GET, [&, ipText]() {
    server.send(200, "text/html; charset=utf-8", renderPage(cfg, ipText));
  });
  server.on("/save", HTTP_POST, [&]() {
    handleSave(server, cfg);
  });
  server.on("/api/preview", HTTP_POST, [&]() {
    handleApiPreview(server);
  });
  server.on("/api/theme", HTTP_POST, [&]() {
    handleApiTheme(server, cfg);
  });
  server.on("/update", HTTP_POST, [&]() {
    handleOtaUpload(server);
  }, [&]() {
    handleOtaData(server);
  });
  server.on("/api/fwcheck", HTTP_GET, [&]() {
    handleFwCheck(server);
  });
  server.on("/api/fwupdate", HTTP_POST, [&]() {
    handleFwUpdate(server);
  });
  server.on("/api/fwstatus", HTTP_GET, [&]() {
    handleFwStatus(server);
  });
  server.onNotFound([&]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/html", "");
  });
}

// ---- softAP 首次配置门户（阻塞） ----
void portalEnter(const AppConfig& current) {
  // 生成 AP SSID：WeatherClock-<MAC后4位>
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String suffix = mac.length() >= 4 ? mac.substring(mac.length() - 4) : "0001";
  String apSsid = "WeatherClock-" + suffix;

  // C3 SuperMini WiFi 初始化：AP+STA（STA 用于射频扫描诊断），关闭省电
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  // C3 SuperMini 批次天线问题：满功率发射不可见，必须限功率（见 main.cpp capWifiTxPower）
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  delay(100);

  // ---- 射频诊断：扫描周围 AP，验证 RX/天线是否工作 ----
  Serial.println("[diag] scanning nearby WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.printf("[diag] scan found %d networks:\n", n);
  for (int i = 0; i < n && i < 12; i++) {
    Serial.printf("[diag]   %d: %s  ch=%d rssi=%d\n", i,
                  WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
  }
  WiFi.scanDelete();

  // 尝试开启 softAP（重试 3 次）
  bool apOk = false;
  for (int i = 0; i < 3 && !apOk; i++) {
    apOk = WiFi.softAP(apSsid.c_str(), nullptr, 11, 0, 4);
    Serial.printf("[portal] softAP try %d: %s\n", i + 1, apOk ? "OK" : "FAIL");
    if (!apOk) delay(500);
  }
  delay(200);  // 给 RF 校准时间

  if (!apOk) {
    Serial.println("[portal] softAP 全部失败！重启...");
    delay(2000);
    ESP.restart();
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[portal] AP: %s  IP: %s  ch=%d  mac=%s\n",
                apSsid.c_str(), apIP.toString().c_str(),
                WiFi.channel(), WiFi.softAPmacAddress().c_str());
  Serial.println("[portal] 请在浏览器访问 http://192.168.4.1");

  WebServer server(80);
  AppConfig cfg = current;
  String ipText = "AP: " + apIP.toString();
  registerRoutes(server, cfg, ipText);
  server.begin();
  while (true) {
    server.handleClient();
    delay(10);
  }
}

// ---- STA 常驻配置服务器（非阻塞） ----
void portalServerBegin(const AppConfig& current) {
  gCfg = current;
  if (gServer) delete gServer;
  gServer = new WebServer(80);
  String ip = WiFi.localIP().toString();
  String ipText = "STA: " + ip + " · 修改配置保存后自动重启";
  registerRoutes(*gServer, gCfg, ipText);
  gServer->begin();
  Serial.printf("[portal] 配置页: http://%s\n", ip.c_str());
  if (MDNS.begin("weatherclock")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[portal] mDNS 配置页: http://weatherclock.local");
  }
}

void portalServerLoop() {
  if (gServer) gServer->handleClient();
}