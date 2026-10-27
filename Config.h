#pragma once
/*
 * Ollie 全局配置：引脚、阈值、节奏集中在这里，调试改这一个文件就够
 */
#include <Arduino.h>

// arduino-esp32 2.x / 3.x 均可编译：LEDC 与 WS2812 的 API 差异
// 在 Buzzer.cpp / MoodLight.cpp 内做了版本垫片（Wokwi 仿真同样适用）

// ===== 调试开关 =====
#define OLLIE_DEBUG 1 // 1: 串口(115200)打印事件日志；0: 关闭
// #define DEBUG_FAST_DECAY 1 // 定义后数值衰减变为 1点/秒，方便快速看全程

#if OLLIE_DEBUG
#define LOGF(...) Serial.printf(__VA_ARGS__)
#else
#define LOGF(...)
#endif

// ===== 引脚（v1 定稿，理由见 docs/plan-v1.md 第二节） =====
#define PIN_OLED_SDA 8
#define PIN_OLED_SCL 9
#define PIN_BTN_FEED 4  // 轻触开关另一脚接 GND
#define PIN_BTN_PET 5   // 同上
#define PIN_LIGHT_AO 1  // 光敏 AO，必须 ADC1（GPIO1-10，避开 v2 WiFi 的 ADC2 冲突）
#define PIN_BUZZER 15   // 无源蜂鸣器
#define PIN_RGB_LED 48  // 板载 WS2812；个别批次是 GPIO38，以板上丝印为准

// ===== OLED =====
#define OLED_ADDR 0x3C
#define FRAME_MS 33 // ~30fps

// ===== 按键 =====
#define DEBOUNCE_MS 30

// ===== 光敏（两种模块二选一，见 README 第六节标定） =====
#define LIGHT_USE_DO 1        // 1: 3 针 LM393 模块只有 DO（数字）；0: 4 针模块用 AO（模拟）
#define LIGHT_DO_DARK_LEVEL 1 // DO 模式"暗"的电平：遮光时串口打印 DO=1 填 1，打印 DO=0 填 0
#define LIGHT_DARK_ADC 800   // AO 模式：低于此值 = 暗
#define LIGHT_BRIGHT_ADC 2000 // AO 模式：高于此值 = 亮（与上面构成迟滞，中间维持原状态）
#define DARK_CONFIRM_S 10    // 持续变暗 10 秒才入睡（两种模式共用）
#define WAKE_CONFIRM_S 2     // DO 模式专属：持续变亮 2 秒才醒（补偿其无硬件迟滞）

// ===== 数值 =====
#define STAT_MIN 0
#define STAT_MAX 100
#ifdef DEBUG_FAST_DECAY
#define SETTLE_MS 1000UL // 调试：1 秒一轮
#else
#define SETTLE_MS 60000UL // 正式：1 分钟一轮
#endif
#define DECAY_PER_TICK 1  // 醒着时三项各 -1
#define ENERGY_GAIN_SLEEP 2 // 睡觉时精力 +2（饱食/心情停掉）

// ===== 互动 =====
#define HAPPY_HOLD_MS 3000 // 喂食/抚摸后的开心表情时长
#define FEED_FOOD 20
#define FEED_MOOD 4
#define PET_MOOD 15

// ===== 表情阈值 =====
#define HUNGRY_BELOW 30 // 饱食度 < 30 → 饥饿脸
#define SLEEPY_BELOW 30 // 精力 < 30 → 困倦脸

// ===== 蜂鸣器 =====
#define BEEP_FREQ 2000
#define BEEP_MS 80 // 哔-哔：80ms 响 / 80ms 停 / 80ms 响

// ===== 心情灯 =====
#define LED_MAX 40 // WS2812 亮度封顶（/255），防夜间刺眼

// ===== 附加功能开关（0/1，按需打开） =====
#define BOOT_I2C_SCAN 0   // 1: 上电只跑 I2C 总线扫描并打印（M1 排查 OLED 地址/接线用）
#define LIGHT_CALIBRATE 0 // 1: 每 500ms 打印光敏 ADC 均值（M5 标定阈值用，串口看数值）
#define PET_WAKE_ENABLED 0 // 1: 睡觉时按"抚摸"可唤醒（彩蛋，默认按计划关闭）
