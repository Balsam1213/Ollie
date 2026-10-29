#!/usr/bin/env python
"""
Ollie 台词字库生成器
扫描 DialogLines.h 里的全部字符串，提取用到的每个字符（含 ASCII/颜文字符号），
用本机中文字体渲染成 12x12 点阵，输出 DialogFont.h（按码点排序，Face 二分查找）。

用法：py tools/genfont.py        （依赖：py -m pip install pillow）
改台词后重跑一次即可，台词与字库永不失配。
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "DialogLines.h"
OUT = ROOT / "DialogFont.h"
PX = 12  # 字形尺寸

# 候选中文字体（按序尝试），Windows 自带
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\msyh.ttc",   # 微软雅黑
    r"C:\Windows\Fonts\simhei.ttf", # 黑体
    r"C:\Windows\Fonts\simsun.ttc", # 宋体
]


def main() -> int:
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("缺少 Pillow：py -m pip install pillow")
        return 1

    text = SRC.read_text(encoding="utf-8")
    chars: set[str] = set()
    for m in re.finditer(r'"([^"\n]*)"', text):
        chars.update(m.group(1))
    if not chars:
        print("错误：DialogLines.h 里没扫到任何字符串")
        return 1

    font_path = next((p for p in FONT_CANDIDATES if Path(p).exists()), None)
    if not font_path:
        print("错误：找不到中文字体（msyh/simhei/simsun），请把字体路径加进 FONT_CANDIDATES")
        return 1
    font = ImageFont.truetype(font_path, PX)
    print(f"字体：{font_path}")

    glyphs = []
    empty = []
    for ch in sorted(chars):
        img = Image.new("L", (PX, PX), 0)
        d = ImageDraw.Draw(img)
        d.text((PX // 2, PX // 2), ch, fill=255, font=font, anchor="mm")
        px = img.load()
        bitmap = []
        for y in range(PX):
            hi = 0  # x 0..7（MSB first）
            lo = 0  # x 8..11（左对齐到高位）
            for x in range(PX):
                if px[x, y] > 128:
                    if x < 8:
                        hi |= 0x80 >> x
                    else:
                        lo |= 0x80 >> (x - 8)
            bitmap += [hi, lo]
        if any(bitmap):
            glyphs.append((ord(ch), bitmap))
        else:
            empty.append(ch)

    if empty:
        print(f"警告：{len(empty)} 个字符渲染为空白（字体缺字形），已跳过：{''.join(empty)}")

    lines = [
        "#pragma once",
        "// 自动生成：py tools/genfont.py —— 手改会被覆盖；改台词请编辑 DialogLines.h 后重新生成",
        "#include <cstdint>",
        "#include <cstddef>",
        "namespace dialogfont {",
        "struct Glyph { uint32_t cp; uint8_t bitmap[24]; }; // 12x12：每行 2 字节，行内 MSB first",
        "inline const Glyph GLYPHS[] = {",
    ]
    for cp, bm in glyphs:
        hexes = ",".join(f"0x{b:02X}" for b in bm)
        lines.append(f"    {{0x{cp:X},{{{hexes}}}}},")
    lines += [
        "};",
        f"inline const size_t N_GLYPHS = sizeof(GLYPHS) / sizeof(GLYPHS[0]);",
        "// 二分查字形；找不到返回 nullptr",
        "inline const Glyph* findGlyph(uint32_t cp) {",
        "    size_t lo = 0, hi = N_GLYPHS;",
        "    while (lo < hi) {",
        "        size_t mid = (lo + hi) / 2;",
        "        if (GLYPHS[mid].cp < cp) lo = mid + 1;",
        "        else if (GLYPHS[mid].cp > cp) hi = mid;",
        "        else return &GLYPHS[mid];",
        "    }",
        "    return nullptr;",
        "}",
        "} // namespace dialogfont",
    ]
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"OK：{len(glyphs)} 个字形 → {OUT.name}（约 {len(glyphs) * 28 // 1024 + 1} KB）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
