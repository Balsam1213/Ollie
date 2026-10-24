#include "Buzzer.h"

// LEDC 版本垫片：3.x 用 ledcAttach；2.x 用 ledcSetup + ledcAttachPin
#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define OLLIE_LEDC_ATTACH(pin, freq, res) ledcAttach(pin, freq, res)
#else
#define OLLIE_LEDC_ATTACH(pin, freq, res) \
    do {                                  \
        ledcSetup(0, (freq), (res));      \
        ledcAttachPin((pin), 0);          \
    } while (0)
#endif

void Buzzer::begin() {
    OLLIE_LEDC_ATTACH(PIN_BUZZER, BEEP_FREQ, 10);
}

void Buzzer::twoBeeps(uint32_t now) {
    playing = true;
    start = now;
}

void Buzzer::update(uint32_t now) {
    if (!playing) return;
    uint32_t t = now - start;
    bool on = (t < BEEP_MS) || (t >= 2 * BEEP_MS && t < 3 * BEEP_MS);
    if (t >= 3 * BEEP_MS) {
        on = false;
        playing = false;
    }
    ledcWriteTone(PIN_BUZZER, on ? BEEP_FREQ : 0);
}
