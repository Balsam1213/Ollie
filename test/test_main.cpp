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
#include "ChatLogic.h"
#include "Dialog.h"
#include "DialogLines.h"
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
extern const char* g_lastDialog;
extern int g_dialogCalls;

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

// 台词辅助：码点数 / 判断桩记录的台词指针是否属于某台词池
static int cpCount(const char* s) {
    int bytes = 0, cont = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        bytes++;
        if ((*p & 0xC0) == 0x80) cont++;
    }
    return bytes - cont;
}
static bool inPool(const char* line, const char* const* pool, int n) {
    for (int i = 0; i < n; i++)
        if (strcmp(line, pool[i]) == 0) return true; // 内容比较（字面量跨编译单元不保证同址）
    return false;
}

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
    while (t < 40000 && ev == LightEvent::None) { // 均值滑到暗阈值约需 20 拍，之后 10s 确认
        ev = ls.update(t);
        CHECK(ev != LightEvent::WakeTime); // 下滑途中不该误报唤醒
        t += 500;
    }
    CHECK(ev == LightEvent::SleepTime);
    CHECK(ls.update(t) == LightEvent::None); // 同一轮暗期只发一次

    // —— 迟滞：暗/亮阈值之间的中间亮度维持"暗"态，不唤醒 ——
    setLight((LIGHT_DARK_ADC + LIGHT_BRIGHT_ADC) / 2);
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
    g_dialogCalls = 0;
    pet.onLight(LightEvent::SleepTime, 0);
    CHECK(pet.sleeping());
    CHECK(g_lastBase == Emotion::Sleeping);
    CHECK(g_dialogCalls == 1); // 睡前一句晚安
    CHECK(inPool(g_lastDialog, LINES_SLEEP, N_SLEEP));
    StatsData before = stats.data();
    pet.onSettle(60000); // 睡觉结算
    CHECK(stats.data().energy == before.energy + ENERGY_GAIN_SLEEP);
    CHECK(stats.data().food == before.food);
    CHECK(stats.data().mood == before.mood);

    // —— 睡着时按键无效（默认关彩蛋）——
    int oc = g_overrideCalls;
    int dc = g_dialogCalls;
    before = stats.data();
    pet.onButton(ButtonEvent::Feed, 61000);
    pet.onButton(ButtonEvent::Pet, 61500);
    CHECK(g_overrideCalls == oc);
    CHECK(g_dialogCalls == dc);
    CHECK(stats.data().food == before.food && stats.data().mood == before.mood);

    // —— 醒来 → 平静 + 早安台词 ——
    g_dialogCalls = 0;
    pet.onLight(LightEvent::WakeTime, 62000);
    CHECK(!pet.sleeping());
    CHECK(g_lastBase == Emotion::Calm);
    CHECK(g_dialogCalls == 1);
    CHECK(inPool(g_lastDialog, LINES_WAKE, N_WAKE));

    // —— 喂食：数值 + 先笑后说 + 两声哔哔 ——
    before = stats.data();
    g_lastTone = 0xFFFFFFFF;
    g_dialogCalls = 0;
    pet.onButton(ButtonEvent::Feed, 63000);
    CHECK(stats.data().food == before.food + FEED_FOOD);
    CHECK(stats.data().mood == before.mood + FEED_MOOD);
    CHECK(g_lastOverride == Emotion::Happy);
    CHECK(g_lastHold == DIALOG_FEED_FACE_MS); // 先笑 1.4s，台词随后
    CHECK(g_dialogCalls == 1);
    CHECK(inPool(g_lastDialog, LINES_FEED, N_FEED));
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

