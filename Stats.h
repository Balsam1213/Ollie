#pragma once
/*
 * 三项数值（饱食/心情/精力）+ NVS 存档，断电重启不丢
 */
#include <Arduino.h>
#include <Preferences.h>

struct StatsData {
    uint8_t food;   // 饱食度 0-100
    uint8_t mood;   // 心情 0-100
    uint8_t energy; // 精力 0-100
};

class Stats {
public:
    void begin();              // 从 NVS 读取；首次上电给默认值
    void tick(bool sleeping);  // 每分钟结算：醒着三项各 -1；睡觉时精力 +2、其余停
    void onFeed();             // 喂食：饱食 +20，心情 +4
    void onPet();              // 抚摸：心情 +15
    const StatsData& data() const { return d; }

private:
    static uint8_t clampAdd(uint8_t v, int delta);
    StatsData d{};
    Preferences prefs;
};
