#include "MoodLight.h"

// arduino-esp32 3.x 叫 rgbLedWrite，2.x 叫 neopixelWrite，做个兼容垫片
#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define OLLIE_RGB_WRITE(pin, r, g, b) rgbLedWrite(pin, r, g, b)
#else
#define OLLIE_RGB_WRITE(pin, r, g, b) neopixelWrite(pin, r, g, b)
#endif

void MoodLight::begin(uint32_t now) {
    (void)now;
    OLLIE_RGB_WRITE(PIN_RGB_LED, 0, 0, 0);
}

void MoodLight::setMood(uint8_t moodPercent, bool sleeping_) {
    mood = moodPercent;
    sleeping = sleeping_;
}

void MoodLight::happyFlash(uint32_t now) {
    happyUntil = now + HAPPY_HOLD_MS;
}

void MoodLight::targetColor(uint32_t now, uint8_t& r, uint8_t& g, uint8_t& b) const {
    // 呼吸：在 lo..hi 之间以 period 为周期缓慢起伏
    auto breath = [](uint32_t t, uint32_t period, float lo, float hi) {
        return lo + (hi - lo) * (0.5f + 0.5f * sinf(6.2831853f * t / (float)period));
    };

    if (now < happyUntil) { // 互动后：粉色快速脉动
        float s = 0.5f + 0.5f * sinf(now / 120.0f);
        r = 255;
        g = (uint8_t)(80 * s);
        b = (uint8_t)(120 * s);
    } else if (sleeping) { // 睡眠：深蓝极慢呼吸 10%~30%
        float s = breath(now, 4000, 0.10f, 0.30f);
        r = 0;
        g = 0;
        b = (uint8_t)(255 * s);
    } else if (mood < 40) { // 心情差：橙红慢闪
        bool on = (now / 800) % 2 == 0;
        r = on ? 255 : 40;
        g = on ? 60 : 8;
        b = 0;
    } else if (mood < 70) { // 心情一般：琥珀黄常亮
        r = 255;
        g = 150;
        b = 0;
    } else { // 心情好：青绿缓慢呼吸
        float s = breath(now, 3000, 0.55f, 1.0f);
        r = 0;
        g = (uint8_t)(255 * s);
        b = (uint8_t)(120 * s);
    }
}

void MoodLight::update(uint32_t now) {
    uint8_t tr, tg, tb;
    targetColor(now, tr, tg, tb);

    // 每帧向目标走约 1/4（至少 1），约 130-260ms 完成过渡，不硬跳
    auto step = [](int cur, int target) {
        int d = (target - cur) / 4;
        if (d == 0 && target != cur) d = (target > cur) ? 1 : -1;
        int v = cur + d;
        return (int)constrain(v, 0, 255);
    };
    curR = step(curR, tr);
    curG = step(curG, tg);
    curB = step(curB, tb);

    OLLIE_RGB_WRITE(PIN_RGB_LED,
                    (uint8_t)(curR * LED_MAX / 255),
                    (uint8_t)(curG * LED_MAX / 255),
                    (uint8_t)(curB * LED_MAX / 255));
}
