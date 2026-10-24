#include "PetState.h"
#include "Config.h"

void PetState::begin(Stats* s, Face* f, Buzzer* b, MoodLight* m, uint32_t now) {
    stats = s;
    face = f;
    buzzer = b;
    light = m;
    refresh(now);
}

void PetState::refresh(uint32_t now) {
    (void)now;
    const StatsData& d = stats->data();
    Emotion e = asleep                   ? Emotion::Sleeping
                : (d.food < HUNGRY_BELOW)   ? Emotion::Hungry
                : (d.energy < SLEEPY_BELOW) ? Emotion::Sleepy
                                            : Emotion::Calm;
    face->setBase(e);
    light->setMood(d.mood, asleep);
}

void PetState::onButton(ButtonEvent ev, uint32_t now) {
    if (ev == ButtonEvent::None) return;

#if PET_WAKE_ENABLED
    // 彩蛋：睡着时按"抚摸"唤醒（喂食不唤醒）；默认关闭
    if (asleep) {
        if (ev == ButtonEvent::Pet) {
            asleep = false;
            refresh(now);
        }
        return;
    }
#else
    if (asleep) return; // 睡着时按键无效
#endif

    if (ev == ButtonEvent::Feed) stats->onFeed();
    else                         stats->onPet();

    face->setExpression(Emotion::Happy, HAPPY_HOLD_MS); // 临时开心 3s
    buzzer->twoBeeps(now);
    light->happyFlash(now);
    refresh(now);
}

void PetState::onLight(LightEvent ev, uint32_t now) {
    if (ev == LightEvent::SleepTime && !asleep) {
        asleep = true;
    } else if (ev == LightEvent::WakeTime && asleep) {
        asleep = false;
    } else {
        return;
    }
    refresh(now);
}

void PetState::onSettle(uint32_t now) {
    stats->tick(asleep); // 醒着三项各 -1；睡眠时精力 +2、其余停
    refresh(now);
}
