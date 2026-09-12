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
#include <time.h>

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

// 行情源下拉选项
static String renderQmOptions(int cur) {
  struct O { int v; const char* t; };
  static const O opts[] = {
    {0, "不显示"},
    {1, "金价（积存金参考 · Au99.99 元/克）"},
    {2, "布伦特原油（美元/桶）"},
    {4, "沪铜（上期所主力连续 · 元/吨）"},
    {3, "自定义 JSON 数据源"},
  };
  String s;
  for (auto& o : opts) {
    s += "<option value='" + String(o.v) + "'" + (o.v == cur ? " selected" : "") +
         ">" + o.t + "</option>";
  }
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
.wrap{max-width:420px;width:100%}
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
#res{display:none;margin-top:10px;font-size:12px;line-height:1.6;padding:10px;border-radius:8px;background:var(--in);border:1px solid var(--line);white-space:pre-wrap;color:var(--txt)}
#res.ok{border-color:#3a6}
.tips{font-size:11px;color:var(--dim);margin-top:12px;line-height:1.6}
/* 显示风格主题卡片 */
.th{display:flex;align-items:center;gap:10px;padding:9px 11px;margin-top:6px;border:1px solid var(--line);border-radius:8px;background:var(--in);cursor:pointer}
.th b{font-size:13px;min-width:58px}
.th .d{font-size:11px;color:var(--dim);flex:1}
.th.sel{border-color:var(--acc);background:#122a20}
.th.sel b{color:var(--acc)}
#thres{font-size:11px;color:var(--acc);margin-top:6px;min-height:14px}
</style></head><body><div class='wrap'>
<div class='card'>
<h1>☀ WeatherClock</h1><small>$IP$</small>
<label>屏幕预览（当前浏览器本地时间，保存后设备按所选时区显示）</label>
<div id='scr'>
  <div class='pdate' id='pv-date'>--</div>
  <div class='pcity' id='pv-city'></div>
  <div class='pclock' id='pv-clock'>--:--</div>
  <div class='pfrule'></div>
  <div class='picon' id='pv-icon'>☀</div>
  <div class='pwtext' id='pv-wtext'>晴</div>
  <div class='ptemp'><span id='pv-temp'>--</span><span class='pdeg'>°C</span></div>
  <div class='pmeta' id='pv-meta'>湿度 -- / 体感 --</div>
  <div class='pqrow'><span id='pv-q'></span><span id='pv-qpct'></span></div>
</div>
<div id='res'></div>
</div>
<form id='cf' method='post' action='/save'>
<div class='card'>
<label>WiFi 名称 (SSID)</label>
<input name='s' value='$S$' placeholder='例如 MyWiFi'>
<label>WiFi 密码（留空则保持不变）</label>
<input type='password' name='p' placeholder='WiFi 密码'>
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
<label>行情显示（时钟下方一行，10 分钟刷新）</label>
<select name='qm' id='qm' onchange='toggleQ()'>$QMOPT$</select>
<div id='qcustom'>
<label>数据源 URL（https://…，返回 JSON）</label>
<input name='qu' value='$QU$' placeholder='https://example.com/api/quote'>
<label>价格字段 JSON 路径（点分级，如 data.f43）</label>
<input name='qp' value='$QP$' placeholder='data.f43'>
<label>显示标签（如 金价 / 汇率 / 股价）</label>
<input name='ql' value='$QL$' placeholder='金价'>
</div>
<label>显示风格（点击即切换，屏幕立即刷新，无需重启）</label>
<input type='hidden' name='th' id='th' value='$THV$'>
<div id='themes'>$THEMES$</div>
<div id='thres'></div>
<button class='btn' type='button' onclick='testApi()'>测试 API · 刷新预览</button>
<button class='btn btn2' type='submit'>保存并重启</button>
<div class='tips'>「测试 API」会用当前填写的 Key / Host / 城市真实请求一次和风接口，并把结果渲染到上方预览，确认无误后再点「保存并重启」。</div>
</div>
</form>
</div>
<script>
var WICON={100:'☀',101:'🌤',102:'⛅',103:'☁',104:'☁',300:'🌦',301:'🌦',302:'⛈',303:'🌦',304:'⛈',305:'🌦',306:'🌦',307:'🌦',308:'🌧',309:'🌧',310:'🌧',311:'🌧',312:'⛈',313:'⛈',314:'⛈',315:'⛈',316:'⛈',317:'⛈',318:'⛈',399:'⛈',400:'🌨',401:'🌨',402:'🌨',403:'🌨',404:'🌨',405:'❄',406:'❄',407:'🌨',408:'❄',409:'❄',410:'❄',499:'🌨',500:'🌫',501:'🌫',502:'🌫',503:'🌫',504:'🌫',507:'🌫',508:'🌫',509:'🌫',510:'🌫',511:'🌫',512:'🌫',513:'🌫',514:'🌫',515:'🌫',999:'❓'};
function ico(c){return WICON[c]||'☁';}
var WTEXT={100:'晴',150:'晴',101:'多云',151:'多云',102:'少云',152:'少云',103:'晴间多云',153:'晴间多云',104:'阴',154:'阴',300:'阵雨',302:'雷阵雨',303:'强雷阵雨',305:'小雨',306:'中雨',307:'大雨',308:'极端降雨',310:'暴雨',399:'雨',400:'小雪',401:'中雪',402:'大雪',404:'雨夹雪',405:'雨夹雪',499:'雪',500:'薄雾',501:'雾',502:'霾',514:'雾'};
function fmt2(n){return (n<10?'0':'')+n;}
var WK=['周日','周一','周二','周三','周四','周五','周六'];
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
function toggleQ(){document.getElementById('qcustom').style.display=(q('qm')==='3')?'block':'none';}
toggleQ();
function pickTheme(el){
  document.querySelectorAll('.th').forEach(function(x){x.classList.remove('sel');});
  el.classList.add('sel');
  document.getElementById('th').value=el.dataset.t;
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
  var body=new URLSearchParams();
  body.append('k',q('k'));body.append('h',q('h'));body.append('c',q('c'));body.append('g',q('g'));
  body.append('qm',q('qm'));body.append('qu',q('qu'));body.append('qp',q('qp'));body.append('ql',q('ql'));
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
  s.replace("$QMOPT$", renderQmOptions(cfg.quote_mode));
  s.replace("$QU$", cfg.quote_url);
  s.replace("$QP$", cfg.quote_path);
  s.replace("$QL$", cfg.quote_label);
  s.replace("$THEMES$", renderThemeOptions(cfg.theme));
  s.replace("$THV$", String(cfg.theme));
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
  // 行情参数
  t.quote_mode  = server.arg("qm").toInt();
  t.quote_url   = server.arg("qu"); t.quote_url.trim();
  t.quote_path  = server.arg("qp"); t.quote_path.trim();
  t.quote_label = server.arg("ql"); t.quote_label.trim();

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
  if (t.quote_mode > 0) {
    QuoteData qd;
    if (fetchQuote(t, qd) && qd.ok) {
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
  // 行情配置
  cfg.quote_mode   = server.arg("qm").toInt();
  cfg.quote_url    = server.arg("qu"); cfg.quote_url.trim();
  cfg.quote_path   = server.arg("qp"); cfg.quote_path.trim();
  cfg.quote_label  = server.arg("ql"); cfg.quote_label.trim();
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
           "<p>设备即将重启并连接 WiFi…</p></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
  delay(600);
  ESP.restart();
}

// 注册路由（两种模式共用）；ipText 按值传入并捕获，避免悬垂引用
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