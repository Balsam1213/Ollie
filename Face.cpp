#include "Face.h"
#include "Config.h"

// —— 几何初始参数（128x64）：先用这套跑起来，再照着屏幕慢慢调 ——
static const int EYE_L_CX = 40, EYE_R_CX = 88, EYE_CY = 22;
static const int EYE_W = 24, EYE_H = 28;
static const int MOUTH_CX = 64, SMILE_CY = 44, FROWN_CY = 56;
static const int FONT_W = 12, FONT_H = 12;         // 台词字库字形尺寸
static const int DIALOG_LINE_CHARS = 8;            // 每行最多字数（8*12=96 ≤ 128）

bool Face::begin() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(400000);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return false;

    // M1 验收：开机先亮出名字
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(31, 24);
    display.print("Ollie");
    display.display();
    delay(800);

    uint32_t now = millis();
    nextBlink = now + random(2000, 6000);
    nextDart = now + random(3000, 8000);
    nextDrift = now + 30000;
    return true;
}

// —— 覆盖队列 ——

void Face::setBase(Emotion e) { base = e; }

void Face::setExpression(Emotion e, uint32_t holdMs) {
    Overlay o;
    o.kind = OvKind::Expression;
    o.emotion = e;
    o.dur = holdMs;
    pushOverlay(o, millis());
}

void Face::showDialog(const char* text, uint32_t holdMs) {
    Overlay o;
    o.kind = OvKind::Dialog;
    o.text = text;
    o.dur = holdMs;
    pushOverlay(o, millis());
}

void Face::pushOverlay(const Overlay& o, uint32_t now) {
    uint32_t start = now;
    if (qN_ > 0) { // 排在队列末尾之后
        const Overlay& last = q_[qN_ - 1];
        start = last.startAt + last.dur;
        if ((int32_t)(start - now) > 10000) return; // 积压保护：丢弃过远的请求
    }
    if (qN_ >= 2) qN_ = 1; // 满了丢弃旧的排队项，保最新的
    q_[qN_] = o;
    q_[qN_].startAt = start;
    qN_++;
}

void Face::expireOverlays(uint32_t now) {
    while (qN_ > 0 && (int32_t)(now - (q_[0].startAt + q_[0].dur)) >= 0) {
        for (uint8_t i = 1; i < qN_; i++) q_[i - 1] = q_[i];
        qN_--;
    }
}

const Face::Overlay* Face::activeOverlay(uint32_t now) const {
    return (qN_ > 0 && (int32_t)(now - q_[0].startAt) >= 0) ? &q_[0] : nullptr;
}

// —— 帧驱动 ——

void Face::update(uint32_t now) {
    if (now - lastFrame < FRAME_MS) return; // 限帧 ~30fps
    lastFrame = now;

    expireOverlays(now);
    const Overlay* ov = activeOverlay(now);
    Emotion e = (ov && ov->kind == OvKind::Expression) ? ov->emotion : base;

    if (!ov || ov->kind == OvKind::Expression) advanceAnim(now, e);
    render(now, e, ov);
}

void Face::advanceAnim(uint32_t now, Emotion e) {
    // 眨眼只对"睁眼"类表情有意义
    bool eyesOpen = (e == Emotion::Calm || e == Emotion::Hungry || e == Emotion::Sleepy);
    if (eyesOpen && !blinking && now >= nextBlink) {
        blinking = true;
        blinkStart = now;
    }
    // 眼珠乱瞟只在平静时（其他表情眼珠本来就不显眼）
    if (e == Emotion::Calm) {
        if (pupDx == 0 && now >= nextDart) {
            do { pupDx = random(-6, 7); } while (pupDx == 0); // 保证真的瞟了一下
            dartUntil = now + random(500, 1000);
            nextDart = now + random(3000, 8000);
        } else if (pupDx != 0 && now >= dartUntil) {
            pupDx = 0;
        }
    } else {
        pupDx = 0;
    }
    // 防灼伤漂移
    if (now >= nextDrift) {
        driftX = random(-1, 2);
        driftY = random(-1, 2);
        nextDrift = now + 30000;
    }
}

