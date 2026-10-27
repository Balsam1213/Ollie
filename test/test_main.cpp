/*
 * Ollie 逻辑单测 —— 跑在 PC 上，不依赖硬件
 * 覆盖：Stats 衰减/睡觉回精力/加成封顶/断电存档、Buttons 消抖、
 *       LightSensor 迟滞与 10 秒确认、PetState 状态机与表情优先级、
 *       Buzzer 两声时序、MoodLight 颜色映射与亮度封顶
 *
 * 运行：bash test/run_tests.sh（默认 zig，CXX 环境变量可换任意 C++17 编译器）
 * 注意：模块真实实现参与编译；只有 Face 用桩（见 Face_stub.cpp），因其依赖 OLED
 */
#include "Arduino.h" // ← test/mocks 优先，命中的是仿真桩
#include "Config.h"
#include "Stats.h"
#include "Buttons.h"
#include "LightSensor.h"
#include "Buzzer.h"
#include "Face.h"
#include "MoodLight.h"
#include "PetState.h"
#include <cstdio>

// ---- 桩的全局定义 ----
uint32_t g_ms = 0;
int g_pinLevel[64] = {0};
uint16_t g_adc[16] = {0};
uint32_t g_lastTone = 0xFFFFFFFF; // 哨兵：尚未发声
uint32_t g_rgb[3] = {999, 999, 999};
MockSerial Serial;

// Face 桩的观测变量（定义在 Face_stub.cpp）
extern Emotion g_lastBase;
extern Emotion g_lastOverride;
extern uint32_t g_lastHold;
extern int g_baseCalls;
extern int g_overrideCalls;

static int failures = 0, checks = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                    \
    } while (0)

static void resetTime() { g_ms = 0; }
static void idlePins() { // INPUT_PULLUP 空闲 = 高电平
    for (int i = 0; i < 64; i++) g_pinLevel[i] = HIGH;
}
static void setLight(uint16_t v) { g_adc[PIN_LIGHT_AO] = v; }
#define setFeed(v) (g_pinLevel[PIN_BTN_FEED] = (v))
#define setPet(v) (g_pinLevel[PIN_BTN_PET] = (v))

// ============ 1. Stats ============
static void test_stats_decay_and_clamp() {
    std::printf("[test] Stats 衰减 / 睡觉回精力 / 加成封顶\n");
    Stats s;
    s.begin(); // 本进程最先跑，NVS 桩为空 → 首次上电默认值
    CHECK(s.data().food == 80 && s.data().mood == 80 && s.data().energy == 80);

    s.tick(false);
    s.tick(false);
    s.tick(false);
    CHECK(s.data().food == 77 && s.data().mood == 77 && s.data().energy == 77);

    s.tick(true);
    s.tick(true); // 睡觉：精力 +2/轮，饱食/心情停
    CHECK(s.data().food == 77 && s.data().mood == 77 && s.data().energy == 81);

    s.onFeed(); // +20 / +4（睡觉只回了精力，心情仍是 77）
    CHECK(s.data().food == 97 && s.data().mood == 81);
    s.onFeed(); // 饱食封顶
    CHECK(s.data().food == 100 && s.data().mood == 85);
    s.onPet();
    s.onPet();
    s.onPet(); // 心情封顶
    CHECK(s.data().mood == 100);

    for (int i = 0; i < 200; i++) s.tick(false); // 全部衰减到底（到 0 不为负）
    CHECK(s.data().food == 0 && s.data().mood == 0 && s.data().energy == 0);
    s.onFeed();
    CHECK(s.data().food == 20 && s.data().mood == 4); // 底部加成照常
}

static void test_stats_persistence() {
    std::printf("[test] Stats 断电重启存档（NVS 桩，同进程新实例=重启）\n");
    Stats a;
    a.begin();
    a.onPet();     // 互动立即生效，但落盘发生在下一次结算（当前设计）
    a.tick(false); // 结算 + 存档
    StatsData before = a.data();

    Stats b; // 模拟断电重启后新建
    b.begin();
    CHECK(b.data().food == before.food);
    CHECK(b.data().mood == before.mood);
    CHECK(b.data().energy == before.energy);
}