// ============ 6. ChatLogic（v2.0 纯逻辑） ============
static void test_chatlogic() {
    using namespace chat;
    std::printf("[test] ChatLogic：JSON 转义 / content 提取 / 回复解析 / 情绪映射 / prompt\n");

    // —— jsonEscape ——
    CHECK(jsonEscape("a\"b\\c") == "a\\\"b\\\\c");
    CHECK(jsonEscape("行\n尾") == "行\\n尾");
    CHECK(jsonEscape("中文") == "中文"); // UTF-8 原样透传
    CHECK(jsonEscape(std::string(1, '\x01')) == "\\u0001"); // 控制字符

    // —— jsonUnescape（含 \u 编码与代理对） ——
    CHECK(jsonUnescape("a\\\"b\\\\c") == "a\"b\\c");
    CHECK(jsonUnescape("\\u4f60\\u597d") == "你好");
    CHECK(jsonUnescape("\\ud83d\\ude00") == "\xF0\x9F\x98\x80"); // 😀 代理对→4字节UTF-8
    CHECK(jsonUnescape(jsonEscape("往返\"测\\试\n")) == "往返\"测\\试\n"); // roundtrip

    // —— findStringValue：key 不会误命中字段值内部的 "key" 文本 ——
    std::string v;
    CHECK(findStringValue("{\"a\":\"有 \\\"content\\\" 字样\",\"content\":\"真身\"}", "content", v));
    CHECK(v == "真身");
    CHECK(findStringValue("{\"reply\":\"hi\"}", "reply", v) && v == "hi");
    CHECK(!findStringValue("{\"other\":1}", "reply", v));

    // —— extractContent：OpenAI 兼容响应 ——
    std::string api = "{\"id\":\"x\",\"choices\":[{\"message\":{\"role\":\"assistant\","
                      "\"content\":\"{\\\"reply\\\":\\\"你好呀\\\",\\\"emotion\\\":\\\"happy\\\"}\"}}]}";
    std::string content;
    CHECK(extractContent(api, content));
    CHECK(content == "{\"reply\":\"你好呀\",\"emotion\":\"happy\"}");

    // —— parseReply ——
    std::string reply; chat::Emotion emo = chat::Emotion::Calm;
    CHECK(parseReply(content, reply, emo));
    CHECK(reply == "你好呀");
    CHECK(emo == chat::Emotion::Happy);
    CHECK(parseReply("```json\n{\"reply\":\"嗯\",\"emotion\":\"sleepy\"}\n```", reply, emo));
    CHECK(reply == "嗯" && emo == chat::Emotion::Sleepy);
    CHECK(parseReply("我不太明白你的意思", reply, emo)); // 非 JSON 兜底：整段当回复
    CHECK(reply == "我不太明白你的意思" && emo == chat::Emotion::Calm);
    CHECK(!parseReply("", reply, emo));
    CHECK(parseReply("{\"emotion\":\"hungry\"}", reply, emo) == false); // 缺 reply

    // —— emotionFromName ——
    CHECK(emotionFromName("happy") == chat::Emotion::Happy);
    CHECK(emotionFromName("hungry") == chat::Emotion::Hungry);
    CHECK(emotionFromName("sleepy") == chat::Emotion::Sleepy);
    CHECK(emotionFromName("whatever") == chat::Emotion::Calm);

    // —— systemPrompt：人设 + 数值注入 + 低状态提示 ——
    std::string p = systemPrompt(20, 80, 10);
    CHECK(p.find("饱食度 20/100") != std::string::npos);
    CHECK(p.find("精力 10/100") != std::string::npos);
    CHECK(p.find("很饿") != std::string::npos);
    CHECK(p.find("很困") != std::string::npos);
    CHECK(p.find("Ollie") != std::string::npos);
    CHECK(systemPrompt(90, 90, 90).find("很饿") == std::string::npos);

    // —— buildRequestBody：模型名/转义后的消息都在，system 在前 ——
    std::string body = buildRequestBody("test-model", 50, 60, 70, "你\"好\"");
    CHECK(body.find("\"model\":\"test-model\"") != std::string::npos);
    CHECK(body.find("你\\\"好\\\"") != std::string::npos);
    CHECK(body.find("\"role\":\"system\"") < body.find("\"role\":\"user\""));
}

