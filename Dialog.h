#pragma once
/*
 * 台词驱动器（v1.2）：决定"什么时候说哪句"，交给 Face 整屏显示。
 *   事件台词（喂食/抚摸/入睡/醒来）——必说，绕过最小间隔
 *   阈值跨越（饱食<30 / 精力<30 / 心情<40）——每次跨越说一次
 *   闲聊——醒着时空闲 90-240s 随机一句，台词池跟随当前主导情绪
 *   冷落——30 分钟无互动撒娇一次（互动后重置）
 * 防刷屏：任意两句间隔 ≥ DIALOG_MIN_GAP_MS（事件台词除外）。
 */
#include <Arduino.h>
#include "Config.h"
#include "Stats.h"
#include "Face.h"

class Dialog {
public:
    void begin(Face* face, uint32_t now);
    void onFeed(uint32_t now);   // 内含 markInteraction
    void onPet(uint32_t now);    // 同上
    void onSleep(uint32_t now);
    void onWake(uint32_t now);
    // 每轮结算后调用（60s 节拍）：d=结算后数值，prev=结算前数值；同时驱动闲聊/冷落检查
    void onSettle(uint32_t now, const StatsData& d, const StatsData& prev, bool asleep);
    void markInteraction(uint32_t now); // 任何互动（喂/摸）重置冷落计时

    // 纯逻辑：从 count 句里挑一句，保证与 last 不同（count≤1 时返回 0）
    static uint8_t pickIndex(uint8_t count, uint8_t last);

private:
    void say(uint8_t scene, uint32_t now, bool force);

    Face* face_ = nullptr;
    uint32_t lastDialogAt_ = 0;   // 最近一次任意台词
    uint32_t nextChatterAt_ = 0;  // 下次闲聊时刻
    uint32_t lastInteractionAt_ = 0;
    bool idleSaid_ = false;       // 本轮冷落台词是否已说过
    uint8_t lastIdx_[9] = {0};    // 每个场景上次选的句序（对应 DialogScene 枚举）
};
