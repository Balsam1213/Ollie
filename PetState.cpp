#include "PetState.h"
#include "Config.h"

void PetState::begin(Stats* s, Face* f, Buzzer* b, MoodLight* m, uint32_t now) {
    stats = s;
    face = f;
    buzzer = b;
    light = m;
    dialog.begin(face, now);
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

    // 先亮开心脸，接一句台词（"先笑再说话"）
    face->setExpression(Emotion::Happy, DIALOG_FEED_FACE_MS);
    buzzer->twoBeeps(now);
    light->happyFlash(now);
    if (ev == ButtonEvent::Feed) dialog.onFeed(now);
    else                         dialog.onPet(now);
    refresh(now);
}

void PetState::onLight(LightEvent ev, uint32_t now) {
    if (ev == LightEvent::SleepTime && !asleep) {
        asleep = true;
        dialog.onSleep(now); // 睡前一句（台词期间屏幕保持亮度，播完才变暗）
    } else if (ev == LightEvent::WakeTime && asleep) {
        asleep = false;
        dialog.onWake(now);
    } else {
        return;
    }
    refresh(now);
}

void PetState::onSettle(uint32_t now) {
    StatsData before = stats->data();
    stats->tick(asleep); // 醒着三项各 -1；睡眠时精力 +2、其余停
    refresh(now);
    dialog.onSettle(now, stats->data(), before, asleep); // 跨越台词 + 闲聊/冷落
}
