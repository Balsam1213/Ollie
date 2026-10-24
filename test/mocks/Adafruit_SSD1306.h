#pragma once
// Adafruit SSD1306 桩：Face.h 只需要"这个类型存在且有对应构造函数"
#include "Wire.h"
class Adafruit_SSD1306 {
public:
    Adafruit_SSD1306(int, int, TwoWire*, int) {}
};
