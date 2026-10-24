/*
 * Ollie — ESP32-S3 桌面电子宠物 v1（离线版）
 *
 * 入口 + 毫秒级调度器（全部非阻塞）：
 *   33ms   渲染一帧 + 蜂鸣器/心情灯推进
 *   20ms   按键扫描（含 30ms 消抖）
 *   500ms  光敏采样 + 昼夜判定
 *   60s    数值结算 + NVS 存档 + 刷新表情/灯（DEBUG_FAST_DECAY 时 1s）
 *
 * 引脚/阈值/节奏统一在 Config.h，模块说明见各 .h 头部注释，
 * 整体设计见 docs/plan-v1.md
 */
#include <Wire.h>
#include "Config.h"
#include "Stats.h"
#include "Buttons.h"
#include "LightSensor.h"
#include "Buzzer.h"
#include "Face.h"
#include "MoodLight.h"
#include "PetState.h"

Stats       stats;
Face        face;
Buzzer      buzzer;
Buttons     buttons;
LightSensor lightSensor;
MoodLight   moodLight;
PetState    pet;

void setup() {
    Serial.begin(115200);
    delay(200);
    LOGF("\n=== Ollie v1 ===\n");
#ifdef DEBUG_FAST_DECAY
    LOGF("[warn] DEBUG_FAST_DECAY 开启：数值衰减为 1点/秒（仅调试）\n");
#endif

    uint32_t now = millis();
    buzzer.begin();
    buttons.begin(now);
    stats.begin();
    moodLight.begin(now);
    lightSensor.begin(now);
    if (!face.begin()) {
        LOGF("[error] OLED 初始化失败：检查接线/地址(0x3C)，3 秒后继续运行\n");
        delay(3000);
    }
    pet.begin(&stats, &face, &buzzer, &moodLight, now);

    const StatsData& d = stats.data();
    LOGF("[boot] 饱食=%u 心情=%u 精力=%u\n", d.food, d.mood, d.energy);
}

void loop() {
    uint32_t now = millis();
    static uint32_t tFrame = 0, tBtn = 0, tLight = 0, tSettle = 0;

    if (now - tFrame >= FRAME_MS) { // ~30fps
        tFrame = now;
        face.update(now);
        buzzer.update(now);
        moodLight.update(now);
    }

    if (now - tBtn >= 20) {
        tBtn = now;
        ButtonEvent ev = buttons.update(now);
        if (ev != ButtonEvent::None) {
            LOGF("[btn] %s\n", ev == ButtonEvent::Feed ? "喂食" : "抚摸");
            pet.onButton(ev, now);
        }
    }

    if (now - tLight >= 500) {
        tLight = now;
        LightEvent ev = lightSensor.update(now);
        if (ev == LightEvent::SleepTime)
            LOGF("[light] ADC=%u 持续变暗 → 入睡\n", lightSensor.raw());
        else if (ev == LightEvent::WakeTime)
            LOGF("[light] ADC=%u 变亮 → 醒来\n", lightSensor.raw());
        if (ev != LightEvent::None) pet.onLight(ev, now);
    }

    if (now - tSettle >= SETTLE_MS) {
        tSettle = now;
        pet.onSettle(now);
        const StatsData& d = stats.data();
        LOGF("[stats] 饱食=%u 心情=%u 精力=%u %s\n",
             d.food, d.mood, d.energy, pet.sleeping() ? "睡眠中" : "醒着");
    }
}
