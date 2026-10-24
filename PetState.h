#pragma once
/*
 * 状态机（大脑）：醒/睡切换 + 表情优先级 + 把事件分发给数值/脸/声/灯
 *
 * 表情优先级（高→低）：
 *   临时开心 3s（Face 的 override 机制实现，压过一切）
 *   > 睡眠（Sleeping）
 *   > 饥饿（饱食 < 30）
 *   > 困倦（精力 < 30）
 *   > 平静（默认）
 */
#include <Arduino.h>
#include "Stats.h"
#include "Face.h"
#include "Buzzer.h"
#include "MoodLight.h"
#include "Buttons.h"
#include "LightSensor.h"

class PetState {
public:
    void begin(Stats* s, Face* f, Buzzer* b, MoodLight* m, uint32_t now);
    void onButton(ButtonEvent ev, uint32_t now); // 按键事件（睡眠中忽略）
    void onLight(LightEvent ev, uint32_t now);   // 昼夜事件：睡/醒切换
    void onSettle(uint32_t now);                 // 每分钟数值结算后刷新
    bool sleeping() const { return asleep; }

private:
    void refresh(uint32_t now); // 按数值/状态刷新常驻表情与心情灯

    bool asleep = false;
    Stats* stats = nullptr;
    Face* face = nullptr;
    Buzzer* buzzer = nullptr;
    MoodLight* light = nullptr;
};