float Face::blinkFactor(uint32_t now) {
    if (!blinking) return 1.0f;
    uint32_t t = now - blinkStart;
    if (t < 60) return 1.0f - (t / 60.0f) * 0.95f;              // 闭上
    if (t < 120) return 0.05f;                                  // 闭住
    if (t < 180) return 0.05f + ((t - 120) / 60.0f) * 0.95f;    // 睁开
    blinking = false;                                           // 一次眨眼结束，排下一次
    nextBlink = now + random(2000, 6000);
    return 1.0f;
}

void Face::render(uint32_t now, Emotion e, const Overlay* ov) {
    if (ov && ov->kind == OvKind::Dialog) { // 台词：整屏文字，盖过一切
        renderDialog(ov->text);
        if (dimmed) { display.dim(false); dimmed = false; } // 台词期间保持亮度可读
        display.display();
        return;
    }

    float f = blinkFactor(now);
    int ox = driftX, oy = driftY;
    if (e == Emotion::Sleepy) oy += (int)((now / 700) % 2); // 困倦时整脸缓慢晃动

    display.clearDisplay();
    switch (e) {
    case Emotion::Calm:
        drawEyeOpen(EYE_L_CX + ox, EYE_CY + oy, f, pupDx);
        drawEyeOpen(EYE_R_CX + ox, EYE_CY + oy, f, pupDx);
        drawArcMouth(MOUTH_CX + ox, SMILE_CY + oy, 9, true);
        break;

    case Emotion::Happy:
        drawEyeHappyArc(EYE_L_CX + ox, EYE_CY + oy);
        drawEyeHappyArc(EYE_R_CX + ox, EYE_CY + oy);
        // 触发瞬间张嘴 2 帧，随后回到大弧线笑
        if (ov && ov->kind == OvKind::Expression && (uint32_t)(now - ov->startAt) < 150) {
            display.fillRoundRect(MOUTH_CX - 10 + ox, SMILE_CY - 2 + oy, 20, 8, 4, SSD1306_WHITE);
        } else {
            drawArcMouth(MOUTH_CX + ox, SMILE_CY + oy, 13, true);
        }
        break;

    case Emotion::Hungry:
        drawBrow(EYE_L_CX + ox, EYE_CY + oy, false);
        drawBrow(EYE_R_CX + ox, EYE_CY + oy, true);
        drawEyeOpen(EYE_L_CX + ox, EYE_CY + oy, 0.45f * f, 0);
        drawEyeOpen(EYE_R_CX + ox, EYE_CY + oy, 0.45f * f, 0);
        drawArcMouth(MOUTH_CX + ox, FROWN_CY + oy, 10, false);
        for (int i = 0; i < 3; i++) // 嘴角边三个小点（馋）
            display.fillCircle(MOUTH_CX + 16 + ox, 48 + 5 * i + oy, 1, SSD1306_WHITE);
        break;

    case Emotion::Sleepy:
        drawEyeOpen(EYE_L_CX + ox, EYE_CY + oy, 0.4f * f, 0);
        drawEyeOpen(EYE_R_CX + ox, EYE_CY + oy, 0.4f * f, 0);
        display.drawCircle(MOUTH_CX + ox, 46 + oy, 2, SSD1306_WHITE); // 打哈欠的 o 嘴
        break;

    case Emotion::Sleeping:
        drawEyeClosed(EYE_L_CX + ox, EYE_CY + oy);
        drawEyeClosed(EYE_R_CX + ox, EYE_CY + oy);
        drawZzz(now);
        break;
    }

    bool wantDim = (e == Emotion::Sleeping); // 睡眠时调暗，护屏也降低存在感
    if (wantDim != dimmed) {
        display.dim(wantDim);
        dimmed = wantDim;
    }
    display.display();
}

// —— 台词渲染 ——

// UTF-8 → 码点，s 前移；非法序列返回 0
static uint32_t utf8Next(const char*& s) {
    const unsigned char c = (unsigned char)*s;
    if (c == 0) return 0;
    if (c < 0x80) { s++; return c; }
    int len = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
    else { s++; return 0; }
    s++;
    for (int i = 1; i < len; i++) {
        unsigned char cc = (unsigned char)*s;
        if ((cc & 0xC0) != 0x80) { return 0; }
        cp = (cp << 6) | (cc & 0x3F);
        s++;
    }
    return cp;
}

