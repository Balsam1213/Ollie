#pragma once
/*
 * LLM 调用（v2.0）：独立的 FreeRTOS 任务跑 HTTPS 请求，loop 只做投递和收结果，
 * 渲染帧率不受 1-3 秒的联网延迟影响。
 * 协议：OpenAI 兼容 /chat/completions；凭据与数值快照由 WebChat 在投递时一并传入
 * （那些数据只在 loop 线程读取，避免跨线程访问）。结果保留到下一次请求。
 */
#include <Arduino.h>
#include "Config.h"
#include "ChatLogic.h"

class Face;

class LLMClient {
public:
    void begin(Face* face); // 创建聊天任务（core 0）
    // 投递一条消息；忙时返回 -1。凭据与数值快照由调用方（WebChat，loop 线程）传入
    int32_t request(const std::string& userMsg, int food, int mood, int energy,
                    const std::string& host, const std::string& path,
                    const std::string& apiKey, const std::string& model);
    bool busy();
    bool hasResult();
    uint32_t lastSeq() const { return seq_; }
    // 取最近一次结果（不清除；seq 用于网页对应请求 id）
    bool getResult(uint32_t& seq, bool& ok, std::string& reply,
                   std::string& error, chat::Emotion& emo);
    void poll(uint32_t now); // 每帧调用：有新结果则驱动表情

private:
    static void taskTramp(void* arg);
    void taskLoop();
    void performHttp(const std::string& userMsg, int food, int mood, int energy,
                     const std::string& host, const std::string& path,
                     const std::string& apiKey, const std::string& model,
                     bool& ok, std::string& reply, std::string& error,
                     chat::Emotion& emo);

    Face* face_ = nullptr;
    SemaphoreHandle_t mtx_ = nullptr;
    TaskHandle_t task_ = nullptr;

    // 共享状态（一律在 mtx_ 内读写；HTTP 在锁外执行）
    bool pending_ = false;   // 已投递待任务处理
    uint32_t seq_ = 0;       // 请求计数，网页用它对应结果
    std::string msg_, host_, path_, key_, model_;
    int food_ = 0, mood_ = 0, energy_ = 0;
    bool hasResult_ = false;
    uint32_t resultSeq_ = 0;
    bool resultOk_ = false;
    std::string resultReply_, resultErr_;
    chat::Emotion resultEmo_ = chat::Emotion::Calm;
    uint32_t faceAppliedSeq_ = 0; // poll 已应用到表情的序号
};
