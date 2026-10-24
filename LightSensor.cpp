#include "LightSensor.h"

void LightSensor::begin(uint32_t now) {
    analogReadResolution(12); // 0-4095
    avg = analogRead(PIN_LIGHT_AO);
    darkSince = now;
}

LightEvent LightSensor::update(uint32_t now) {
    uint16_t r = analogRead(PIN_LIGHT_AO);
    avg = (uint16_t)((avg * 7u + r) / 8u); // 滑动均值，抑制抖动

    if (!dark && avg < LIGHT_DARK_ADC) {
        dark = true;
        darkSince = now;
    }
    if (dark && avg > LIGHT_BRIGHT_ADC) {
        dark = false;
        sleepFired = false;
        return LightEvent::WakeTime; // 暗转亮；PetState 只在睡着时才理会
    }
    if (dark && !sleepFired &&
        (uint32_t)(now - darkSince) >= (uint32_t)DARK_CONFIRM_S * 1000ul) {
        sleepFired = true;
        return LightEvent::SleepTime;
    }
    return LightEvent::None;
}
