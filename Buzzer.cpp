#include "Buzzer.h"

void Buzzer::begin() {
    ledcAttach(PIN_BUZZER, BEEP_FREQ, 10); // 3.x：一个调用完成绑定
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