void Face::renderDialog(const char* text) {
    display.clearDisplay();
    if (!text || !*text) {
        display.display();
        return;
    }
    // 先数总字符数决定单行还是两行
    int total = 0;
    for (const char* p = text; *p; ) {
        if (utf8Next(p) == 0) break;
        total++;
    }
    if (total <= DIALOG_LINE_CHARS) {
        int x = (128 - FONT_W * total) / 2;
        const char* p = text;
        for (int i = 0; i < total && *p; i++) {
            uint32_t cp = utf8Next(p);
            const dialogfont::Glyph* g = cp ? dialogfont::findGlyph(cp) : nullptr;
            if (g) display.drawBitmap(x + i * FONT_W, 26, g->bitmap, FONT_W, FONT_H, SSD1306_WHITE);
        }
    } else {
        // 两行：前半/后半，都居中
        int n1 = (total + 1) / 2, n2 = total - n1;
        const char* p = text;
        // 第一行
        int x = (128 - FONT_W * n1) / 2;
        for (int i = 0; i < n1 && *p; i++) {
            uint32_t cp = utf8Next(p);
            const dialogfont::Glyph* g = cp ? dialogfont::findGlyph(cp) : nullptr;
            if (g) display.drawBitmap(x + i * FONT_W, 18, g->bitmap, FONT_W, FONT_H, SSD1306_WHITE);
        }
        // 第二行
        x = (128 - FONT_W * n2) / 2;
        for (int i = 0; i < n2 && *p; i++) {
            uint32_t cp = utf8Next(p);
            const dialogfont::Glyph* g = cp ? dialogfont::findGlyph(cp) : nullptr;
            if (g) display.drawBitmap(x + i * FONT_W, 34, g->bitmap, FONT_W, FONT_H, SSD1306_WHITE);
        }
    }
    display.display();
}

// —— 图元实现 ——

void Face::drawEyeOpen(int cx, int cy, float hScale, int pupDx) {
    int h = (int)(EYE_H * hScale);
    if (h < 4) h = 4;
    int r = (h < 12) ? h / 2 : 6;
    display.fillRoundRect(cx - EYE_W / 2, cy - h / 2, EYE_W, h, r, SSD1306_WHITE);
    if (hScale > 0.6f) { // 半闭眼时不画眼珠（hungry/sleepy/眨眼中）
        display.fillCircle(cx + pupDx, cy, 5, SSD1306_BLACK);
    }
}

void Face::drawEyeHappyArc(int cx, int cy) {
    for (int t = 0; t < 2; t++) { // 画两遍加粗
        display.drawLine(cx - 9, cy + 5 + t, cx, cy - 6 + t, SSD1306_WHITE);
        display.drawLine(cx, cy - 6 + t, cx + 9, cy + 5 + t, SSD1306_WHITE);
    }
}

void Face::drawEyeClosed(int cx, int cy) {
    // 下弯闭眼线（∪）
    display.drawCircleHelper(cx, cy, 8, 0x0C, SSD1306_WHITE);
}

void Face::drawArcMouth(int cx, int cy, int r, bool smile) {
    // 下半弧 = 笑（∪），上半弧 = 沮丧（∩）；画两圈 2px 粗
    uint8_t corner = smile ? 0x0C : 0x03;
    display.drawCircleHelper(cx, cy, r, corner, SSD1306_WHITE);
    display.drawCircleHelper(cx, cy, r - 1, corner, SSD1306_WHITE);
}

void Face::drawBrow(int cx, int cy, bool mirror) {
    // 眉角下压：从外上斜向内下
    int top = cy - EYE_H / 2;
    if (!mirror) display.drawLine(cx - 9, top - 6, cx + 7, top - 1, SSD1306_WHITE);
    else         display.drawLine(cx + 9, top - 6, cx - 7, top - 1, SSD1306_WHITE);
}

void Face::drawZzz(uint32_t now) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    for (int i = 0; i < 3; i++) {
        int y = 26 - ((now / 300 + (uint32_t)i * 7) % 20); // 从下往上飘、循环
        display.setCursor(104 + i * 8, y);
        display.print('Z');
    }
}
