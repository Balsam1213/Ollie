#include "NetConfig.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// 配网页（AP 模式）：保存后重启生效
static const char PORTAL_HTML[] PROGMEM = R"html(<!DOCTYPE html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ollie 配置</title><style>
body{font-family:sans-serif;background:#111;color:#eee;max-width:420px;margin:0 auto;padding:16px}
h2{color:#7dc} label{display:block;margin:10px 0 4px;font-size:14px;color:#9ab}
input{width:100%;box-sizing:border-box;padding:9px;border-radius:8px;border:1px solid #444;background:#1c1c1c;color:#eee}
button{margin-top:16px;width:100%;padding:12px;border:0;border-radius:8px;background:#2a6;color:#fff;font-size:15px}
p{font-size:13px;color:#889}</style></head><body><h2>Ollie 首次配置</h2>
<form method="POST" action="/save">
<label>WiFi 名称</label><input name="ssid" maxlength="32" required>
<label>WiFi 密码</label><input name="pass" type="password" maxlength="64" required>
<label>LLM 接口地址（不含 https://，可留空用默认）</label><input name="host" placeholder="api.deepseek.com">
<label>接口路径（可留空）</label><input name="path" placeholder="/chat/completions">
<label>API Key</label><input name="key" type="password" placeholder="sk-...">
<label>模型名（可留空）</label><input name="model" placeholder="deepseek-chat">
<button type="submit">保存并重启</button>
<p>保存后 Ollie 会重启并连接 WiFi。之后用同一 WiFi 的手机/电脑访问 http://ollie.local 即可聊天。</p>
</form></body></html>)html";

static const char PORTAL_OK_HTML[] PROGMEM = R"html(<!DOCTYPE html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ollie</title></head>
<body style="font-family:sans-serif;background:#111;color:#eee;text-align:center;padding-top:80px">
<h2>已保存 ✓</h2><p>Ollie 正在重启并连接 WiFi，本页可以关闭。</p></body></html>)html";

void NetConfig::begin() {
    loadPrefs();
    WiFi.persistent(false); // 凭据由我们自己存 NVS，不用 SDK 重复写
    WiFi.mode(WIFI_STA);
    if (hasCreds()) {
        startConnect(millis());
    } else {
        startPortal(); // 从没配过网 → 直接开配置热点
    }
}

void NetConfig::loadPrefs() {
    Preferences p;
    p.begin("ollie", true);
    wifiSsid_ = p.getString("wifi_ssid", "");
    wifiPass_ = p.getString("wifi_pass", "");
    apiHost_ = p.getString("api_host", LLM_DEFAULT_HOST);
    apiPath_ = p.getString("api_path", LLM_DEFAULT_PATH);
    apiKey_ = p.getString("api_key", "");
    apiModel_ = p.getString("api_model", LLM_DEFAULT_MODEL);
    p.end();
}

void NetConfig::saveFromForm(const String& ssid, const String& pass, const String& host,
                             const String& path, const String& key, const String& model) {
    Preferences p;
    p.begin("ollie", false);
    if (ssid.length()) p.putString("wifi_ssid", ssid);
    if (pass.length()) p.putString("wifi_pass", pass);
    if (host.length()) p.putString("api_host", host);
    if (path.length()) p.putString("api_path", path);
    if (key.length())  p.putString("api_key", key);
    if (model.length()) p.putString("api_model", model);
    p.end();
    loadPrefs();
    rebootAt_ = millis() + 2500; // 让响应先发出去
}

void NetConfig::startPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    dns_.start(53, "*", WiFi.softAPIP()); // captive：任意域名都指到自己
    portal_.on("/save", [this]() { handlePortalSave(); });
    portal_.on("/", [this]() { portal_.send_P(200, "text/html", PORTAL_HTML); });
    // 未匹配路径（含 captive 探测）统一重定向回配置页
    portal_.onNotFound([this]() {
        portal_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
        portal_.send(302, "text/plain", "");
    });
    portal_.begin();
    state_ = ST_AP_PORTAL;
    LOGF("[net] 配网热点已开启：%s 密码 %s  →  http://%s\n",
         AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
}

void NetConfig::stopPortal() {
    dns_.stop();
    portal_.stop();
    WiFi.softAPdisconnect(true);
}

void NetConfig::handlePortalSave() {
    saveFromForm(portal_.arg("ssid"), portal_.arg("pass"), portal_.arg("host"),
                 portal_.arg("path"), portal_.arg("key"), portal_.arg("model"));
    portal_.send_P(200, "text/html", PORTAL_OK_HTML);
}

void NetConfig::startConnect(uint32_t now) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid_.c_str(), wifiPass_.c_str());
    state_ = ST_CONNECTING;
    connectStart_ = now;
    LOGF("[net] 连接 %s …\n", wifiSsid_.c_str());
}

void NetConfig::update(uint32_t now) {
    if (rebootAt_ && (int32_t)(now - rebootAt_) >= 0) {
        ESP.restart();
    }

    switch (state_) {
    case ST_AP_PORTAL:
        dns_.processNextRequest();
        portal_.handleClient();
        break;

    case ST_CONNECTING:
        if (WiFi.status() == WL_CONNECTED) {
            state_ = ST_CONNECTED;
            fails_ = 0;
            LOGF("[net] 已连接，IP %s\n", WiFi.localIP().toString().c_str());
            if (!mdnsOn_ && MDNS.begin("ollie")) {
                MDNS.addService("http", "tcp", 80);
                mdnsOn_ = true;
            }
        } else if ((uint32_t)(now - connectStart_) > WIFI_CONNECT_TIMEOUT_MS) {
            fails_++;
            WiFi.disconnect();
            LOGF("[net] 连接超时（第 %d 次）\n", fails_);
            if (fails_ >= PORTAL_FAILS_BEFORE_AP) {
                startPortal(); // 密码错了也能补救
            } else {
                retryAt_ = now + WIFI_RETRY_MS;
                state_ = ST_IDLE;
            }
        }
        break;

    case ST_IDLE:
        if (hasCreds() && (int32_t)(now - retryAt_) >= 0) startConnect(now);
        break;

    case ST_CONNECTED:
        if (WiFi.status() != WL_CONNECTED) {
            state_ = ST_IDLE;
            retryAt_ = now + WIFI_RETRY_MS;
            if (mdnsOn_) { MDNS.end(); mdnsOn_ = false; }
            LOGF("[net] WiFi 断开，稍后重连\n");
        }
        break;
    }
}
