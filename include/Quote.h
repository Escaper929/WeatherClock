// ============================================================================
//  Quote.h - 行情数据客户端（金价 / 布伦特原油 / 自定义 JSON 数据源）
//  预设数据源：东方财富 push2 行情接口（免鉴权、HTTPS、JSON）
//    金价(积存金参考): secid=118.AU9999  上海黄金交易所 Au99.99 现货，元/克
//    布伦特原油:       secid=112.B00Y    布油当月连续，美元/桶
//  自定义：用户在配置页填完整 URL + JSON 价格字段路径（如 data.f43）
// ============================================================================
#pragma once
#include <Arduino.h>
#include "AppConfig.h"

struct QuoteData {
  bool   ok = false;
  String label;       // 显示标签，如 "金价" / "布油" / 用户自定义
  float  price = 0;   // 最新价
  float  changePct = 0; // 涨跌幅 %（预设有效；自定义无此数据时为 0）
  int    decimals = 2;  // 价格小数位
  String unit;        // 单位，如 "元/克" / "美元/桶"
};

// 有效行情条目数（<= QUOTE_SLOTS；槽位无配置或不可用则返回实际启用数）
int quoteSlotCount(const AppConfig& cfg);

// 拉取第 index 个（0 起）行情条目。该槽位未启用时直接返回 false
bool fetchQuote(const AppConfig& cfg, int index, QuoteData& out);
