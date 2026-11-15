"""화면 시안 5차 — 전송 모드 (아래 버튼 길게 → 와이파이 → 변환 → 접속 안내). 3차 디자인 규칙 그대로.

실행: (PIL 있는 파이썬) tools/mockup5.py → design/v5_all.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_font as mf  # noqa: E402
from mockup2 import draw_text, text_c, text_r, to_display  # noqa: E402
from mockup3 import hint_bar, status_bar  # noqa: E402

W = H = 200
SCALE = 3
OUT = Path(__file__).resolve().parent.parent / "design"


def check(d, x, y):
    for t in range(3):
        d.line([x, y + 5 + t, x + 4, y + 9 + t], fill=0)
        d.line([x + 4, y + 9 + t, x + 13, y + t], fill=0)


def step_list(img, d, steps):
    y = 50
    for state, label in steps:
        if state == "ok":
            check(d, 12, y - 13)
        elif state == "run":
            d.polygon([(13, y - 14), (13, y - 2), (23, y - 8)], fill=0)
        else:
            d.ellipse([13, y - 13, 23, y - 3], outline=0, width=2)
        draw_text(img, 32, y, label, "KO18")
        y += 26


def screen_progress():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "전송 모드", "14:32")
    step_list(
        img,
        d,
        [
            ("ok", "와이파이: 휴대폰"),
            ("run", "글자로 변환 2/3"),
            ("todo", "스택찬에 알리기"),
            ("todo", "폰에서 보기 준비"),
        ],
    )
    # 진행 막대 (지금 변환 중인 녹음)
    x0, x1, y = 32, W - 12, 158
    d.rounded_rectangle([x0, y - 4, x1, y + 4], radius=4, outline=0, width=2)
    d.rounded_rectangle([x0, y - 4, x0 + int((x1 - x0) * 0.55), y + 4], radius=4, fill=0)
    hint_bar(img, d, "", "그만 ▼")
    return img


def screen_ready():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "전송 모드", "14:32")
    text_c(img, 46, "폰에서 열어 보세요", "KO18")
    d.rounded_rectangle([8, 54, W - 9, 116], radius=8, fill=0)
    text_c(img, 80, "voicenote.local", "KO20", color=1)
    text_c(img, 108, "172.20.10.3", "KO26", color=1)
    text_c(img, 140, "새로 변환 3개", "KO18")
    text_c(img, 162, "5분 뒤 저절로 잠들어요", "KO18")
    hint_bar(img, d, "", "끝내기 ▼")
    return img


def screen_nowifi():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "전송 모드", "14:32")
    text_c(img, 70, "와이파이를 못 찾았어요", "KO20")
    text_c(img, 104, "집 와이파이도", "KO18")
    text_c(img, 128, "휴대폰 핫스팟도 안 잡혀요", "KO18")
    text_c(img, 156, "녹음은 그대로 있어요", "KO18")
    hint_bar(img, d, "▲ 다시", "끝내기 ▼")
    return img


SCREENS = [
    ("변환 중", screen_progress),
    ("폰에서 열기 안내", screen_ready),
    ("와이파이 없음", screen_nowifi),
]


def main():
    tiles = [(label, to_display(fn())) for label, fn in SCREENS]
    pad, lh = 30, 40
    tw = th = W * SCALE
    sheet = Image.new("L", (len(tiles) * (tw + pad) + pad, th + lh + 2 * pad), 255)
    sd = ImageDraw.Draw(sheet)
    lf = mf.load_font("NotoSansKR[wght].ttf", 26, "Bold")
    for i, (label, im) in enumerate(tiles):
        x = pad + i * (tw + pad)
        sd.text((x, pad), f"{i + 1}. {label}", font=lf, fill=0)
        sheet.paste(im, (x, pad + lh))
        sd.rectangle([x - 1, pad + lh - 1, x + tw, pad + lh + th], outline=120)
    sheet.save(OUT / "v5_all.png")
    print("saved", OUT / "v5_all.png")


if __name__ == "__main__":
    main()
