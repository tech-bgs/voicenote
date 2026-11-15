"""화면 시안 6차 — 기기 목록 화면에 '▼ 길게: 메인' 안내 넣기 (지금 vs 바꾼 뒤)

실행: (PIL 있는 파이썬) tools/mockup6.py → design/v6_list_hint.png
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_font as mf  # noqa: E402
import mockup3  # noqa: E402
from mockup2 import to_display  # noqa: E402

W = H = 200
OUT = Path(__file__).resolve().parent.parent / "design"


def list_with_title(title):
    orig = mockup3.status_bar

    def patched(img, d, left, right_text=None):
        orig(img, d, title, right_text)

    mockup3.status_bar = patched
    try:
        return mockup3.screen_list(0)
    finally:
        mockup3.status_bar = orig


def main():
    tiles = [("지금", mockup3.screen_list(0)), ("바꾼 뒤", list_with_title("▼ 길게: 메인"))]
    pad, lh, tw = 30, 40, W * 3
    sheet = Image.new("L", (len(tiles) * (tw + pad) + pad, tw + lh + 2 * pad), 255)
    sd = ImageDraw.Draw(sheet)
    lf = mf.load_font("NotoSansKR[wght].ttf", 26, "Bold")
    for i, (label, im) in enumerate(tiles):
        x = pad + i * (tw + pad)
        sd.text((x, pad), label, font=lf, fill=0)
        sheet.paste(to_display(im), (x, pad + lh))
    sheet.save(OUT / "v6_list_hint.png")
    print("saved", OUT / "v6_list_hint.png")


if __name__ == "__main__":
    main()
