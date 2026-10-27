#include "ChatLogic.h"
#include <cctype>

namespace chat {

static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) b++;
    while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}

std::string systemPrompt(int food, int mood, int energy) {
    std::string s = "你是 Ollie，一只住在桌面上、由一块小 OLED 屏显示表情的电子宠物小生物。";
    s += "性格温顺黏人、有点贪吃，说话口语化、简短亲切（不超过50个字）。";
    s += "它当前的状态：饱食度 " + std::to_string(food) + "/100";
    s += "，心情 " + std::to_string(mood) + "/100";
    s += "，精力 " + std::to_string(energy) + "/100。";
    if (food < 30) s += "它现在很饿，回答时可以表现出想吃东西。";
    if (energy < 30) s += "它现在很困，回答时可以无精打采、打哈欠。";
    if (mood < 40) s += "它现在心情低落，回答时可以委屈一点，需要被哄。";
    s += "严格只输出一行 JSON：{\"reply\":\"你对用户说的话\",\"emotion\":\"happy|calm|hungry|sleepy\"}。";
    s += "emotion 根据对话气氛选择；不要输出 JSON 以外的任何文字、解释或代码围栏。";
    return s;
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    char buf[8];
    for (unsigned char c : s) {
        switch (c) {
        case '"':  o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n";  break;
        case '\r': o += "\\r";  break;
        case '\t': o += "\\t";  break;
        default:
            if (c < 0x20) { // 其余控制字符按 \u00XX
                snprintf(buf, sizeof(buf), "\\u%04X", c);
                o += buf;
            } else {
                o += (char)c; // UTF-8 原样透传
            }
        }
    }
    return o;
}

// 把一个 \uXXXX 码点按 UTF-8 追加到输出（含代理对）
static void appendCodepoint(std::string& o, unsigned cp) {
    if (cp >= 0xD800 && cp <= 0xDBFF) return;      // 高代理：等待低代理配对
    if (cp >= 0xDC00 && cp <= 0xDFFF) {            // 孤立低代理：忽略
        return;
    }
    if (cp < 0x80) o += (char)cp;
    else if (cp < 0x800) {
        o += (char)(0xC0 | (cp >> 6));
        o += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        o += (char)(0xE0 | (cp >> 12));
        o += (char)(0x80 | ((cp >> 6) & 0x3F));
        o += (char)(0x80 | (cp & 0x3F));
    } else {
        o += (char)(0xF0 | (cp >> 18));
        o += (char)(0x80 | ((cp >> 12) & 0x3F));
        o += (char)(0x80 | ((cp >> 6) & 0x3F));
        o += (char)(0x80 | (cp & 0x3F));
    }
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string jsonUnescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c != '\\') { o += c; continue; }
        if (++i >= s.size()) break;
        char e = s[i];
        switch (e) {
        case '"':  o += '"';  break;
        case '\\': o += '\\'; break;
        case '/':  o += '/';  break;
        case 'n':  o += '\n'; break;
        case 'r':  o += '\r'; break;
        case 't':  o += '\t'; break;
        case 'b':  o += '\b'; break;
        case 'f':  o += '\f'; break;
        case 'u': {
            if (i + 4 >= s.size()) return o; // 截断的 \u，丢弃剩余
            unsigned cp = 0;
            bool bad = false;
            for (int k = 1; k <= 4; k++) {
                int v = hexVal(s[i + k]);
                if (v < 0) { bad = true; break; }
                cp = cp * 16 + (unsigned)v;
            }
            if (bad) { o += '?'; break; }
            i += 4;
            // 高代理：尝试配低代理
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < s.size() &&
                s[i + 1] == '\\' && s[i + 2] == 'u') {
                unsigned lo = 0;
                bool loBad = false;
                for (int k = 3; k <= 6; k++) {
                    int v = hexVal(s[i + k]);
                    if (v < 0) { loBad = true; break; }
                    lo = lo * 16 + (unsigned)v;
                }
                if (!loBad && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    i += 6;
                }
            }
            if (cp >= 0xD800 && cp <= 0xDFFF) cp = '?'; // 未配对代理
            appendCodepoint(o, cp);
            break;
        }
        default: o += e; break;
        }
    }
    return o;
}

