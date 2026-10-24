#pragma once
/*
 * 光敏模块：ADC 采样 + 滑动均值 + 迟滞判定
 * 持续变暗 ≥10s → SleepTime；变亮 → WakeTime
 */
#include <Arduino.h>
#include "Config.h"

enum class LightEvent : uint8_t { None, SleepTime, WakeTime };

class LightSensor {
public:
    void begin(uint32_t now);
    LightEvent update(uint32_t now); // 调度器每 500ms 调用
    uint16_t raw() const { return avg; } // 均值，供串口标定用

private:
    uint16_t avg = 0;        // 8 次滑动均值
    bool dark = false;       // 迟滞当前态：是否处于"暗"（两阈值之间维持原态）
    bool sleepFired = false; // 本轮暗期是否已发过入睡事件
    uint32_t darkSince = 0;  // 本轮变暗的起始时刻
};
