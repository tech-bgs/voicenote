"""화면 시안 3차 — 1차 느낌(여백·강약) + 큰 글자. 기기와 같은 점 글꼴로 그린다.

실행: python3 tools/mockup3.py → design/v3_all.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_font as mf  # noqa: E402
from mockup2 import battery, draw_text, text_c, text_r, text_width, to_display  # noqa: E402,F401

W = H = 200
SCALE = 3
OUT = Path(__file__).resolve().parent.parent / "design"


def mic(d, cx, cy):
    # 1차 시안 크기의 마이크, 받침 고리는 끊김 없이 한 덩어리
    d.rounded_rectangle([cx - 15, cy - 30, cx + 15, cy + 8], radius=15, fill=0)
    d.pieslice([cx - 25, cy - 18, cx + 25, cy + 22], 0, 180, fill=0)
    d.pieslice([cx - 21, cy - 14, cx + 21, cy + 18], 0, 180, fill=1)
    d.rounded_rectangle([cx - 15, cy - 30, cx + 15, cy + 8], radius=15, fill=0)
    d.rectangle([cx - 25, cy + 1, cx - 21, cy + 2], fill=1)
    d.rectangle([cx - 1, cy + 20, cx + 1, cy + 30], fill=0)
    d.rectangle([cx - 11, cy + 30, cx + 11, cy + 32], fill=0)


def status_bar(img, d, left, right_text=None):
    draw_text(img, 6, 18, left, "KO18")
    x = 170
    if right_text:
        text_r(img, 164, 18, right_text, "KO18")
    battery(d, 172, 6, 80)
    d.line([0, 24, W, 24], fill=0)


def hint_bar(img, d, left, right):
    d.line([0, 170, W, 170], fill=0)
    draw_text(img, 6, 192, left, "KO20")
    text_r(img, 194, 192, right, "KO20")


def screen_main():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "VOICE NOTE")
    mic(d, 100, 70)
    text_c(img, 136, "녹음 4개", "KO26")
    text_c(img, 159, "새 글 1개", "KO18")
    hint_bar(img, d, "▲ 녹음", "목록 ▼")
    return img


def screen_list(sel):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "녹음 목록", f"{sel + 1}/4")
    rows = [
        ("#013", "새 글", "11/13 14:20 · 5분"),
        ("#012", "변환됨", "11/13 09:05 · 1분"),
        ("#011", "변환 전", "11/12 21:40 · 12분"),
    ]
    y = 26
    for i, (no, st, sub) in enumerate(rows):
        on = i == sel
        c = 1 if on else 0
        if on:
            d.rounded_rectangle([3, y + 1, W - 4, y + 45], radius=6, fill=0)
        draw_text(img, 12, y + 23, no, "KO26", c)
        text_r(img, 188, y + 21, st, "KO18", c)
        draw_text(img, 12, y + 41, sub, "KO18", c)
        y += 48
    hint_bar(img, d, "▲ 선택", "다음 ▼")
    return img


SCREENS = [
    ("지금 기기 (2차)", None),
    ("3차: 메인", screen_main),
    ("3차: 목록", lambda: screen_list(0)),
]


def main():
    from mockup2 import screen_main as v2_main

    tiles = []
    for label, fn in SCREENS:
        im = to_display((fn or v2_main)())
        tiles.append((label, im))
    pad, lh = 30, 40
    tw = th = W * SCALE
    sheet = Image.new("L", (len(tiles) * (tw + pad) + pad, th + lh + 2 * pad), 255)
    sd = ImageDraw.Draw(sheet)
    lf = ImageFont.truetype(str(mf.FONT_DIR / "Pretendard-Bold.otf"), 26)
    for i, (label, im) in enumerate(tiles):
        x = pad + i * (tw + pad)
        sd.text((x, pad), label, font=lf, fill=0)
        sheet.paste(im, (x, pad + lh))
        sd.rectangle([x - 1, pad + lh - 1, x + tw, pad + lh + th], outline=120)
    sheet.save(OUT / "v3_all.png")
    print("saved", OUT / "v3_all.png")


if __name__ == "__main__":
    main()
