// ============================================================================
//  Quote.h - 行情数据客户端（金价 / 布伦特原油 / 自定义 JSON 数据源）
//  预设数据源：京东金融·浙商积存金（金价）+ 东方财富 push2（布仑特原油/沪铜）
//    金价: GET api.jdjygold.com/gw2/generic/jrm/h5/m/stdLatestPrice?productSku=1961543816（元/克）
//    布伦特原油: secid=112.B00Y    布油当月连续，美元/桶
//    沪铜指数:   secid=113.CUM     上期所铜，元/吨
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
