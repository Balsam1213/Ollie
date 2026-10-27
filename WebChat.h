#pragma once
/*
 * 网页聊天（v2.0）：WiFi 连上后起局域网 Web 服务（80 端口，mDNS: http://ollie.local）
 *   GET  /              聊天页（手机优先，气泡式，轮询取结果，全程不阻塞固件）
 *   GET  /settings      设置页：WiFi 与 LLM 端点/Key（保存后重启生效）
 *   POST /api/chat      {message} → {id, status:"pending"}；忙时 409
 *   GET  /api/result?id → pending | {status:"done",reply,emotion} | {status:"error",error}
 *   GET  /api/state     数值快照 + WiFi 强度（页面顶栏）
 *   POST /api/settings  表单保存（空=保持原值）→ 重启
 * WiFi 断开时 stop()，重连后 start()；停止期间路由不响应。
 */
#include <Arduino.h>
#include <WebServer.h>
#include "ChatLogic.h"

class LLMClient;
class NetConfig;
class Stats;

class WebChat {
public:
    void begin(LLMClient* llm, NetConfig* net, Stats* stats);
    void start();              // WiFi 就绪后调用一次
    void stop();               // WiFi 掉线时调用
    void update(uint32_t now); // 调度器 20ms 节拍：handleClient + 设置保存后的定时重启

private:
    void handleChat();
    void handleResult();
    void handleState();
    void handleSettingsPage();
    void handleSettingsSave();
    void sendJson(int code, const std::string& body);

    WebServer server_{80};
    LLMClient* llm_ = nullptr;
    NetConfig* net_ = nullptr;
    Stats* stats_ = nullptr;
    bool active_ = false;
    uint32_t rebootAt_ = 0;
};
