#pragma once
/*
 * 两路轻触开关（喂食/抚摸），INPUT_PULLUP + 消抖
 */
#include <Arduino.h>
#include "Config.h"

enum class ButtonEvent : uint8_t { None, Feed, Pet };

class DebouncedButton {
public:
    void begin(uint8_t pin, uint32_t now);
    bool fell(uint32_t now); // 检测到一次新的按下返回 true（下降沿）

private:
    uint8_t pin = 0;
    int stable = HIGH; // 消抖后的稳定电平
    int raw = HIGH;    // 最近一次原始读数
    uint32_t lastChange = 0;
};

class Buttons {
public:
    void begin(uint32_t now);
    ButtonEvent update(uint32_t now); // 调度器每 20ms 调用

private:
    DebouncedButton feed, pet;
};