// 在 pos 处读取一个 JSON 字符串的原始转义内容（不含首尾引号），
// 返回结束引号位置；失败返回 npos
static size_t readRawString(const std::string& json, size_t start, std::string& rawOut) {
    size_t j = start;
    rawOut.clear();
    while (j < json.size()) {
        char c = json[j];
        if (c == '\\') {
            rawOut += c;
            if (j + 1 < json.size()) rawOut += json[j + 1];
            j += 2;
            continue;
        }
        if (c == '"') return j;
        rawOut += c;
        j++;
    }
    return std::string::npos;
}

bool findStringValue(const std::string& json, const std::string& key, std::string& out) {
    const std::string token = "\"" + key + "\"";
    size_t i = 0;
    while ((i = json.find(token, i)) != std::string::npos) {
        // 前一个字符必须是 { , [ 或空白，避免命中 "my_key" 这类子串
        char prev = (i == 0) ? '{' : json[i - 1];
        if (prev == '{' || prev == ',' || prev == '[') {
            size_t k = i + token.size();
            while (k < json.size() && std::isspace((unsigned char)json[k])) k++;
            if (k < json.size() && json[k] == ':') {
                k++;
                while (k < json.size() && std::isspace((unsigned char)json[k])) k++;
                if (k < json.size() && json[k] == '"') {
                    std::string raw;
                    size_t end = readRawString(json, k + 1, raw);
                    if (end == std::string::npos) return false;
                    out = jsonUnescape(raw);
                    return true;
                }
            }
        }
        i += token.size();
    }
    return false;
}

bool extractContent(const std::string& apiJson, std::string& contentOut) {
    return findStringValue(apiJson, "content", contentOut);
}

bool parseReply(const std::string& content, std::string& replyOut, Emotion& emoOut) {
    std::string s = trim(content);
    // 容忍 ```json ... ``` 围栏
    if (s.rfind("```", 0) == 0) {
        size_t nl = s.find('\n');
        s = (nl == std::string::npos) ? "" : s.substr(nl + 1);
        size_t fence = s.rfind("```");
        if (fence != std::string::npos) s = s.substr(0, fence);
        s = trim(s);
    }
    if (!s.empty() && s.front() == '{' && s.back() == '}') {
        std::string r, e;
        bool hasReply = findStringValue(s, "reply", r);
        bool hasEmo = findStringValue(s, "emotion", e);
        if (hasReply && !trim(r).empty()) {
            replyOut = trim(r);
            emoOut = emotionFromName(hasEmo ? e : "calm");
            return true;
        }
        return false;
    }
    // 模型没按 JSON 输出：整段当回复，情绪默认
    if (s.empty()) return false;
    replyOut = s;
    emoOut = Emotion::Calm;
    return true;
}

Emotion emotionFromName(const std::string& name) {
    if (name == "happy")  return Emotion::Happy;
    if (name == "hungry") return Emotion::Hungry;
    if (name == "sleepy") return Emotion::Sleepy;
    return Emotion::Calm;
}

std::string buildRequestBody(const std::string& model, int food, int mood, int energy,
                             const std::string& userMsg) {
    std::string o = "{\"model\":\"" + jsonEscape(model) + "\",\"messages\":[";
    o += "{\"role\":\"system\",\"content\":\"" + jsonEscape(systemPrompt(food, mood, energy)) + "\"},";
    o += "{\"role\":\"user\",\"content\":\"" + jsonEscape(userMsg) + "\"}],";
    o += "\"max_tokens\":120,\"temperature\":0.8}";
    return o;
}

} // namespace chat
