#pragma once
/*
 * NVS(Preferences) 桩：进程内全局存储。
 * 同一进程里新建的 Preferences 实例共享底层数据，恰好模拟"断电重启后数值仍在"。
 */
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
public:
    bool begin(const char*, bool) { return true; }
    bool isKey(const char* k) { return store().count(k) != 0; }
    size_t getBytes(const char* k, void* buf, size_t len) {
        auto it = store().find(k);
        if (it == store().end() || it->second.size() != len) return 0;
        std::memcpy(buf, it->second.data(), len);
        return len;
    }
    void putBytes(const char* k, const void* buf, size_t len) {
        store()[k].assign((const uint8_t*)buf, (const uint8_t*)buf + len);
    }

private:
    static std::map<std::string, std::vector<uint8_t>>& store() {
        static std::map<std::string, std::vector<uint8_t>> s;
        return s;
    }
};
