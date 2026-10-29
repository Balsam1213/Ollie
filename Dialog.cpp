#include "Dialog.h"
#include "DialogLines.h"

// 场景索引与台词池表（顺序即 DialogScene 枚举；lastIdx_ 按此索引）
namespace {
enum Scene : uint8_t { SC_CALM, SC_HUNGRY, SC_SLEEPY, SC_SAD, SC_SLEEP, SC_WAKE, SC_FEED, SC_PET, SC_IDLE, SC_COUNT };
struct Pool {
    const char* const* lines;
    uint8_t count;
};
const Pool POOLS[SC_COUNT] = {
    {LINES_CALM, N_CALM},   {LINES_HUNGRY, N_HUNGRY}, {LINES_SLEEPY, N_SLEEPY},
    {LINES_SAD, N_SAD},     {LINES_SLEEP, N_SLEEP},   {LINES_WAKE, N_WAKE},
    {LINES_FEED, N_FEED},   {LINES_PET, N_PET},       {LINES_IDLE, N_IDLE},
};
static_assert(SC_COUNT == 9, "场景数与 POOLS 表不一致");
} // namespace

void Dialog::begin(Face* face, uint32_t now) {
    face_ = face;
    lastInteractionAt_ = now;
    nextChatterAt_ = now + (uint32_t)random(DIALOG_CHATTER_MIN_S, DIALOG_CHATTER_MAX_S) * 1000UL;
}

uint8_t Dialog::pickIndex(uint8_t count, uint8_t last) {
    if (count <= 1) return 0;
    uint8_t i = (uint8_t)random(count);
    if (i == last) i = (uint8_t)((i + 1) % count); // 保证不连续重复
    return i;
}

void Dialog::say(uint8_t scene, uint32_t now, bool force) {
    if (!DIALOG_ENABLE || face_ == nullptr) return;
    if (scene >= SC_COUNT) return;
    if (!force && (uint32_t)(now - lastDialogAt_) < DIALOG_MIN_GAP_MS) return;
    const Pool& p = POOLS[scene];
    if (p.count == 0) return;
    uint8_t idx = pickIndex(p.count, lastIdx_[scene]);
    lastIdx_[scene] = idx;
    lastDialogAt_ = now;
    face_->showDialog(p.lines[idx], DIALOG_HOLD_MS);
    LOGF("[dialog] 场景%u 第%u句\n", (unsigned)scene, (unsigned)idx);
}

void Dialog::onFeed(uint32_t now) {
    markInteraction(now);
    say(SC_FEED, now, true);
}

void Dialog::onPet(uint32_t now) {
    markInteraction(now);
    say(SC_PET, now, true);
}

void Dialog::onSleep(uint32_t now) {
    say(SC_SLEEP, now, true);
}

void Dialog::onWake(uint32_t now) {
    say(SC_WAKE, now, true);
}

void Dialog::markInteraction(uint32_t now) {
    lastInteractionAt_ = now;
    idleSaid_ = false;
}

void Dialog::onSettle(uint32_t now, const StatsData& d, const StatsData& prev, bool asleep) {
    // 阈值跨越：只在跨越那一轮说（醒来回升后再跌破会再说）
    if (!asleep) {
        if (prev.food >= HUNGRY_BELOW && d.food < HUNGRY_BELOW) {
            say(SC_HUNGRY, now, true);
        } else if (prev.energy >= SLEEPY_BELOW && d.energy < SLEEPY_BELOW) {
            say(SC_SLEEPY, now, true);
        } else if (prev.mood >= 40 && d.mood < 40) {
            say(SC_SAD, now, true);
        }
    }

    if (asleep) return; // 睡觉时不闲聊不撒娇

    // 冷落撒娇：30 分钟无互动说一次
    if (!idleSaid_ && (uint32_t)(now - lastInteractionAt_) >= DIALOG_IDLE_MIN_S * 1000UL) {
        say(SC_IDLE, now, false);
        idleSaid_ = true;
        return;
    }

    // 闲聊：台词池跟随当前主导情绪（优先级与表情一致）
    if ((int32_t)(now - nextChatterAt_) >= 0 &&
        (uint32_t)(now - lastDialogAt_) >= DIALOG_MIN_GAP_MS) {
        uint8_t scene = d.mood < 40 ? SC_SAD
                        : d.food < HUNGRY_BELOW ? SC_HUNGRY
                        : d.energy < SLEEPY_BELOW ? SC_SLEEPY
                                                  : SC_CALM;
        say(scene, now, false);
        nextChatterAt_ = now + (uint32_t)random(DIALOG_CHATTER_MIN_S, DIALOG_CHATTER_MAX_S) * 1000UL;
    }
}
