# Ollie 🐣

基于 ESP32-S3 的桌面电子宠物：不追求完整的宠物轮廓，核心只有一块 0.96" OLED 上的一张脸——眼睛和嘴巴全部用代码画出来，靠表情、数值和互动让人感觉它"活着"。

**v1（离线版，插电即玩）**：
- 表情动画：自动眨眼、眼珠乱瞟；平静 / 开心 / 饥饿 / 困倦 / 睡眠五种表情
- 按键互动：喂食、抚摸两个轻触开关，触发开心表情 3 秒 + 蜂鸣器两声
- 三项数值：饱食度 / 心情 / 精力，每分钟各掉 1 点，NVS 存 Flash 断电不丢；睡觉时精力恢复
- 昼夜感知：光敏检测持续变暗 10 秒自动入睡，变亮自动醒来
- 心情灯：板载 WS2812 颜色随心情变化

**后续版本**：AI 聊天、语音、外壳，按阶段迭代。

## 目录结构

```
Ollie.ino            入口 + 毫秒级调度器
Config.h             全部引脚/阈值/节奏，调试改这里
PetState.h/.cpp      状态机：醒/睡切换、表情优先级
Stats.h/.cpp         三项数值 + NVS 存档
Face.h/.cpp          表情绘制与动画（眨眼/眼动/Zzz/防灼伤漂移）
Buttons.h/.cpp       消抖按键
LightSensor.h/.cpp   光敏采样 + 昼夜判定（迟滞）
Buzzer.h/.cpp        无源蜂鸣器
MoodLight.h/.cpp     WS2812 心情灯
platformio.ini       VS Code + PlatformIO 配置
wokwi/               Wokwi 在线仿真（diagram.json + 使用说明）
docs/plan-v1.md      可行性评定与实施计划
```

## 编译烧录

**Arduino IDE 2.x（推荐）**：开发板管理器装 `esp32 by Espressif`，库管理器装 `Adafruit SSD1306`，然后直接打开 `Ollie.ino`。板子选项见[计划第五节](docs/plan-v1.md#五开发环境)——务必开 `USB CDC On Boot: Enabled`，否则串口无输出。

**VS Code + PlatformIO**：直接用根目录的 `platformio.ini`（已含 N16R8 的 Flash/PSRAM 参数与库依赖）。

调试开关在 `Config.h`：
- `OLLIE_DEBUG`：串口事件日志（115200），默认开
- `DEBUG_FAST_DECAY`：注释掉则数值 1 分钟一轮，打开则 1 秒一轮，快速验证数值曲线

## 在线仿真（Wokwi）

不接硬件先初验：按 [wokwi/README.md](wokwi/README.md) 在浏览器里仿真 ESP32-S3-DevKitC-1 + OLED + 双按键 + 光敏 + 蜂鸣器 + 板载 RGB，引脚与实机一致，可验收 M1–M6 的绝大部分逻辑。

## 硬件

ESP32-S3-DevKitC-1 (N16R8) · 0.96" SSD1306 128×64 I2C OLED · 光敏模块 · 无源蜂鸣器 · 轻触开关 ×2 · 板载 WS2812

引脚分配表见[计划第二节](docs/plan-v1.md#二硬件与接线)。

## 文档

- [v1 可行性评定与实施计划](docs/plan-v1.md)
