"""화면 시안 2차 — 기기와 같은 글꼴(점 그림)로 그린다.

실행: python3 tools/mockup2.py → design/v2_*.png, design/v2_all.png
화면 배치 숫자는 firmware/voicenote/screen.cpp 와 맞춘다.
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_font as mf  # noqa: E402

W = H = 200
SCALE = 3
OUT = Path(__file__).resolve().parent.parent / "design"

FONTS = {n: mf.load_font(f, s, wt) for n, f, s, wt in mf.SIZES}
_cache = {}


def glyph(fname, ch):
    key = (fname, ch)
    if key not in _cache:
        _cache[key] = mf.rasterize(FONTS[fname], ch)
    return _cache[key]


def text_width(fname, s):
    return sum(glyph(fname, ch)[5] for ch in s)


def draw_text(img, x, y_base, s, fname, color=0):
    px = img.load()
    for ch in s:
        rows, w, h, xo, yo, adv = glyph(fname, ch)
        if rows:
            for yy in range(h):
                for xx in range(w):
                    if rows[yy][xx]:
                        X, Y = x + xo + xx, y_base + yo + yy
                        if 0 <= X < W and 0 <= Y < H:
                            px[X, Y] = color
        x += adv
    return x


def text_c(img, y, s, f, color=0):
    draw_text(img, (W - text_width(f, s)) // 2, y, s, f, color)


def text_r(img, xr, y, s, f, color=0):
    draw_text(img, xr - text_width(f, s), y, s, f, color)


def battery(d, x, y, pct):
    d.rectangle([x, y, x + 20, y + 10], outline=0)
    d.rectangle([x + 21, y + 3, x + 22, y + 7], fill=0)
    w = 17 * pct // 100
    if w:
        d.rectangle([x + 2, y + 2, x + 1 + w, y + 8], fill=0)


def status_bar(img, d, center=None):
    draw_text(img, 6, 19, "14:32", "KO20")
    if center:
        text_c(img, 19, center, "KO20")
    battery(d, 172, 6, 80)
    d.line([0, 24, W, 24], fill=0)


def hint_bar(img, d, left, right):
    d.line([0, 170, W, 170], fill=0)
    if left:
        draw_text(img, 6, 193, left, "KO20")
    if right:
        text_r(img, 194, 193, right, "KO20")


def mic(d, cx, cy):
    # 작은 마이크 (약 34x46)
    d.rounded_rectangle([cx - 11, cy - 20, cx + 11, cy + 6], radius=11, fill=0)
    d.arc([cx - 18, cy - 14, cx + 18, cy + 14], 20, 160, fill=0, width=3)
    d.line([cx, cy + 14, cx, cy + 22], fill=0, width=3)
    d.line([cx - 9, cy + 23, cx + 9, cy + 23], fill=0, width=3)


def screen_main():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d)
    mic(d, 100, 56)
    text_c(img, 106, "VOICE NOTE", "KO20")
    text_c(img, 140, "녹음 12개", "KO26")
    text_c(img, 162, "새 글 2개", "KO20")
    hint_bar(img, d, "위: 녹음", "아래: 목록")
    return img


def screen_list(sel=0):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, center=f"{sel + 1}/12")
    rows = [
        ("#013", "새 글", "11/13 14:20 · 5분"),
        ("#012", "변환됨", "11/13 09:05 · 1분"),
        ("#011", "변환 전", "11/12 21:40 · 12분"),
    ]
    y = 26
    for i, (no, st, sub) in enumerate(rows):
        on = i == sel
        color = 1 if on else 0
        if on:
            d.rectangle([0, y, W - 1, y + 46], fill=0)
        draw_text(img, 8, y + 22, no, "KO26", color)
        text_r(img, 192, y + 21, st, "KO20", color)
        draw_text(img, 8, y + 42, sub, "KO20", color)
        if not on and i < 2 and sel != i + 1:
            d.line([8, y + 47, W - 8, y + 47], fill=0)
        y += 48
    hint_bar(img, d, "위: 선택", "아래: 다음")
    return img


SCREENS = [
    ("v2_main", "메인", screen_main),
    ("v2_list", "목록 (첫 줄 선택)", lambda: screen_list(0)),
    ("v2_list2", "목록 (아래 버튼 누른 뒤)", lambda: screen_list(1)),
]


def to_display(img):
    big = img.convert("L").resize((W * SCALE, H * SCALE), Image.NEAREST)
    return big.point(lambda v: 235 if v > 128 else 25)


def main():
    OUT.mkdir(exist_ok=True)
    tiles = []
    for name, label, fn in SCREENS:
        im = to_display(fn())
        im.save(OUT / f"{name}.png")
        tiles.append((label, im))
    pad, lh = 30, 40
    tw = th = W * SCALE
    sheet = Image.new("L", (len(tiles) * (tw + pad) + pad, th + lh + 2 * pad), 255)
    sd = ImageDraw.Draw(sheet)
    lf = ImageFont.truetype(str(mf.FONT_DIR / "NotoSansKR[wght].ttf"), 26)
    for i, (label, im) in enumerate(tiles):
        x = pad + i * (tw + pad)
        sd.text((x, pad), f"{i + 1}. {label}", font=lf, fill=0)
        sheet.paste(im, (x, pad + lh))
        sd.rectangle([x - 1, pad + lh - 1, x + tw, pad + lh + th], outline=120)
    sheet.save(OUT / "v2_all.png")
    print("saved", OUT / "v2_all.png")


if __name__ == "__main__":
    main()
