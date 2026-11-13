"""화면 시안 만들기 (맥에서 실행). 200x200 흑백 화면을 그림 파일로 만든다.

실행: python3 tools/mockup.py  → design/ 폴더에 PNG 저장
기기 글꼴은 아직 정하지 않았고, 시안은 맥 기본 한글 글꼴로 그린다.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W = H = 200
SCALE = 3
OUT = Path(__file__).resolve().parent.parent / "design"
FONT_PATH = "/System/Library/Fonts/AppleSDGothicNeo.ttc"
BLACK, WHITE = 0, 1


def font(size, bold=True):
    # ttc 안의 0번=Regular, 6번=Bold 근처. 없으면 0번 사용
    for idx in ([6, 0] if bold else [0]):
        try:
            return ImageFont.truetype(FONT_PATH, size, index=idx)
        except Exception:
            continue
    return ImageFont.load_default()


def canvas():
    img = Image.new("1", (W, H), WHITE)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"  # 흑백 화면처럼 계단 글자
    return img, d


def text_c(d, y, s, f, fill=BLACK, x0=0, x1=W):
    w = d.textlength(s, font=f)
    d.text(((x0 + x1 - w) / 2, y), s, font=f, fill=fill)


def battery(d, x, y, pct):
    d.rectangle([x, y, x + 20, y + 10], outline=BLACK)
    d.rectangle([x + 21, y + 3, x + 22, y + 7], fill=BLACK)
    fill_w = int(17 * pct / 100)
    if fill_w:
        d.rectangle([x + 2, y + 2, x + 2 + fill_w, y + 8], fill=BLACK)


def wifi(d, x, y, on=True):
    if not on:
        return
    for i, r in enumerate([10, 7, 4]):
        d.arc([x + 10 - r, y + 10 - r, x + 10 + r, y + 10 + r], 225, 315, fill=BLACK, width=2)
    d.ellipse([x + 9, y + 9, x + 11, y + 11], fill=BLACK)


def status_bar(d, time="14:32", pct=80, wifi_on=False, inverted=False):
    fg = WHITE if inverted else BLACK
    if inverted:
        d.rectangle([0, 0, W, 20], fill=BLACK)
    else:
        d.line([0, 20, W, 20], fill=BLACK)
    d.text((6, 2), time, font=font(14), fill=fg)
    if not inverted:
        battery(d, 172, 5, pct)
        wifi(d, 145, 2, wifi_on)


def mic(d, cx, cy, s=1.0):
    w, h = int(16 * s), int(28 * s)
    d.rounded_rectangle([cx - w, cy - h, cx + w, cy + int(6 * s)], radius=w, fill=BLACK)
    d.arc([cx - int(26 * s), cy - int(22 * s), cx + int(26 * s), cy + int(20 * s)], 20, 160, fill=BLACK, width=3)
    d.line([cx, cy + int(20 * s), cx, cy + int(32 * s)], fill=BLACK, width=3)
    d.line([cx - int(12 * s), cy + int(32 * s), cx + int(12 * s), cy + int(32 * s)], fill=BLACK, width=3)


def hint_bar(d, left, right):
    d.line([0, 178, W, 178], fill=BLACK)
    f = font(13)
    d.text((6, 181), left, font=f, fill=BLACK)
    w = d.textlength(right, font=f)
    d.text((W - 6 - w, 181), right, font=f, fill=BLACK)


# ---------- 화면들 ----------

def screen_main():
    img, d = canvas()
    status_bar(d)
    mic(d, 100, 78, 1.1)
    text_c(d, 122, "VOICE NOTE", font(20))
    text_c(d, 150, "녹음 12개 · 새 글 2개", font(13, bold=False))
    hint_bar(d, "위: 녹음", "아래: 목록")
    return img


def screen_recording():
    img, d = canvas()
    d.rectangle([0, 0, W, 20], fill=BLACK)
    d.ellipse([6, 5, 16, 15], fill=WHITE)
    d.text((22, 2), "녹음 중", font=font(14), fill=WHITE)
    d.text((150, 2), "#013", font=font(14), fill=WHITE)
    text_c(d, 40, "02:31", font(52))
    # 소리 크기 막대
    levels = [3, 6, 9, 14, 20, 26, 18, 12, 22, 30, 24, 16, 10, 7, 12, 19, 25, 15, 8, 5]
    for i, lv in enumerate(levels):
        x = 20 + i * 8
        d.rectangle([x, 140 - lv, x + 4, 140], fill=BLACK)
    text_c(d, 150, "SD 남은 공간 약 9시간", font(12, bold=False))
    hint_bar(d, "위: 멈춤", "")
    return img


def screen_list():
    img, d = canvas()
    status_bar(d)
    d.text((6, 24), "녹음 목록", font=font(15))
    d.text((150, 26), "1/12", font=font(12, bold=False))
    rows = [
        ("#013", "11/13 14:20", "05:12", "new"),
        ("#012", "11/13 09:05", "01:47", "done"),
        ("#011", "11/12 21:40", "12:03", "done"),
        ("#010", "11/12 18:11", "00:38", "wait"),
    ]
    y = 46
    for i, (no, date, length, st) in enumerate(rows):
        sel = i == 0
        if sel:
            d.rectangle([2, y, W - 3, y + 30], fill=BLACK)
        fg = WHITE if sel else BLACK
        d.text((8, y + 2), no, font=font(14), fill=fg)
        d.text((52, y + 3), date, font=font(13, bold=False), fill=fg)
        d.text((8, y + 16), length, font=font(12, bold=False), fill=fg)
        mark = {"new": "새 글", "done": "변환됨", "wait": "변환 전"}[st]
        w = d.textlength(mark, font=font(12, bold=False))
        d.text((W - 10 - w, y + 16), mark, font=font(12, bold=False), fill=fg)
        if not sel:
            d.line([8, y + 31, W - 8, y + 31], fill=BLACK)
        y += 32
    hint_bar(d, "위: 선택", "아래: 다음")
    return img


def screen_detail():
    img, d = canvas()
    status_bar(d)
    d.text((6, 24), "#013", font=font(18))
    d.text((70, 28), "11/13 14:20", font=font(13, bold=False))
    d.text((6, 48), "길이 05:12 · 변환됨", font=font(13, bold=False))
    d.line([6, 68, W - 6, 68], fill=BLACK)
    summary = ["요약", "· 다음 주 촬영 일정 확정", "· 스택찬 브리핑 문구 수정", "· 배터리 케이스 주문"]
    y = 74
    for i, s in enumerate(summary):
        d.text((8, y), s, font=font(14 if i == 0 else 13, bold=i == 0), fill=BLACK)
        y += 22
    hint_bar(d, "위: 재생", "아래: 뒤로")
    return img


def screen_transfer():
    img, d = canvas()
    status_bar(d, wifi_on=True)
    d.text((6, 24), "전송 모드", font=font(16))
    steps = [("ok", "와이파이: 휴대폰 핫스팟"), ("ok", "글자로 변환 2/2"), ("run", "요약 중…"), ("todo", "스택찬에 알리기")]
    y = 50
    for st, s in steps:
        if st == "ok":
            d.line([10, y + 8, 14, y + 12], fill=BLACK, width=2)
            d.line([14, y + 12, 21, y + 3], fill=BLACK, width=2)
        elif st == "run":
            d.polygon([(11, y + 2), (11, y + 14), (20, y + 8)], fill=BLACK)
        else:
            d.ellipse([11, y + 3, 20, y + 12], outline=BLACK)
        d.text((28, y), s, font=font(13, bold=st == "run"), fill=BLACK)
        y += 22
    # 접속 주소 상자
    d.rectangle([4, 142, W - 5, 174], outline=BLACK, width=2)
    text_c(d, 145, "voicenote.local", font(14))
    text_c(d, 160, "172.20.10.3", font(12, bold=False))
    hint_bar(d, "", "아래 길게: 끄기")
    return img


def screen_ready():
    img, d = canvas()
    status_bar(d, wifi_on=True)
    text_c(d, 30, "폰에서 열어 보세요", font(15))
    d.rectangle([10, 58, W - 11, 128], fill=BLACK)
    text_c(d, 66, "voicenote.local", font(18), fill=WHITE)
    text_c(d, 92, "172.20.10.3", font(18), fill=WHITE)
    text_c(d, 138, "새 글 2개 준비됨", font(14, bold=False))
    text_c(d, 156, "5분 뒤 자동으로 잠듦", font(12, bold=False))
    hint_bar(d, "", "아래 길게: 끄기")
    return img


SCREENS = [
    ("1_main", "메인", screen_main),
    ("2_recording", "녹음 중", screen_recording),
    ("3_list", "목록", screen_list),
    ("4_detail", "내용 보기", screen_detail),
    ("5_transfer", "전송 모드", screen_transfer),
    ("6_ready", "접속 안내", screen_ready),
]


def to_display(img):
    # 실제 화면 느낌: 살짝 회색 종이 + 확대
    big = img.convert("L").resize((W * SCALE, H * SCALE), Image.NEAREST)
    return big.point(lambda v: 235 if v > 128 else 25)


def main():
    OUT.mkdir(exist_ok=True)
    tiles = []
    for name, label, fn in SCREENS:
        im = to_display(fn())
        im.save(OUT / f"{name}.png")
        tiles.append((label, im))

    pad, label_h, cols = 30, 40, 3
    tw, th = W * SCALE, H * SCALE
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("L", (cols * (tw + pad) + pad, rows * (th + pad + label_h) + pad), 255)
    sd = ImageDraw.Draw(sheet)
    lf = font(26)
    for i, (label, im) in enumerate(tiles):
        r, c = divmod(i, cols)
        x = pad + c * (tw + pad)
        y = pad + r * (th + pad + label_h)
        sd.text((x, y), f"{i + 1}. {label}", font=lf, fill=0)
        sheet.paste(im, (x, y + label_h))
        sd.rectangle([x - 1, y + label_h - 1, x + tw, y + label_h + th], outline=120)
    sheet.save(OUT / "all.png")
    print("saved", OUT / "all.png")


if __name__ == "__main__":
    main()
