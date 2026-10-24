#include "Stats.h"
#include "Config.h"

static const char* NVS_NS = "ollie";
static const char* NVS_KEY = "stats";

void Stats::begin() {
    prefs.begin(NVS_NS, false);
    StatsData loaded{};
    size_t got = prefs.getBytes(NVS_KEY, &loaded, sizeof(loaded));
    if (got == sizeof(loaded)) {
        d = loaded;
    } else {
        d = {80, 80, 80}; // 首次上电
    }
    d.food = constrain(d.food, STAT_MIN, STAT_MAX);
    d.mood = constrain(d.mood, STAT_MIN, STAT_MAX);
    d.energy = constrain(d.energy, STAT_MIN, STAT_MAX);
}

void Stats::tick(bool sleeping) {
    StatsData before = d;
    if (sleeping) {
        d.energy = clampAdd(d.energy, +ENERGY_GAIN_SLEEP);
    } else {
        d.food = clampAdd(d.food, -DECAY_PER_TICK);
        d.mood = clampAdd(d.mood, -DECAY_PER_TICK);
        d.energy = clampAdd(d.energy, -DECAY_PER_TICK);
    }
    if (memcmp(&before, &d, sizeof(d)) != 0) {
        prefs.putBytes(NVS_KEY, &d, sizeof(d)); // 数值有变化才写，进一步省 Flash
    }
}

void Stats::onFeed() {
    d.food = clampAdd(d.food, +FEED_FOOD);
    d.mood = clampAdd(d.mood, +FEED_MOOD);
}

void Stats::onPet() {
    d.mood = clampAdd(d.mood, +PET_MOOD);
}

uint8_t Stats::clampAdd(uint8_t v, int delta) {
    int r = (int)v + delta;
    if (r < STAT_MIN) r = STAT_MIN;
    if (r > STAT_MAX) r = STAT_MAX;
    return (uint8_t)r;
}
