"""기기용 한글 글꼴 만들기 (맥에서 실행).

Noto Sans KR(OFL) 글꼴에서 자주 쓰는 한글 2350자 + 영문·숫자·기호를 뽑아
흑백 점 그림으로 바꾼 뒤 firmware/voicenote/font_ko.h 로 저장한다.

실행: python3 tools/make_font.py
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT_DIR = ROOT / "tools" / "fonts"
OUT = ROOT / "firmware" / "voicenote" / "font_ko.h"

# (이름, 글꼴 파일, 픽셀 크기)
SIZES = [
    # (이름, 글꼴 파일, 픽셀 크기, 굵기)
    ("KO18", "NotoSansKR[wght].ttf", 18, "Medium"),  # 보조 글자
    ("KO20", "NotoSansKR[wght].ttf", 20, "Medium"),  # 기본 글자
    ("KO26", "NotoSansKR[wght].ttf", 26, "Bold"),    # 강조 글자
    ("NUM48", "NotoSansKR[wght].ttf", 48, "Bold"),   # 녹음 시간 큰 숫자 (숫자와 : 만)
]
DIGITS_ONLY = {"NUM48"}


def charset():
    chars = [chr(c) for c in range(0x20, 0x7F)]
    # 완성형 한글 2350자 (KS X 1001)
    for hi in range(0xB0, 0xC9):
        for lo in range(0xA1, 0xFF):
            try:
                chars.append(bytes([hi, lo]).decode("euc-kr"))
            except UnicodeDecodeError:
                pass
    chars += list("·…→←↑↓●○▶◀▲▼›✓×")
    return sorted(set(chars), key=ord)


def rasterize(font, ch):
    """글자 하나 → (점 목록 2차원, 가로, 세로, x위치, 기준선 위 높이, 다음 글자까지 거리)"""
    adv = round(font.getlength(ch))
    bbox = font.getbbox(ch, anchor="ls")  # 기준선(baseline) 기준
    if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
        return None, 0, 0, 0, 0, adv
    x0, y0, x1, y1 = bbox
    w, h = x1 - x0, y1 - y0
    img = Image.new("1", (w, h), 0)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"  # 흑백 화면용으로 다듬어 그리기 (회색 없이 점 단위로 맞춤)
    d.text((-x0, -y0), ch, font=font, fill=1, anchor="ls")
    px = img.load()
    rows = [[1 if px[x, y] else 0 for x in range(w)] for y in range(h)]
    return rows, w, h, x0, y0, adv


def has_glyph(font, ch):
    # 글꼴에 없는 글자는 네모(.notdef)로 나오는데, 그걸 걸러낸다
    if ch == " ":
        return True
    try:
        return font.getmask(ch).getbbox() is not None
    except Exception:
        return False


def load_font(file, size, weight):
    font = ImageFont.truetype(str(FONT_DIR / file), size)
    if weight:
        font.set_variation_by_name(weight)
    return font


def build(name, file, size, weight, chars):
    font = load_font(file, size, weight)
    ascent, descent = font.getmetrics()
    notdef = rasterize(font, chr(0xFFFF))[0]
    glyphs, bits = [], bytearray()
    for ch in chars:
        if ch != " " and not has_glyph(font, ch):
            continue
        rows, w, h, xo, yo, adv = rasterize(font, ch)
        if ch != " " and rows == notdef:
            continue
        off = len(bits)
        if rows:
            acc, n = 0, 0
            for r in rows:
                for v in r:
                    acc = (acc << 1) | v
                    n += 1
                    if n == 8:
                        bits.append(acc)
                        acc, n = 0, 0
            if n:
                bits.append(acc << (8 - n))
        glyphs.append((ord(ch), w, h, xo, yo, adv, off))
    return {"name": name, "glyphs": glyphs, "bits": bits, "ascent": ascent, "height": ascent + descent}


def emit(fonts):
    lines = [
        "// 자동 생성 파일 — 직접 고치지 말 것. tools/make_font.py 로 다시 만든다.",
        "// 글꼴: Noto Sans KR (SIL Open Font License 1.1) — tools/fonts/NotoSansKR-OFL.txt",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "struct KGlyph {",
        "  uint16_t code;   // 유니코드",
        "  uint8_t w, h;    // 점 그림 크기",
        "  int8_t xo, yo;   // 기준선 기준 위치 (yo는 위쪽이 음수)",
        "  uint8_t adv;     // 다음 글자까지 거리",
        "  uint32_t off;    // 점 데이터 시작 위치",
        "};",
        "",
        "struct KFont {",
        "  const KGlyph *glyphs;",
        "  uint16_t count;",
        "  const uint8_t *bits;",
        "  uint8_t ascent;",
        "  uint8_t height;",
        "};",
        "",
    ]
    for f in fonts:
        n = f["name"]
        lines.append(f"static const KGlyph {n}_GLYPHS[] = {{")
        for code, w, h, xo, yo, adv, off in f["glyphs"]:
            lines.append(f"  {{0x{code:04X}, {w}, {h}, {xo}, {yo}, {adv}, {off}}},")
        lines.append("};")
        lines.append(f"static const uint8_t {n}_BITS[] = {{")
        b = f["bits"]
        for i in range(0, len(b), 24):
            lines.append("  " + ",".join(f"0x{v:02X}" for v in b[i : i + 24]) + ",")
        lines.append("};")
        lines.append(
            f"static const KFont {n} = {{{n}_GLYPHS, {len(f['glyphs'])}, {n}_BITS, {f['ascent']}, {f['height']}}};"
        )
        lines.append("")
    OUT.write_text("\n".join(lines))


def main():
    chars = charset()
    digits = list("0123456789: ")
    fonts = [build(n, file, s, wt, digits if n in DIGITS_ONLY else chars) for n, file, s, wt in SIZES]
    emit(fonts)
    for f in fonts:
        print(f"{f['name']}: 글자 {len(f['glyphs'])}개, 점 데이터 {len(f['bits']) // 1024}KB")
    print("saved", OUT)


if __name__ == "__main__":
    main()
