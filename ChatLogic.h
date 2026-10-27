#pragma once
/*
 * 聊天纯逻辑（ChatLogic）：不依赖任何 Arduino 类型，PC 单测与固件共用同一份实现。
 * 职责：人设 prompt 组装、JSON 转义/提取、模型回复解析、情绪名映射。
 */
#include <string>
#include <cstdint>

namespace chat {

enum class Emotion : uint8_t { Happy, Calm, Hungry, Sleepy };

// 系统提示词：人设 + 当前数值快照 + 严格 JSON 输出契约
std::string systemPrompt(int food, int mood, int energy);

// JSON 字符串转义 / 还原（\uXXXX 按 UTF-8 编码）
std::string jsonEscape(const std::string& s);
std::string jsonUnescape(const std::string& s);

// 在任意 JSON 里找 "key" 对应的字符串值；扫描跳过整段字符串，
// 因此 key 名不会误命中其他字段值内部的 `"key"` 文本（如 reasoning_content）
bool findStringValue(const std::string& json, const std::string& key, std::string& out);

// 从 /chat/completions 响应里取 choices[0].message.content
bool extractContent(const std::string& apiJson, std::string& contentOut);

// 解析模型回复内容：要求 {"reply":"..","emotion":".."}；容忍 ```json 围栏；
// emotion 缺省 calm；reply 为空则失败
bool parseReply(const std::string& content, std::string& replyOut, Emotion& emoOut);

Emotion emotionFromName(const std::string& name);

// 组装 /chat/completions 请求体
std::string buildRequestBody(const std::string& model, int food, int mood, int energy,
                             const std::string& userMsg);

} // namespace chat
