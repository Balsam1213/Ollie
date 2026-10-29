/*
 * Face 桩：单测不链接 Face.cpp，改由这里提供同名实现，
 * 把 PetState 传给 Face 的表情/台词记录到全局变量供断言。
 */
#include "Face.h"

Emotion g_lastBase = Emotion::Calm;
Emotion g_lastOverride = Emotion::Calm;
uint32_t g_lastHold = 0;
int g_baseCalls = 0;
int g_overrideCalls = 0;
const char* g_lastDialog = nullptr;
uint32_t g_lastDialogHold = 0;
int g_dialogCalls = 0;

bool Face::begin() { return true; }
void Face::setBase(Emotion e) {
    g_lastBase = e;
    g_baseCalls++;
}
void Face::setExpression(Emotion e, uint32_t holdMs) {
    g_lastOverride = e;
    g_lastHold = holdMs;
    g_overrideCalls++;
}
void Face::showDialog(const char* text, uint32_t holdMs) {
    g_lastDialog = text;
    g_lastDialogHold = holdMs;
    g_dialogCalls++;
}
void Face::update(uint32_t) {}
