#pragma once
/*
 * 脸部绘制与动画：五种表情 + 自动眨眼 + 眼珠乱瞟 + Zzz + 防灼伤漂移
 * 对外接口：
 *   setBase / setExpression —— 常驻/临时表情（v2 聊天情绪也在用）
 *   showDialog(text, holdMs) —— 台词整屏显示（v1.2），排在当前覆盖之后播放
 * 内部是"最多 2 项的覆盖队列"（表情/台词两种），队列播完回到常驻表情。
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DialogFont.h"

enum class Emotion : uint8_t { Calm, Happy, Hungry, Sleepy, Sleeping };

class Face {
public:
    bool begin();                                  // I2C 初始化 + 点屏 + 开机画面
    void setBase(Emotion e);                       // 常驻表情（PetState 每轮刷新）
    void setExpression(Emotion e, uint32_t holdMs); // 临时表情
    void showDialog(const char* text, uint32_t holdMs); // 台词：排到当前覆盖之后
    void update(uint32_t now);                     // 调度器每帧调用，内部限帧

private:
    // —— 覆盖队列 ——
    enum class OvKind : uint8_t { Expression, Dialog };
    struct Overlay {
        OvKind kind = OvKind::Expression;
        Emotion emotion = Emotion::Calm;
        const char* text = nullptr; // 台词池常量，生命周期等同程序
        uint32_t startAt = 0;
        uint32_t dur = 0;
    };
    void pushOverlay(const Overlay& o, uint32_t now);
    void expireOverlays(uint32_t now);
    const Overlay* activeOverlay(uint32_t now) const;

    // —— 动画/渲染 ——
    void advanceAnim(uint32_t now, Emotion e);
    float blinkFactor(uint32_t now); // 1=睁眼 ~0.05=闭眼；结束时会排下一次眨眼
    void render(uint32_t now, Emotion e, const Overlay* ov);
    void renderDialog(const char* text); // 台词整屏居中

    // —— 图元 ——
    void drawEyeOpen(int cx, int cy, float hScale, int pupDx); // 圆角矩形眼 + 黑眼珠
    void drawEyeHappyArc(int cx, int cy);                      // ^ 眯眼
    void drawEyeClosed(int cx, int cy);                        // 下弯闭眼线
    void drawArcMouth(int cx, int cy, int r, bool smile);      // 弧线嘴（2px 粗）
    void drawBrow(int cx, int cy, bool mirror);                // 饥饿时眉角下压
    void drawZzz(uint32_t now);                                // 睡眠 Zzz 上浮

    Adafruit_SSD1306 display{128, 64, &Wire, -1};

    Emotion base = Emotion::Calm;
    Overlay q_[2];       // 覆盖队列：按 startAt 顺序播放
    uint8_t qN_ = 0;

    // 自动眨眼（每 2-6s 随机，闭合约 180ms）
    bool blinking = false;
    uint32_t blinkStart = 0, nextBlink = 0;
    // 眼珠乱瞟（平静时每 3-8s 随机偏移 ±6px，持续 0.5-1s 回中）
    int pupDx = 0;
    uint32_t dartUntil = 0, nextDart = 0;
    // 防 OLED 灼伤：整脸每 30s 随机漂移 1px
    int driftX = 0, driftY = 0;
    uint32_t nextDrift = 0;

    bool dimmed = false;      // 睡眠时调暗屏幕
    uint32_t lastFrame = 0;
};