// ============ 2. Buttons ============
static void test_buttons_debounce() {
    std::printf("[test] Buttons 消抖（抖动只出一个事件，毛刺不触发）\n");
    idlePins();
    resetTime();
    Buttons btns;
    btns.begin(0);

    // —— 带抖动的按下：LOW→HIGH→LOW，最终稳定 LOW ——
    setFeed(LOW);
    CHECK(btns.update(100) == ButtonEvent::None);
    setFeed(HIGH);
    CHECK(btns.update(105) == ButtonEvent::None); // 抖回
    setFeed(LOW);
    CHECK(btns.update(110) == ButtonEvent::None); // 再抖下，lastChange=110
    CHECK(btns.update(115) == ButtonEvent::None);
    CHECK(btns.update(120) == ButtonEvent::None);
    CHECK(btns.update(125) == ButtonEvent::None);
    CHECK(btns.update(130) == ButtonEvent::None);
    CHECK(btns.update(135) == ButtonEvent::None);
    CHECK(btns.update(140) == ButtonEvent::Feed); // 140-110=30 ≥ 消抖窗 → 恰好一次
    CHECK(btns.update(145) == ButtonEvent::None); // 持续按住不再触发
    CHECK(btns.update(200) == ButtonEvent::None);

    // —— 松开 ——
    setFeed(HIGH);
    CHECK(btns.update(210) == ButtonEvent::None);
    CHECK(btns.update(240) == ButtonEvent::None); // 采纳为高，无事件

    // —— 小于消抖窗的毛刺：完全不触发 ——
    setFeed(LOW);
    CHECK(btns.update(300) == ButtonEvent::None);
    setFeed(HIGH);
    CHECK(btns.update(310) == ButtonEvent::None);
    CHECK(btns.update(500) == ButtonEvent::None);

    // —— 抚摸键：一次干净的按压 ——
    setPet(LOW);
    CHECK(btns.update(600) == ButtonEvent::None);
    CHECK(btns.update(640) == ButtonEvent::Pet);
    setPet(HIGH);
    CHECK(btns.update(700) == ButtonEvent::None);
    CHECK(btns.update(740) == ButtonEvent::None);
}

// ============ 3. LightSensor ============
static void test_light_sensor() {
    std::printf("[test] LightSensor AO 模式：迟滞与持续变暗 10 秒确认\n");
    resetTime();
    setLight(3000);
    LightSensor ls;
    ls.begin(false, 0); // AO 模式，begin 时播种均值
    CHECK(ls.update(500) == LightEvent::None); // 亮，无事

    // —— 全黑：均值滑落 → 判暗 → 满 10 秒才入睡 ——
    setLight(0);
    LightEvent ev = LightEvent::None;
    uint32_t t = 1000;
    while (t < 6000) { // 均值下降期间不应有任何事件
        ev = ls.update(t);
        CHECK(ev == LightEvent::None);
        t += 500;
    }
    ev = LightEvent::None; // 判暗后 10 秒确认
    while (t < 17000 && ev == LightEvent::None) {
        ev = ls.update(t);
        t += 500;
    }
    CHECK(ev == LightEvent::SleepTime);
    CHECK(ls.update(t) == LightEvent::None); // 同一轮暗期只发一次

    // —— 迟滞：中间亮度（800~2000 之间）维持"暗"态，不唤醒 ——
    setLight(1500);
    ev = LightEvent::None;
    t += 500;
    for (int i = 0; i < 16; i++) { // 8 秒
        ev = ls.update(t);
        CHECK(ev == LightEvent::None);
        t += 500;
    }

    // —— 变亮 → 醒来 ——
    setLight(4095);
    ev = LightEvent::None;
    while (t < 40000 && ev == LightEvent::None) {
        ev = ls.update(t);
        t += 500;
    }
    CHECK(ev == LightEvent::WakeTime);

    // —— 短暂阴影（没滑到暗阈值就回亮）：不应产生任何事件 ——
    setLight(0);
    CHECK(ls.update(t) == LightEvent::None);
    CHECK(ls.update(t + 500) == LightEvent::None);
    CHECK(ls.update(t + 1000) == LightEvent::None);
    CHECK(ls.update(t + 1500) == LightEvent::None);
    setLight(4095);
    CHECK(ls.update(t + 2000) == LightEvent::None);
    CHECK(ls.update(t + 2500) == LightEvent::None);
}