// ============ 7. Dialog（v1.2 台词） ============
static void test_dialog_lines() {
    std::printf("[test] 台词表合法性（逐句 ≤8 字非空）+ 真实字库查询 + 选句不重复\n");
    struct PoolRef {
        const char* const* lines;
        int n;
        const char* name;
    };
    PoolRef pools[] = {
        {LINES_CALM, N_CALM, "CALM"},       {LINES_HUNGRY, N_HUNGRY, "HUNGRY"},
        {LINES_SLEEPY, N_SLEEPY, "SLEEPY"}, {LINES_SAD, N_SAD, "SAD"},
        {LINES_SLEEP, N_SLEEP, "SLEEP"},    {LINES_WAKE, N_WAKE, "WAKE"},
        {LINES_FEED, N_FEED, "FEED"},       {LINES_PET, N_PET, "PET"},
        {LINES_IDLE, N_IDLE, "IDLE"},
    };
    for (auto& p : pools) {
        CHECK(p.n >= 1);
        for (int i = 0; i < p.n; i++) {
            int n = cpCount(p.lines[i]);
            CHECK(n >= 1 && n <= DIALOG_MAX_CHARS); // 屏宽约束：8 字 × 12px
        }
    }

    // —— 真实字库：台词字符可查、字形有墨 ——
    CHECK(dialogfont::findGlyph(0x4ECA) != nullptr);  // 今（今天也要加油鸭）
    CHECK(dialogfont::findGlyph(0x597D) != nullptr);  // 好
    const dialogfont::Glyph* g = dialogfont::findGlyph(0x4ECA);
    bool anyInk = false;
    for (int y = 0; y < 12 && !anyInk; y++)
        for (int x = 0; x < 12; x++)
            if (g->bitmap[y * 2 + x / 8] & (0x80 >> (x % 8))) anyInk = true;
    CHECK(anyInk);
    CHECK(dialogfont::findGlyph(0xE000) == nullptr); // 私有区字符必不在字库

    // —— pickIndex：永不连续重复 ——
    CHECK(Dialog::pickIndex(0, 0) == 0);
    CHECK(Dialog::pickIndex(1, 0) == 0);
    for (int i = 0; i < 60; i++) {
        uint8_t last = (uint8_t)(i % 3);
        CHECK(Dialog::pickIndex(3, last) != last);
    }
}

static void test_dialog_driver() {
    std::printf("[test] Dialog 驱动器：事件台词 / 阈值跨越 / 冷却防刷屏\n");
    resetTime();
    Face face;
    Dialog dlg;
    dlg.begin(&face, 0);

    // —— 事件台词必说，取自对应池 ——
    g_dialogCalls = 0;
    dlg.onFeed(1000);
    CHECK(g_dialogCalls == 1);
    CHECK(inPool(g_lastDialog, LINES_FEED, N_FEED));
    dlg.onPet(2000);
    CHECK(g_dialogCalls == 2);
    CHECK(inPool(g_lastDialog, LINES_PET, N_PET));
    dlg.onSleep(3000);
    CHECK(inPool(g_lastDialog, LINES_SLEEP, N_SLEEP));
    dlg.onWake(4000);
    CHECK(inPool(g_lastDialog, LINES_WAKE, N_WAKE));

    // —— 阈值跨越：饱食跌破 30 → 饥饿台词（60s < 最早闲聊 90s，无干扰） ——
    StatsData prev{31, 80, 80}, now{29, 80, 80};
    g_dialogCalls = 0;
    dlg.onSettle(60000, now, prev, false);
    CHECK(g_dialogCalls == 1);
    CHECK(inPool(g_lastDialog, LINES_HUNGRY, N_HUNGRY));

    // —— 未跨越 + 距上次台词仅 1s：不再说 ——
    StatsData now2{28, 80, 80};
    dlg.onSettle(61000, now2, now, false);
    CHECK(g_dialogCalls == 1);

    // —— 睡觉时不闲聊不撒娇（onSettle asleep=true 只报跨越） ——
    g_dialogCalls = 0;
    StatsData now3{28, 80, 90};
    dlg.onSettle(300000, now3, now2, true);
    CHECK(g_dialogCalls == 0); // 300s 处闲聊计时已过，但睡着必须安静
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
    test_chatlogic();
    test_dialog_lines();
    test_dialog_driver();
    std::printf("\n共 %d 项断言，失败 %d → %s\n", checks, failures, failures ? "FAILED" : "ALL PASS");
    return failures ? 1 : 0;
}
