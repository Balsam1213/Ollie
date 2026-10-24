#include "Buttons.h"

void DebouncedButton::begin(uint8_t pin, uint32_t now) {
    this->pin = pin;
    pinMode(pin, INPUT_PULLUP);
    stable = HIGH;
    raw = HIGH;
    lastChange = now;
}

bool DebouncedButton::fell(uint32_t now) {
    int r = digitalRead(pin);
    if (r != raw) {
        raw = r;
        lastChange = now;
    }
    // 电平与稳定态不同且已保持超过消抖窗口 → 采纳为新稳定态
    if (raw != stable && (uint32_t)(now - lastChange) >= DEBOUNCE_MS) {
        bool pressed = (stable == HIGH && raw == LOW);
        stable = raw;
        return pressed;
    }
    return false;
}

void Buttons::begin(uint32_t now) {
    feed.begin(PIN_BTN_FEED, now);
    pet.begin(PIN_BTN_PET, now);
}

ButtonEvent Buttons::update(uint32_t now) {
    if (feed.fell(now)) return ButtonEvent::Feed;
    if (pet.fell(now)) return ButtonEvent::Pet;
    return ButtonEvent::None;
}