static void test_light_sensor_digital() {
    std::printf("[test] LightSensor DO 模式（LM393 三针）：极性映射 / 变亮确认 / 抖动过滤\n");
    resetTime();
    // 极性按 Config.h 的 LIGHT_DO_DARK_LEVEL = 1：HIGH = 暗，LOW = 亮
    g_pinLevel[PIN_LIGHT_AO] = HIGH; // 开机即处于暗态
    LightSensor ls;
    ls.begin(true, 0);

    // —— 暗态持续 10s → SleepTime（一次） ——
    LightEvent ev = LightEvent::None;
    uint32_t t = 500;
    while (t <= 12000 && ev == LightEvent::None) {
        ev = ls.update(t);
        t += 500;
    }
    CHECK(ev == LightEvent::SleepTime);
    CHECK(t - 500 <= 11000); // 在确认期刚满时触发
    CHECK(ls.update(t) == LightEvent::None); // 同一轮暗期不重复发

    // —— 临界抖动（HIGH/LOW 交替）：连续一致滤波挡住，无任何事件 ——
    for (int i = 0; i < 10; i++) {
        g_pinLevel[PIN_LIGHT_AO] = (i % 2 == 0) ? LOW : HIGH;
        CHECK(ls.update(t) == LightEvent::None);
        t += 500;
    }

    // —— 变亮：需持续 WAKE_CONFIRM_S(2s) 才醒 ——
    g_pinLevel[PIN_LIGHT_AO] = LOW;
    uint32_t tAdopt = 0;
    ev = LightEvent::None;
    while (t < 60000 && ev == LightEvent::None) {
        ev = ls.update(t);
        if (ev == LightEvent::None && tAdopt == 0 && ls.raw() == (uint16_t)LOW)
            tAdopt = t; // 首次采纳亮电平
        t += 500;
    }
    CHECK(ev == LightEvent::WakeTime);
    CHECK(t - 500 - tAdopt >= (uint32_t)WAKE_CONFIRM_S * 1000ul); // 确认期已满才醒
    CHECK(ls.raw() == (uint16_t)LOW);

    // —— 唤醒后再变暗：10s 后应能再次入睡（sleepFired 已在唤醒时复位） ——
    g_pinLevel[PIN_LIGHT_AO] = HIGH;
    ev = LightEvent::None;
    while (t < 30000 && ev == LightEvent::None) { ev = ls.update(t); t += 500; }
    CHECK(ev == LightEvent::SleepTime); // 复位生效，昼夜循环可反复进行
    t += 500;
    CHECK(ls.update(t) == LightEvent::None);

    g_pinLevel[PIN_LIGHT_AO] = LOW;           // 变亮，先确认 1 秒
    CHECK(ls.update(t) == LightEvent::None);  // 电平切换拍：只记录不采纳
    t += 500;
    CHECK(ls.update(t) == LightEvent::None);  // 采纳亮，确认起点
    t += 500;
    CHECK(ls.raw() == (uint16_t)LOW);
    CHECK(ls.update(t) == LightEvent::None);  // 确认 0.5s
    t += 500;
    CHECK(ls.update(t) == LightEvent::None);  // 确认 1.0s（不足 2s，没醒）
    t += 500;

    g_pinLevel[PIN_LIGHT_AO] = HIGH;          // 确认中途抖回暗
    CHECK(ls.update(t) == LightEvent::None);  // 电平切换拍：不采纳、不清计时
    t += 500;
    CHECK(ls.update(t) == LightEvent::None);  // 采纳暗：亮确认计时清零，未醒
    t += 500;
    CHECK(ls.raw() == (uint16_t)HIGH);

    g_pinLevel[PIN_LIGHT_AO] = LOW;           // 再持续亮 2s
    ev = LightEvent::None;
    while (t < 150000 && ev == LightEvent::None) {
        ev = ls.update(t);
        t += 500;
    }
    CHECK(ev == LightEvent::WakeTime);        // 抖动后依然能正常醒来
}

