#pragma once
/*
 * 心情灯：板载 WS2812，颜色随心情/状态变化
 *   心情>=70 青绿缓慢呼吸 | 40-69 琥珀黄常亮 | <40 橙红慢闪
 *   睡眠 深蓝极慢呼吸 | 互动后 3 秒粉色脉动
 * 颜色切换做 ~200ms 渐变，亮度整体封顶防刺眼。
 */
#include <Arduino.h>
#include "Config.h"

class MoodLight {
public:
    void begin(uint32_t now);
    void setMood(uint8_t moodPercent, bool sleeping); // 每轮结算后刷新
    void happyFlash(uint32_t now);                    // 互动后 3 秒粉色
    void update(uint32_t now);                        // 调度器每帧调用

private:
    void targetColor(uint32_t now, uint8_t& r, uint8_t& g, uint8_t& b) const;

    uint8_t mood = 80;
    bool sleeping = false;
    uint32_t happyUntil = 0;
    int curR = 0, curG = 0, curB = 0; // 当前输出，用于渐变
};
