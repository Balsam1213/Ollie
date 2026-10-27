# Wokwi 在线仿真（上机前初验）

在 [Wokwi](https://wokwi.com) 浏览器里仿真 ESP32-S3-DevKitC-1，**引脚与实机完全一致**，不用改一行代码。适合在硬件到货前验证 M1–M6 的全部逻辑。

**能验证**：表情与动画、按键互动、蜂鸣器、数值衰减与 NVS 断电恢复、昼夜判定（用光照滑杆代替遮光）、RGB 心情灯颜色。
**不能验证**：真实光敏阈值标定（光谱不同）、蜂鸣器音量、WS2812 实际亮度、电气问题（接线松动/上拉缺失等）。

## 搭建步骤

1. 打开 [wokwi.com/projects/new/esp32-s3](https://wokwi.com/projects/new/esp32-s3) 新建项目（免费账号可保存）
2. 在左侧项目文件区：
   - 删除模板自带的 `sketch.ino`，用"+"新建 `Ollie.ino`，把仓库根目录的 `Ollie.ino` 内容贴进去
   - 依次新建并粘贴以下文件（**全部平铺在项目根，不要建子文件夹**）：
     `Config.h`、`ChatLogic.h`、`ChatLogic.cpp`、`Stats.h`、`Stats.cpp`、`Buttons.h`、`Buttons.cpp`、`Buzzer.h`、`Buzzer.cpp`、`LightSensor.h`、`LightSensor.cpp`、`Face.h`、`Face.cpp`、`MoodLight.h`、`MoodLight.cpp`、`PetState.h`、`PetState.cpp`、`NetConfig.h`、`NetConfig.cpp`、`LLMClient.h`、`LLMClient.cpp`、`WebChat.h`、`WebChat.cpp`
3. 新建 `diagram.json`，粘贴本目录 [diagram.json](diagram.json) 的内容
   - 若编辑器报板卡 type 不认识，以新建项目模板 diagram.json 里的板卡行为准，只替换其余 parts 和 connections
   - 若某根线没接上（引脚名不匹配），鼠标悬停引脚可查看名称，改对应字符串即可
4. 新建 `libraries.txt`，粘贴本目录 [libraries.txt](libraries.txt) 的内容
5. 点绿色 ▶ 播放键编译运行；下方 **Serial Monitor** 标签看日志（115200）

## 仿真验收清单（对照 docs/plan-v1.md 的里程碑）

| 操作 | 预期 |
|---|---|
| 运行后先看屏幕 | 显示 "Ollie" 约 1 秒 → 平静脸（M1/M2） |
| 静置观察 | 每 2–6 秒自动眨眼；每 3–8 秒眼珠乱瞟一次（M2） |
| 点绿色按钮（喂食）/ 蓝色按钮（抚摸） | 开心眯眼脸 3 秒 + 蜂鸣器两声 + 板载 RGB 粉色脉动（M3/M6） |
| 把光敏模块的 light 滑杆拖到最低 | 约 10 秒后入睡：闭眼 + Zzz 上浮 + 屏幕变暗 + RGB 深蓝呼吸；串口停止掉数值、精力回升（M5） |
| 睡着后把滑杆拖高 | 立即醒来（M5） |
| 睡着时点按钮 | 无反应（默认设定；彩蛋"抚摸唤醒"开关 `PET_WAKE_ENABLED` 在 Config.h） |
| 先在 Config.h 打开 `DEBUG_FAST_DECAY`（去掉行首注释）再运行 | 数值 1 秒掉 1 点：饱食 <30 变饥饿脸（眉毛下压+波浪嘴），精力 <30 变困倦脸（眼皮垂 + o 嘴 + 整脸晃动），心情档位切换灯色（M4/M6） |
| 点停止再运行（模拟断电重启） | 数值从上次值继续，不重置（M4；仿真里 NVS 会保留，实机复测为准） |

## 已知差异与技巧

- **v2.0 联网功能**：Wokwi 需要开启 IoT 网关（浏览器弹窗授权）才能仿真 WiFi，且聊天 E2E 依赖真实 API 网络，未在仿真中验证；仿真主要看离线玩法逻辑。WiFi 相关代码已通过编译 + 逻辑单测覆盖（ChatLogic 100%）
- **光敏 DO 模式**：固件默认 `LIGHT_USE_DO=1`（实机 3 针模块），但 Wokwi 的光敏部件在仿真里 **AO 输出对滑杆响应可靠、DO 可能不响应**（已知 issue）。仿真验证昼夜请临时把 Config.h 的 `LIGHT_USE_DO` 改 `0`（diagram.json 已接 AO）；实机 DO 模块把橙线改接 DO、`LIGHT_USE_DO` 保持 1 即可
- 板载 RGB 灯在仿真图里就是板子右下角那颗彩色 LED（对应 GPIO48，实机同款）
- 蜂鸣器音量可在 diagram.json 的 `bz` attrs 里调 `volume`（0.01–1.0）；没声音先检查浏览器是否静音
- 光敏的 light 滑杆从左到右 = 从暗到亮；拖动时看串口 ADC 打印值，可以顺便给实机标定 `LIGHT_DARK_ADC` / `LIGHT_BRIGHT_ADC`
- 仿真不走 PSRAM/Flash 时序，`platformio.ini` 里的 N16R8 参数对仿真无影响

## VS Code 里跑（可选）

装 Wokwi for VS Code 扩展后，在工程里加 `wokwi.toml` 指向 PlatformIO 构建产物即可本地仿真，配置见 [Wokwi 官方文档](https://docs.wokwi.com/vscode/platformio)。
