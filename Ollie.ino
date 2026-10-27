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
#include <WiFi.h>
#include "Config.h"
#include "Stats.h"
#include "Buttons.h"
#include "LightSensor.h"
#include "Buzzer.h"
#include "Face.h"
#include "MoodLight.h"
#include "PetState.h"
#include "NetConfig.h"
#include "LLMClient.h"
#include "WebChat.h"

Stats       stats;
Face        face;
Buzzer      buzzer;
Buttons     buttons;
LightSensor lightSensor;
MoodLight   moodLight;
PetState    pet;
NetConfig   net;   // v2: WiFi/凭据
LLMClient   llm;   // v2: LLM 调用（独立任务）
WebChat     web;   // v2: 网页聊天
bool        netWasConnected = false;

void setup() {
    Serial.begin(115200);
    delay(200);
    LOGF("\n=== Ollie %s ===\n", OLLIE_VERSION);
#ifdef DEBUG_FAST_DECAY
    LOGF("[warn] DEBUG_FAST_DECAY 开启：数值衰减为 1点/秒（仅调试）\n");
#endif

#if BOOT_I2C_SCAN
    // 只跑 I2C 扫描（Config.h 打开），用于 M1 排查；OLED 应出现 0x3C
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    LOGF("[scan] I2C 总线扫描开始\n");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) LOGF("[scan] 发现设备: 0x%02X\n", addr);
    }
    LOGF("[scan] 完成，停在此模式\n");
    while (true) delay(1000);
#endif

    uint32_t now = millis();
    buzzer.begin();
    buttons.begin(now);
    stats.begin();
    moodLight.begin(now);
    lightSensor.begin(LIGHT_USE_DO != 0, now);
    if (!face.begin()) {
        LOGF("[error] OLED 初始化失败：检查接线/地址(0x3C)，3 秒后继续运行\n");
        delay(3000);
    }
    pet.begin(&stats, &face, &buzzer, &moodLight, now);

    net.begin();               // v2: 读凭据，没配过网则开配置热点
    llm.begin(&face);          // v2: 聊天任务
    web.begin(&llm, &net, &stats);
    LOGF("[net] 状态：%s\n", net.hasCreds() ? "有凭据，尝试连接" : "未配置，等待配网");

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
        llm.poll(now); // v2: 聊天结果 → 表情
    }

    if (now - tBtn >= 20) {
        tBtn = now;
        web.update(now); // v2: 网页服务（含 handleClient）
        ButtonEvent ev = buttons.update(now);
        if (ev != ButtonEvent::None) {
            LOGF("[btn] %s\n", ev == ButtonEvent::Feed ? "喂食" : "抚摸");
            pet.onButton(ev, now);
        }
    }

    if (now - tLight >= 500) {
        tLight = now;
        net.update(now); // v2: WiFi 状态机
        // 联网状态变化编排：起停网页服务 + 断网蓝闪提示
        bool c = net.connected();
        if (c != netWasConnected) {
            netWasConnected = c;
            if (c) {
                web.start();
                LOGF("[net] 网页聊天就绪：http://ollie.local （或 %s）\n",
                     WiFi.localIP().toString().c_str());
            } else {
                web.stop();
                moodLight.netLostFlash(now);
                LOGF("[net] 离线模式：Ollie 回到纯离线玩法\n");
            }
        }
#if LIGHT_CALIBRATE
#if LIGHT_USE_DO
        // DO 模块标定：遮光/照亮各看几次，"暗"时打印的电平填进 LIGHT_DO_DARK_LEVEL
        LOGF("[light-cal] DO=%u（暗电平应为 %u）\n",
             lightSensor.raw(), (unsigned)LIGHT_DO_DARK_LEVEL);
#else
        // AO 模块标定：对照串口值改 Config.h 的两个 ADC 阈值
        LOGF("[light-cal] ADC=%u（暗<%u 亮>%u）\n",
             lightSensor.raw(), LIGHT_DARK_ADC, LIGHT_BRIGHT_ADC);
#endif
#endif
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
        // up/heap 是 M7 过夜长测的观测点：heap 持续下降说明有泄漏
        LOGF("[stats] 饱食=%u 心情=%u 精力=%u %s | up=%lus heap=%lu\n",
             d.food, d.mood, d.energy, pet.sleeping() ? "睡眠中" : "醒着",
             (unsigned long)(now / 1000), (unsigned long)ESP.getFreeHeap());
    }
}
