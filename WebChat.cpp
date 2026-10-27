#include "WebChat.h"
#include "LLMClient.h"
#include "NetConfig.h"
#include "Stats.h"
#include "Config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

// —— 页面（PROGMEM）——
static const char CHAT_HTML[] PROGMEM = R"html(<!DOCTYPE html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ollie</title><style>
body{font-family:sans-serif;background:#111;color:#eee;margin:0;max-width:640px;margin:0 auto;padding:10px}
#bar{font-size:13px;color:#9ab;padding:4px 0 10px}
#log{display:flex;flex-direction:column;gap:8px;padding-bottom:76px}
.b{max-width:78%;padding:8px 12px;border-radius:12px;white-space:pre-wrap;word-break:break-word}
.me{align-self:flex-end;background:#2a6f5a}.ol{align-self:flex-start;background:#333}
.sys{align-self:center;color:#889;font-size:12px}
#row{position:fixed;bottom:0;left:0;right:0;display:flex;gap:8px;padding:8px;background:#111}
#in{flex:1;padding:10px;border-radius:8px;border:1px solid #444;background:#1c1c1c;color:#eee}
button{padding:10px 16px;border:0;border-radius:8px;background:#2a6f5a;color:#fff}
a{color:#7dc;font-size:13px}</style></head><body>
<div id="bar">连接中… <a href="/settings">设置</a></div>
<div id="log"></div>
<div id="row"><input id="in" placeholder="和 Ollie 说点什么…"><button onclick="send()">发送</button></div>
<script>
const log=document.getElementById('log');
function add(cls,t){const d=document.createElement('div');d.className='b '+cls;d.textContent=t;log.appendChild(d);window.scrollTo(0,1e9);}
async function state(){try{const s=await(await fetch('/api/state')).json();
document.getElementById('bar').innerHTML='饱食 '+s.food+' · 心情 '+s.mood+' · 精力 '+s.energy+' · WiFi '+s.rssi+'dBm <a href="/settings">设置</a>';}catch(e){}}
setInterval(state,5000);state();
let busy=false;
async function send(){if(busy)return;const i=document.getElementById('in');const m=i.value.trim();if(!m)return;busy=true;i.value='';add('me',m);
try{const r=await(await fetch('/api/chat',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({message:m})})).json();
if(r.status!=='pending'){add('sys',r.error||'发送失败');busy=false;return;}
add('sys','思考中…');const th=log.lastChild;
for(;;){const j=await(await fetch('/api/result?id='+r.id)).json();
if(j.status==='done'){th.remove();add('ol',j.reply);break;}
if(j.status==='error'){th.remove();add('sys',j.error);break;}
await new Promise(res=>setTimeout(res,400));}}catch(e){add('sys','网络错误');}busy=false;}
document.getElementById('in').addEventListener('keydown',e=>{if(e.key==='Enter')send();});
add('sys','戳右上角"设置"填 API Key；回复只代表模型，Ollie 的表情会跟着情绪变');
</script></body></html>)html";

static const char SETTINGS_HTML[] PROGMEM = R"html(<!DOCTYPE html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ollie 设置</title><style>
body{font-family:sans-serif;background:#111;color:#eee;max-width:480px;margin:0 auto;padding:16px}
h2{color:#7dc} label{display:block;margin:10px 0 4px;font-size:14px;color:#9ab}
input{width:100%;box-sizing:border-box;padding:9px;border-radius:8px;border:1px solid #444;background:#1c1c1c;color:#eee}
button{margin-top:16px;width:100%;padding:12px;border:0;border-radius:8px;background:#2a6f5a;color:#fff;font-size:15px}
p{font-size:13px;color:#889}</style></head><body><h2>Ollie 设置</h2>
<form method="POST" action="/api/settings">
<label>WiFi 名称（留空=不修改）</label><input name="ssid" maxlength="32">
<label>WiFi 密码（留空=不修改）</label><input name="pass" type="password" maxlength="64">
<label>LLM 接口地址（不含 https://）</label><input name="host" placeholder="api.deepseek.com">
<label>接口路径</label><input name="path" placeholder="/chat/completions">
<label>API Key（留空=不修改）</label><input name="key" type="password" placeholder="sk-...">
<label>模型名</label><input name="model" placeholder="deepseek-chat">
<button type="submit">保存并重启</button>
<p>修改 WiFi 后 Ollie 会重启并重连。聊天入口：http://ollie.local</p></form></body></html>)html";

static const char SAVED_HTML[] PROGMEM = R"html(<!DOCTYPE html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ollie</title></head>
<body style="font-family:sans-serif;background:#111;color:#eee;text-align:center;padding-top:80px">
<h2>已保存 ✓</h2><p>Ollie 正在重启使配置生效，本页可以关闭。</p></body></html>)html";

void WebChat::begin(LLMClient* llm, NetConfig* net, Stats* stats) {
    llm_ = llm;
    net_ = net;
    stats_ = stats;
    server_.on("/", [this]() { server_.send_P(200, "text/html", CHAT_HTML); });
    server_.on("/settings", [this]() { handleSettingsPage(); });
    server_.on("/api/chat", HTTP_POST, [this]() { handleChat(); });
    server_.on("/api/result", [this]() { handleResult(); });
    server_.on("/api/state", [this]() { handleState(); });
    server_.on("/api/settings", HTTP_POST, [this]() { handleSettingsSave(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "not found"); });
}

void WebChat::start() {
    if (!active_) {
        server_.begin();
        active_ = true;
    }
}

void WebChat::stop() {
    if (active_) {
        server_.stop();
        active_ = false;
    }
}

void WebChat::update(uint32_t now) {
    if (rebootAt_ && (int32_t)(now - rebootAt_) >= 0) ESP.restart();
    if (active_) server_.handleClient();
}

void WebChat::sendJson(int code, const std::string& body) {
    server_.setContentLength(body.size());
    server_.send(code, "application/json", body.c_str());
}

void WebChat::handleChat() {
    std::string body = server_.arg("plain").c_str();
    std::string msg;
    if (!chat::findStringValue(body, "message", msg) || msg.empty()) {
        sendJson(400, "{\"status\":\"error\",\"error\":\"消息为空\"}");
        return;
    }
    if (msg.size() > 200) msg.resize(200);

    const StatsData& d = stats_->data();
    int32_t id = llm_->request(msg, d.food, d.mood, d.energy,
                               net_->apiHost_.c_str(), net_->apiPath_.c_str(),
                               net_->apiKey_.c_str(), net_->apiModel_.c_str());
    if (id < 0) {
        sendJson(409, "{\"status\":\"error\",\"error\":\"Ollie 正在想上一条，稍等一下\"}");
        return;
    }
    LOGF("[chat] #%lu 消息 %u 字节\n", (unsigned long)id, (unsigned)msg.size());
    sendJson(200, "{\"id\":" + std::to_string(id) + ",\"status\":\"pending\"}");
}

void WebChat::handleResult() {
    uint32_t want = (uint32_t)server_.arg("id").toInt();
    uint32_t seq; bool ok; std::string reply, err; chat::Emotion emo;
    if (!llm_->getResult(seq, ok, reply, err, emo) || seq != want) {
        sendJson(200, "{\"status\":\"pending\"}");
        return;
    }
    if (!ok) {
        sendJson(200, "{\"status\":\"error\",\"error\":\"" + chat::jsonEscape(err) + "\"}");
        return;
    }
    static const char* EMO_NAME[] = {"happy", "calm", "hungry", "sleepy"};
    sendJson(200, "{\"status\":\"done\",\"reply\":\"" + chat::jsonEscape(reply) +
                  "\",\"emotion\":\"" + EMO_NAME[(int)emo] + "\"}");
}

void WebChat::handleState() {
    const StatsData& d = stats_->data();
    long rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    sendJson(200, "{\"food\":" + std::to_string(d.food) +
                  ",\"mood\":" + std::to_string(d.mood) +
                  ",\"energy\":" + std::to_string(d.energy) +
                  ",\"rssi\":" + std::to_string(rssi) + "}");
}

void WebChat::handleSettingsPage() {
    server_.send_P(200, "text/html", SETTINGS_HTML);
}

void WebChat::handleSettingsSave() {
    net_->saveFromForm(server_.arg("ssid"), server_.arg("pass"), server_.arg("host"),
                       server_.arg("path"), server_.arg("key"), server_.arg("model"));
    server_.send_P(200, "text/html", SAVED_HTML);
    rebootAt_ = millis() + 2500; // 响应发出后再重启
}
