# 执行步骤 · procedures

动手干活照着做。环境准备见 [work-guide.md](work-guide.md)。

## 1. 构建固件

**路线 A · Arduino IDE 2.x**：打开 `Ollie.ino` → 板子选 ESP32S3 Dev Module，关键选项：USB CDC On Boot **Enabled**、Flash Size 16MB、PSRAM OPI → 编译上传。

**路线 B · PlatformIO**：
```bash
py -m platformio run                 # 只编译
py -m platformio run -t upload       # 编译+烧录（板子插 USB）
py -m platformio device monitor      # 串口监视器 115200
```
上传失败：按住板上 BOOT 键再点上传。

## 2. 本地逻辑单测（改了逻辑必跑）

```bash
bash test/run_tests.sh
# 期望最后一行：ALL PASS（当前 102 项断言）
```

## 3. Wokwi 在线仿真（改了外设/渲染必跑）

1. 打开 [wokwi.com/projects/new/esp32-s3](https://wokwi.com/projects/new/esp32-s3)（免费可运行，保存需登录）
2. 删除模板 sketch.ino，新建 `Ollie.ino` + 15 个模块文件（平铺，清单见 [wokwi/README.md](../wokwi/README.md)），内容从仓库复制
3. `diagram.json`、`libraries.txt` 用 wokwi/ 目录下的
4. 点 ▶ 运行，按验收清单逐项点按钮/拖光照滑杆
5. **已知坑**：部件覆盖层在 shadow DOM；判定 OLED 内容用固定取景窗口的裁剪特写（全页截图看不清）；串口监视器入口未查明

## 4. 光敏阈值标定规程（M5）

1. `Config.h` 置 `LIGHT_CALIBRATE 1`，烧录
2. 串口每 500ms 打印 `ADC=xxx（暗<800 亮>2000）`
3. 分别在**白天 / 傍晚 / 夜晚 / 手遮**四种环境各记 10 个读数
4. 定阈值原则：暗阈值 = "应入睡"环境的最高值再留余量；亮阈值 = "应醒来"环境的最低值再留余量；两阈值之间是迟滞带
5. 改 `LIGHT_DARK_ADC` / `LIGHT_BRIGHT_ADC`，`LIGHT_CALIBRATE` 回 0，同步 docs/tech-spec.md

## 5. OLED 排查规程（M1）

1. `Config.h` 置 `BOOT_I2C_SCAN 1`，烧录
2. 串口应出现 `[scan] 发现设备: 0x3C`；没有 → 查 SDA/SCL 是否接反、VCC 是否 3V3
3. 地址是 0x3D → 改 `OLED_ADDR`；多个设备 → 记录地址避免冲突
4. 排查完回 0

## 6. 里程碑验收（M0–M7 详情见 plan-v1.md 第四节）

每个里程碑流程：**实现 → 单测 → Wokwi 冒烟 →（有硬件则）实机验收 → devlog 记录 + TODO 勾选 → git 提交**。

M7 过夜长测：傍晚烧录正式配置，串口开日志过夜；早晨检查——
- `[stats]` 每分钟一行未中断
- `heap=` 数值稳定（持续下降 >10% 即有泄漏，先查 String/递归/未释放缓冲）
- `up=` 与实际时长一致（无重启/死机）
- 数值符合预期（睡了一夜精力应明显回升）

## 7. 调试开关速查（都在 Config.h）

| 开关 | 作用 |
|---|---|
| `DEBUG_FAST_DECAY` | 数值衰减 1点/秒，4 分钟看全程 |
| `LIGHT_CALIBRATE` | 500ms 打印光敏 ADC |
| `BOOT_I2C_SCAN` | 只跑 I2C 扫描 |
| `PET_WAKE_ENABLED` | 睡眠中抚摸唤醒彩蛋 |
| `OLLIE_DEBUG 0` | 关全部串口日志 |
