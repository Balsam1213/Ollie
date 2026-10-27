#pragma once
/*
 * 光敏模块：支持两种硬件（Config.h 的 LIGHT_USE_DO 切换）
 *   AO 模式（4 针模块）：模拟分压 → 滑动均值 + 双阈值硬件迟滞
 *   DO 模式（3 针 LM393）：板载比较器输出数字电平，单一阈值无硬件迟滞
 *     → 软件迟滞补偿：连续两次一致才采纳 + 变暗 10s 确认 + 变亮 2s 确认
 * 两种模式共用同一状态机；同一引脚，AO/DO 按模块实际接其一。
 */
#include <Arduino.h>
#include "Config.h"

enum class LightEvent : uint8_t { None, SleepTime, WakeTime };

class LightSensor {
public:
    void begin(bool digitalMode, uint32_t now);
    LightEvent update(uint32_t now); // 调度器每 500ms 调用
    uint16_t raw() const { return reading; } // AO: 均值(0-4095)；DO: 电平 0/1，供标定

private:
    bool digital = false;
    uint16_t reading = 0;   // 最近一次采纳的采样值
    int prevLevel = -1;     // DO 模式：上一次原始电平（连续一致滤波用）
    bool dark = false;      // 当前迟滞判定态：是否处于"暗"
    bool sleepFired = false; // 本轮暗期是否已发过入睡事件
    uint32_t darkSince = 0;  // 进入暗态的时刻
    uint32_t brightSince = 0; // DO 模式：变亮确认计时起点
};
