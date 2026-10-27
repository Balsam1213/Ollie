#include "LightSensor.h"

void LightSensor::begin(bool digitalMode, uint32_t now) {
    digital = digitalMode;
    if (digital) {
        // DO 模式同引脚接模块 DO；LM393 开集输出，内部上拉兜底防悬空
        pinMode(PIN_LIGHT_AO, INPUT_PULLUP);
        prevLevel = digitalRead(PIN_LIGHT_AO);
        reading = (uint16_t)prevLevel;
    } else {
        analogReadResolution(12); // 0-4095
        reading = analogRead(PIN_LIGHT_AO);
        prevLevel = -1;
    }
    dark = false;
    sleepFired = false;
    darkSince = now;
    brightSince = 0;
}

LightEvent LightSensor::update(uint32_t now) {
    // —— 采样并给出"此刻是否暗"的原始判定 ——
    bool darkNow;
    if (digital) {
        int lv = digitalRead(PIN_LIGHT_AO);
        if (lv != prevLevel) { // 电平与上次不同：临界抖动嫌疑，只记录不采纳
            prevLevel = lv;
            return LightEvent::None;
        }
        reading = (uint16_t)lv;
        darkNow = (LIGHT_DO_DARK_LEVEL == 1) ? (lv == HIGH) : (lv == LOW);
    } else {
        uint16_t r = analogRead(PIN_LIGHT_AO);
        reading = (uint16_t)((reading * 7u + r) / 8u); // 滑动均值，抑制抖动
        // 硬件迟滞：两阈值之间维持原判定
        if (!dark && reading < LIGHT_DARK_ADC) darkNow = true;
        else if (dark && reading > LIGHT_BRIGHT_ADC) darkNow = false;
        else darkNow = dark;
    }

    // —— 公共状态机 ——
    if (darkNow) {
        brightSince = 0; // 亮态被打断（抖动过滤）
        if (!dark) {
            dark = true;
            darkSince = now;
        }
    } else if (dark) {
        if (digital) { // AO 有硬件迟滞立即醒；DO 需持续变亮 WAKE_CONFIRM_S
            if (brightSince == 0) brightSince = now;
            if ((uint32_t)(now - brightSince) < (uint32_t)WAKE_CONFIRM_S * 1000ul)
                return LightEvent::None;
        }
        dark = false;
        sleepFired = false;
        brightSince = 0;
        return LightEvent::WakeTime;
    }

    if (dark && !sleepFired &&
        (uint32_t)(now - darkSince) >= (uint32_t)DARK_CONFIRM_S * 1000ul) {
        sleepFired = true;
        return LightEvent::SleepTime; // 同一轮暗期只发一次
    }
    return LightEvent::None;
}
