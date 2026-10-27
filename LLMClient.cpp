#include "LLMClient.h"
#include "Face.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>

void LLMClient::begin(Face* face) {
    face_ = face;
    mtx_ = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(taskTramp, "chat", CHAT_TASK_STACK,
                            this, 1, &task_, 0); // core 0，渲染在 core 1
}

int32_t LLMClient::request(const std::string& userMsg, int food, int mood, int energy,
                           const std::string& host, const std::string& path,
                           const std::string& apiKey, const std::string& model) {
    int32_t id = -1;
    if (xSemaphoreTake(mtx_, 0) == pdTRUE) {
        if (!pending_) {
            msg_ = userMsg;
            food_ = food;
            mood_ = mood;
            energy_ = energy;
            host_ = host;
            path_ = path;
            key_ = apiKey;
            model_ = model;
            seq_++;
            pending_ = true;
            id = (int32_t)seq_;
        }
        xSemaphoreGive(mtx_);
    }
    if (id > 0) xTaskNotifyGive(task_);
    return id; // 忙时 -1
}

bool LLMClient::busy() {
    bool b = false;
    if (xSemaphoreTake(mtx_, 0) == pdTRUE) {
        b = pending_;
        xSemaphoreGive(mtx_);
    }
    return b;
}

bool LLMClient::hasResult() {
    bool h = false;
    if (xSemaphoreTake(mtx_, 0) == pdTRUE) {
        h = hasResult_;
        xSemaphoreGive(mtx_);
    }
    return h;
}

bool LLMClient::getResult(uint32_t& seq, bool& ok, std::string& reply,
                          std::string& error, chat::Emotion& emo) {
    bool has = false;
    if (xSemaphoreTake(mtx_, portMAX_DELAY) == pdTRUE) {
        has = hasResult_;
        if (has) {
            seq = resultSeq_;
            ok = resultOk_;
            reply = resultReply_;
            error = resultErr_;
            emo = resultEmo_;
        }
        xSemaphoreGive(mtx_);
    }
    return has;
}

void LLMClient::poll(uint32_t now) {
    (void)now;
    uint32_t seq;
    bool ok, has;
    std::string reply, err;
    chat::Emotion emo;
    has = getResult(seq, ok, reply, err, emo);
    if (has && seq != faceAppliedSeq_) {
        faceAppliedSeq_ = seq;
        if (ok) {
            Emotion e = Emotion::Calm;
            switch (emo) {
            case chat::Emotion::Happy:  e = Emotion::Happy;  break;
            case chat::Emotion::Hungry: e = Emotion::Hungry; break;
            case chat::Emotion::Sleepy: e = Emotion::Sleepy; break;
            default:                    e = Emotion::Calm;   break;
            }
            face_->setExpression(e, CHAT_HOLD_MS);
        }
        // 失败不动表情，错误信息已透传给网页
    }
}

void LLMClient::taskTramp(void* arg) {
    static_cast<LLMClient*>(arg)->taskLoop();
}

void LLMClient::taskLoop() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        std::string msg, host, path, key, model;
        int food = 0, mood = 0, energy = 0;
        uint32_t seq;
        bool go = false;
        if (xSemaphoreTake(mtx_, portMAX_DELAY) == pdTRUE) {
            if (pending_) {
                msg = msg_; host = host_; path = path_; key = key_; model = model_;
                food = food_; mood = mood_; energy = energy_;
                seq = seq_;
                pending_ = false;
                go = true;
            }
            xSemaphoreGive(mtx_);
        }
        if (!go) continue;

        bool ok = false;
        std::string reply, error;
        chat::Emotion emo = chat::Emotion::Calm;
        performHttp(msg, food, mood, energy, host, path, key, model, ok, reply, error, emo);

        if (xSemaphoreTake(mtx_, portMAX_DELAY) == pdTRUE) {
            hasResult_ = true;
            resultSeq_ = seq;
            resultOk_ = ok;
            resultReply_ = reply;
            resultErr_ = error;
            resultEmo_ = emo;
            xSemaphoreGive(mtx_);
        }
    }
}

void LLMClient::performHttp(const std::string& userMsg, int food, int mood, int energy,
                            const std::string& host, const std::string& path,
                            const std::string& apiKey, const std::string& model,
                            bool& ok, std::string& reply, std::string& error,
                            chat::Emotion& emo) {
    ok = false;
    if (WiFi.status() != WL_CONNECTED) {
        error = "WiFi 未连接";
        return;
    }

    // TLS：桌宠在局域网发起请求，采用跳过证书校验的务实方案（详见 devlog 决策）；
    // 超时收紧，保证最坏情况也在 CHAT_TIMEOUT_MS 量级内给网页答复
    WiFiClientSecure tls;
    tls.setInsecure();
    tls.setTimeout(CHAT_TIMEOUT_MS);
    tls.setHandshakeTimeout(8);

    HTTPClient http;
    http.setTimeout(CHAT_TIMEOUT_MS);
    http.setConnectTimeout(8000);
    String url = String("https://") + host.c_str() + path.c_str();
    if (!http.begin(tls, url)) {
        error = "连接初始化失败";
        return;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + apiKey.c_str());

    std::string body = chat::buildRequestBody(model, food, mood, energy, userMsg);
    int code = http.POST(String(body.c_str()));
    if (code != 200) {
        error = "接口返回 " + std::to_string(code);
        if (code < 0) error += " (" + std::string(http.errorToString(code).c_str()) + ")";
        http.end();
        return;
    }
    String resp = http.getString();
    http.end();

    std::string content;
    if (!chat::extractContent(resp.c_str(), content)) {
        error = "响应格式异常";
        return;
    }
    if (!chat::parseReply(content, reply, emo)) {
        error = "回复内容为空";
        return;
    }
    ok = true;
}
