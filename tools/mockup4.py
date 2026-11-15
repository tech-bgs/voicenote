"""화면 시안 4차 — 녹음 하나 열어 보기(내용) + 재생 화면. 3차 디자인 규칙 그대로.

실행: python3 tools/mockup4.py → design/v4_all.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_font as mf  # noqa: E402
from mockup2 import draw_text, text_c, text_r, text_width, to_display  # noqa: E402
from mockup3 import hint_bar, status_bar  # noqa: E402

W = H = 200
SCALE = 3
OUT = Path(__file__).resolve().parent.parent / "design"


def wrap(s, font, width):
    """글자 단위로 줄바꿈 (한글은 띄어쓰기 없이도 끊어도 됨). 띄어쓰기 앞에서 끊을 수 있으면 우선"""
    lines, cur = [], ""
    for ch in s:
        if text_width(font, cur + ch) > width:
            cut = cur.rfind(" ")
            if cut > len(cur) // 2:
                lines.append(cur[:cut])
                cur = cur[cut + 1 :] + ch
            else:
                lines.append(cur)
                cur = ch
        else:
            cur += ch
    if cur:
        lines.append(cur)
    return lines


def header(img, d, no, sub):
    draw_text(img, 10, 52, no, "KO26")
    text_r(img, 190, 50, sub, "KO18")


def screen_detail_pending():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "녹음 보기", "14:32")
    header(img, d, "#013", "5분 12초")
    draw_text(img, 10, 76, "11월 13일 오후 2:20", "KO18")
    d.line([10, 88, W - 10, 88], fill=0)
    text_c(img, 114, "아직 변환 전이에요", "KO18")
    text_c(img, 140, "와이파이에 연결하면", "KO18")
    text_c(img, 162, "글자로 바뀌어요", "KO18")
    hint_bar(img, d, "▲ 재생", "뒤로 ▼")
    return img


def screen_detail_text():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "녹음 보기", "14:32")
    header(img, d, "#012", "1분 47초")
    draw_text(img, 10, 76, "11월 13일 오전 9:05", "KO18")
    d.line([10, 88, W - 10, 88], fill=0)
    text = "다음 주 촬영은 화요일로 확정하고 스택찬 브리핑 문구를 조금 더 짧게 바꾸기로 했다. 배터리 케이스는 이번 주에 주문."
    y = 108
    lines = wrap(text, "KO18", W - 20)
    for i, ln in enumerate(lines[:3]):
        if i == 2 and len(lines) > 3:
            while text_width("KO18", ln + "…") > W - 20:
                ln = ln[:-1]
            ln += "…"
        draw_text(img, 10, y, ln, "KO18")
        y += 22
    hint_bar(img, d, "▲ 재생", "뒤로 ▼")
    return img


def screen_playing():
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    status_bar(img, d, "재생 중", "#012")
    # 큰 재생 표시
    cx, cy = 100, 72
    d.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], fill=0)
    d.rectangle([cx - 10, cy - 13, cx - 4, cy + 13], fill=1)  # 일시정지 모양(재생 중)
    d.rectangle([cx + 4, cy - 13, cx + 10, cy + 13], fill=1)
    # 진행 막대
    x0, x1, y = 14, W - 14, 128
    d.rounded_rectangle([x0, y - 4, x1, y + 4], radius=4, outline=0, width=2)
    p = 0.38
    d.rounded_rectangle([x0, y - 4, x0 + int((x1 - x0) * p), y + 4], radius=4, fill=0)
    draw_text(img, 14, 156, "0:41", "KO20")
    text_r(img, W - 14, 156, "1:47", "KO20")
    hint_bar(img, d, "▲ 멈춤", "뒤로 ▼")
    return img


SCREENS = [
    ("내용 보기 (변환 전)", screen_detail_pending),
    ("내용 보기 (변환됨)", screen_detail_text),
    ("재생 중", screen_playing),
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
    sheet.save(OUT / "v4_all.png")
    print("saved", OUT / "v4_all.png")


if __name__ == "__main__":
    main()
