#pragma once
/*
 * 台词池（v1.2）：按场景分组的离线台词，整屏 12×12 显示。
 *
 * 改台词的完整流程：
 *   1. 编辑本文件的字符串（每句 ≤8 个字/字符，可含颜文字，别用英文双引号）
 *   2. 运行 py tools/genfont.py 重新生成 DialogFont.h
 *   3. py -m platformio run 编译烧录（单测会自动校验每句长度）
 *
 * 场景由 Dialog 驱动器选用；新增场景需同步 Dialog.h 的枚举与 Dialog.cpp 的池表。
 */

inline const char* const LINES_CALM[] = {
    "今天也要加油鸭",
    "陪我玩会儿嘛",
    "你在看我吗",
    "无聊中…",
    "(￣▽￣)",
    "陪我聊聊天",
    "发呆中…",
};
inline constexpr uint8_t N_CALM = sizeof(LINES_CALM) / sizeof(LINES_CALM[0]);

inline const char* const LINES_HUNGRY[] = {
    "肚子咕咕叫",
    "想吃小饼干",
    "饿到贴墙了",
    "喂我嘛喂我嘛",
};
inline constexpr uint8_t N_HUNGRY = sizeof(LINES_HUNGRY) / sizeof(LINES_HUNGRY[0]);

inline const char* const LINES_SLEEPY[] = {
    "眼皮好重…",
    "哈欠~~",
    "困得不行了",
};
inline constexpr uint8_t N_SLEEPY = sizeof(LINES_SLEEPY) / sizeof(LINES_SLEEPY[0]);

inline const char* const LINES_SAD[] = {
    "有点小失落…",
    "多陪陪我嘛",
    "别不理我嘛",
};
inline constexpr uint8_t N_SAD = sizeof(LINES_SAD) / sizeof(LINES_SAD[0]);

inline const char* const LINES_SLEEP[] = {
    "晚安啦~",
    "梦里有零食",
    "Zzz…晚安",
};
inline constexpr uint8_t N_SLEEP = sizeof(LINES_SLEEP) / sizeof(LINES_SLEEP[0]);

inline const char* const LINES_WAKE[] = {
    "早安！",
    "睡饱啦！",
    "天亮了呀",
};
inline constexpr uint8_t N_WAKE = sizeof(LINES_WAKE) / sizeof(LINES_WAKE[0]);

inline const char* const LINES_FEED[] = {
    "好吃！",
    "谢谢投喂~",
    "还想再来点",
};
inline constexpr uint8_t N_FEED = sizeof(LINES_FEED) / sizeof(LINES_FEED[0]);

inline const char* const LINES_PET[] = {
    "摸摸真舒服",
    "再摸一下嘛",
    "咕噜咕噜~",
};
inline constexpr uint8_t N_PET = sizeof(LINES_PET) / sizeof(LINES_PET[0]);

inline const char* const LINES_IDLE[] = {
    "喂？还有人吗",
    "你去哪玩了",
    "想你了…",
};
inline constexpr uint8_t N_IDLE = sizeof(LINES_IDLE) / sizeof(LINES_IDLE[0]);