// ============ 4. PetState ============
static void test_petstate() {
    std::printf("[test] PetState 状态机 / 表情优先级 / 睡觉回精力 / 睡中按键无效\n");
    resetTime();
    idlePins();
    setLight(3000);

    Stats stats;
    stats.begin();
    // 前面测试可能留下低数值存档；拉回健康区间再测
    while (stats.data().food < 60) stats.onFeed();
    while (stats.data().energy < 60) stats.tick(true);
    CHECK(stats.data().food >= 30 && stats.data().energy >= 30);

    Face face;
    Buzzer buzzer;
    MoodLight light;
    PetState pet;
    buzzer.begin();
    pet.begin(&stats, &face, &buzzer, &light, 0);
    CHECK(g_lastBase == Emotion::Calm); // 数值健康 → 平静

    // —— 入睡 ——
    pet.onLight(LightEvent::SleepTime, 0);
    CHECK(pet.sleeping());
    CHECK(g_lastBase == Emotion::Sleeping);
    StatsData before = stats.data();
    pet.onSettle(60000); // 睡觉结算
    CHECK(stats.data().energy == before.energy + ENERGY_GAIN_SLEEP);
    CHECK(stats.data().food == before.food);
    CHECK(stats.data().mood == before.mood);

    // —— 睡着时按键无效（默认关彩蛋）——
    int oc = g_overrideCalls;
    before = stats.data();
    pet.onButton(ButtonEvent::Feed, 61000);
    pet.onButton(ButtonEvent::Pet, 61500);
    CHECK(g_overrideCalls == oc);
    CHECK(stats.data().food == before.food && stats.data().mood == before.mood);

    // —— 醒来 → 平静 ——
    pet.onLight(LightEvent::WakeTime, 62000);
    CHECK(!pet.sleeping());
    CHECK(g_lastBase == Emotion::Calm);

    // —— 喂食：数值 + 临时开心 3 秒 + 两声哔哔 ——
    before = stats.data();
    g_lastTone = 0xFFFFFFFF;
    pet.onButton(ButtonEvent::Feed, 63000);
    CHECK(stats.data().food == before.food + FEED_FOOD);
    CHECK(stats.data().mood == before.mood + FEED_MOOD);
    CHECK(g_lastOverride == Emotion::Happy);
    CHECK(g_lastHold == HAPPY_HOLD_MS);
    buzzer.update(63000);
    CHECK(g_lastTone == BEEP_FREQ); // 第一声
    buzzer.update(63000 + BEEP_MS + 10);
    CHECK(g_lastTone == 0); // 间隙
    buzzer.update(63000 + 2 * BEEP_MS + 10);
    CHECK(g_lastTone == BEEP_FREQ); // 第二声
    buzzer.update(63000 + 3 * BEEP_MS + 10);
    CHECK(g_lastTone == 0); // 结束
    buzzer.update(70000);
    CHECK(g_lastTone == 0); // 空闲时不再驱动

    // —— 优先级：又饿又困 → 饥饿优先 ——
    int guard = 0;
    while (!(stats.data().food < HUNGRY_BELOW && stats.data().energy < SLEEPY_BELOW)) {
        pet.onSettle(71000 + guard * 1000); // 醒着，三项齐掉（顺带刷新表情）
        if (++guard > 150) break;
    }
    CHECK(stats.data().food < HUNGRY_BELOW);
    CHECK(stats.data().energy < SLEEPY_BELOW);
    CHECK(g_lastBase == Emotion::Hungry);

    // —— 补饱后：只剩困 → 困倦脸（food≥30 不再是饥饿）——
    while (stats.data().food < 50) stats.onFeed();
    pet.onSettle(300000); // 触发 refresh
    CHECK(stats.data().energy < SLEEPY_BELOW);
    CHECK(g_lastBase == Emotion::Sleepy);

    // —— 事件幂等：醒着再来 Wake / 睡着再来 Sleep 都不炸 ——
    pet.onLight(LightEvent::WakeTime, 310000);
    CHECK(!pet.sleeping());
    pet.onLight(LightEvent::SleepTime, 320000);
    CHECK(pet.sleeping());
}

// ============ 5. MoodLight ============
static void test_moodlight() {
    std::printf("[test] MoodLight 颜色映射 / 亮度封顶 / 渐变\n");
    resetTime();
    MoodLight ml;
    ml.begin(0);
    // 连跑 ~32 帧让 1/4 步进的渐变完全收敛（每帧 33ms）
    auto settle = [&](uint32_t t0) {
        for (int i = 0; i < 32; i++) ml.update(t0 + i * 33);
    };

    ml.setMood(80, false); // 心情好 → 青绿
    settle(1000);
    CHECK(g_rgb[0] == 0);
    CHECK(g_rgb[1] > 0 && g_rgb[2] > 0);
    CHECK(g_rgb[1] <= LED_MAX && g_rgb[2] <= LED_MAX);

    ml.setMood(50, false); // 中档 → 琥珀黄
    settle(5000);
    CHECK(g_rgb[0] > 0 && g_rgb[1] > 0 && g_rgb[2] == 0);

    ml.setMood(20, false); // 低档 → 橙红（t=9000 落在"亮"段）
    settle(9000);
    CHECK(g_rgb[0] > 0 && g_rgb[2] == 0);

    ml.setMood(50, true); // 睡眠 → 深蓝呼吸
    settle(13000);
    CHECK(g_rgb[0] == 0 && g_rgb[1] == 0);
    CHECK(g_rgb[2] > 0 && g_rgb[2] <= LED_MAX);

    ml.happyFlash(21000); // 互动 → 粉色（R 满格=封顶值）
    settle(21000);
    CHECK(g_rgb[0] == LED_MAX);
}

int main() {
    std::printf("=== Ollie 逻辑单测 ===\n");
    test_stats_decay_and_clamp();
    test_stats_persistence();
    test_buttons_debounce();
    test_light_sensor();
    test_light_sensor_digital();
    test_petstate();
    test_moodlight();
    std::printf("\n共 %d 项断言，失败 %d → %s\n", checks, failures, failures ? "FAILED" : "ALL PASS");
    return failures ? 1 : 0;
}
