#pragma once
/*
 * WiFi/凭据管理（v2.0）：
 *  - 凭据与 LLM 配置存 NVS（namespace "ollie"）
 *  - 无凭据或连续连不上 → 开 "Ollie-Setup" 热点 + 配网页（带 captive DNS）
 *  - 有凭据 → STA 连接（非阻塞状态机 + 退避重连），连上后起 mDNS（ollie.local）
 *  注意：本类不持有 WebChat；联网状态变化由 Ollie.ino 编排（起停网页、心情灯提示）
 */
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "Config.h"

class NetConfig {
public:
    void begin();
    void update(uint32_t now); // 500ms 节拍调用（内部含门户 http/dns 处理，需更频触发也无碍）

    bool connected() const { return state_ == ST_CONNECTED; }
    bool provisioning() const { return state_ == ST_AP_PORTAL; }
    // 已保存过凭据（用于区分"没配过"和"配错了"）
    bool hasCreds() const { return wifiSsid_.length() > 0; }

    // 凭据与配置（loop 线程访问）
    String wifiSsid_, wifiPass_;
    String apiHost_ = LLM_DEFAULT_HOST, apiPath_ = LLM_DEFAULT_PATH;
    String apiKey_, apiModel_ = LLM_DEFAULT_MODEL;

    void loadPrefs();
    // 逐项保存：空串表示保持原值（设置页/配网页共用）
    void saveFromForm(const String& ssid, const String& pass, const String& host,
                      const String& path, const String& key, const String& model);

private:
    enum St { ST_IDLE, ST_AP_PORTAL, ST_CONNECTING, ST_CONNECTED };
    void startPortal();
    void stopPortal();
    void startConnect(uint32_t now);
    void handlePortalSave();

    St state_ = ST_IDLE;
    uint32_t connectStart_ = 0, retryAt_ = 0;
    int fails_ = 0;
    uint32_t rebootAt_ = 0; // 配置保存后定时重启
    DNSServer dns_;
    WebServer portal_{80};
    bool mdnsOn_ = false;
};
