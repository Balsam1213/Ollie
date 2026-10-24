#pragma once
/*
 * 无源蜂鸣器：两声"哔哔"（2kHz 方波，80ms-80ms-80ms），非阻塞
 */
#include <Arduino.h>
#include "Config.h"

class Buzzer {
public:
    void begin();                // LEDC 绑定引脚（arduino-esp32 3.x API）
    void twoBeeps(uint32_t now); // 触发音效
    void update(uint32_t now);   // 调度器每帧调用，推进音效时序

private:
    bool playing = false;
    uint32_t start = 0;
};
