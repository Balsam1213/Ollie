# 工作说明 · work-guide

新会话/新成员上手第一篇：怎么把环境跑起来、怎么干活、坑在哪。

## 1. 项目一句话

ESP32-S3 桌面电子宠物：OLED 上的一张脸 + 五表情 + 三数值养成 + 昼夜睡眠 + 心情灯；v1 纯离线，v2 加 AI 聊天。当前状态：**v1 代码全量完成，三层验证（单测/编译/仿真）全绿，待实机验收**。

## 2. 环境搭建

**本机已就绪**（2026-10-25 配好）：Python 3.14（用 `py` 调用，`python` 命令是坏的占位符）、PlatformIO + 完整 ESP32-S3 工具链（`py -m platformio run` 12 秒出固件）、zig（跑单测）。

**新机器从零搭**：
1. Python 3.10+，确认 `py --version` 可用
2. `py -m pip install platformio ziglang`
3. 硬件烧录另装 CP210x 驱动（DevKitC-1 的 USB 串口）；Arduino IDE 路线则装 IDE + esp32 板级支持 + Adafruit SSD1306 库（板子选项务必开 **USB CDC On Boot: Enabled**，否则串口无输出）
4. 自检：`py -m platformio run` 编译通过 = 环境就绪

## 3. 干活的标准循环

```
看 devlog/TODO.md 选任务
→ 读 docs/requirements.md（为什么做）+ tech-spec.md（影响什么）
→ 按 design-conventions.md 写码，参数进 Config.h
→ 三层验证（procedures.md §1–3）：单测 → 编译 → Wokwi 冒烟
→ 有硬件则实机验收
→ 更新 devlog 当日日志 + 勾 TODO
→ git commit（Conventional Commits）
```

## 4. 常见任务指引

**加/改一个数值规则**（如新互动）：改 `Stats` + `PetState` 事件分发 → 补 `test/test_main.cpp` 断言 → 跑单测 → 同步 requirements.md FR 表和 tech-spec §2.2。

**加一种表情**：`Face.h` 的 `Emotion` 枚举加项 → Face.cpp 渲染 switch 加分支 + 表情参数表加行 → PetState 优先级链插入 → Wokwi 看观感 → 若可被数值触发，更新 requirements FR-05。

**调脸部观感**：只动 Face.cpp 顶部几何常量（眼位/尺寸/嘴弧），不用动逻辑；在 Wokwi 上调最快。

**加外设**：先查 tech-spec §1.2 引脚表有没有空位、是否踩 v2 预留 → 新建模块 .h/.cpp（自封装时序）→ Ollie.ino 调度表加行 → 单测桩 + 仿真部件。

## 5. 常见坑速查

| 症状 | 原因 → 解法 |
|---|---|
| 串口完全无输出 | USB CDC On Boot 没开 → Arduino IDE 板子选项打开 |
| 照教程抄 LEDC 代码编译不过 | arduino-esp32 3.x 换了 API → 用本项目 Buzzer.cpp 的垫片写法 |
| 外设接 GPIO35–37 后玄学死机 | N16R8 八线 PSRAM 占用 → 换脚 |
| 上传失败 | 按住 BOOT 再点上传 |
| 光敏来回横跳睡醒 | 阈值差太小 → 按 procedures §4 重标定，拉大迟滞带 |
| Wokwi 元件看不见 | 布局太散视野外 → 用 wokwi/diagram.json 的紧凑布局 |
| `python`/`pip` 命令不存在 | 用 `py` / `py -m pip` |
| PlatformIO 首次构建很慢 | 在下载 ~1GB 工具链，正常，等它跑完 |

## 6. 求助与交接

- **为什么这么设计**：查 devlog 各日"决策"节
- **这个东西当时怎么坏的**：查 devlog 各日"问题与坑"节
- **现在进行到哪**：devlog/TODO.md
- 交接三件套：README（全貌）→ 本篇（怎么干活）→ TODO（接着做什么）
