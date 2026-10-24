#pragma once
// Wire 桩：只需让 Face.h 里的成员声明可编译
class TwoWire {
public:
    void begin(int, int) {}
    void setClock(long) {}
    void beginTransmission(uint8_t) {}
    int endTransmission() { return 0; }
};
inline TwoWire Wire;
