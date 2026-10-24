#pragma once
/*
 * Arduino API 最小仿真桩 —— 仅供 test/ 下的本地逻辑单测使用，不参与固件编译
 * 时间由 g_ms 驱动（测试直接推进），IO 由 g_pinLevel / g_adc 注入
 */
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cmath>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(amt, low, high) \
    ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

extern uint32_t g_ms;         // 仿真时钟（毫秒）
extern int g_pinLevel[64];    // digitalRead 电平表（按 GPIO 号）
extern uint16_t g_adc[16];    // analogRead 值表
extern uint32_t g_lastTone;   // ledcWriteTone 最近频率（0=停）
extern uint32_t g_rgb[3];     // WS2812 最近一次 RGB 输出

inline uint32_t millis() { return g_ms; }
inline void delay(uint32_t ms) { g_ms += ms; }
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t pin) { return g_pinLevel[pin]; }
inline void analogReadResolution(int) {}
inline uint16_t analogRead(uint8_t pin) { return g_adc[pin]; }
inline long random(long lo, long hi) { return lo + std::rand() % (hi - lo); }
inline long random(long hi) { return std::rand() % hi; }
inline void randomSeed(unsigned long) {}

// LEDC（2.x/3.x 两种 API 都给桩）
inline void ledcSetup(int, uint32_t, int) {}
inline void ledcAttachPin(uint8_t, int) {}
inline bool ledcAttach(uint8_t, uint32_t, int) { return true; }
inline void ledcWriteTone(uint8_t, uint32_t f) { g_lastTone = f; }

// WS2812（2.x/3.x）
inline void neopixelWrite(uint8_t, uint8_t r, uint8_t g, uint8_t b) {
    g_rgb[0] = r;
    g_rgb[1] = g;
    g_rgb[2] = b;
}
inline void rgbLedWrite(uint8_t p, uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(p, r, g, b); }

struct MockSerial {
    void begin(unsigned long) {}
    void print(const char* s) { std::printf("%s", s); }
    void println(const char* s) { std::printf("%s\n", s); }
    void printf(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        std::vprintf(fmt, ap);
        va_end(ap);
    }
};
extern MockSerial Serial;
