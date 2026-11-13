#include "screen.h"

#include <U8g2lib.h>

#include "epd.h"
#include "font_ko.h"

namespace screen {

// U8g2는 선·네모 그리는 도구로만 쓴다. 글자는 아래 drawK(Pretendard 점 글꼴)로 그린다.
static U8G2 g;
static uint8_t frame[epd::BUF_SIZE];

static uint8_t noopByte(u8x8_t *, uint8_t, uint8_t, void *) { return 1; }
static uint8_t noopGpio(u8x8_t *, uint8_t, uint8_t, void *) { return 1; }

void begin() {
  u8g2_Setup_ssd1607_200x200_f(g.getU8g2(), U8G2_R0, noopByte, noopGpio);
  epd::begin();
}

// U8g2 버퍼(세로 8픽셀 묶음, 1=검정) → 화면 칩 형식(가로 8픽셀 묶음, 1=흰색)
static void flush(bool partial) {
  const uint8_t *src = g.getBufferPtr();
  memset(frame, 0xFF, sizeof(frame));
  for (int y = 0; y < epd::HEIGHT; y++) {
    const uint8_t *row = src + (y / 8) * epd::WIDTH;
    const uint8_t bit = 1 << (y % 8);
    for (int x = 0; x < epd::WIDTH; x++) {
      if (row[x] & bit) frame[y * 25 + x / 8] &= ~(0x80 >> (x % 8));
    }
  }
  if (partial) {
    epd::showPartial(frame);
  } else {
    epd::showFull(frame);
  }
}

// ---------- 한글 글자 그리기 ----------

// UTF-8 한 글자 읽기
static uint16_t nextCode(const char *&p) {
  const uint8_t c = *p++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0) {
    const uint16_t v = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    return v;
  }
  if ((c & 0xF0) == 0xE0) {
    uint16_t v = (c & 0x0F) << 12;
    v |= (*p++ & 0x3F) << 6;
    v |= (*p++ & 0x3F);
    return v;
  }
  return '?';
}

static const KGlyph *findGlyph(const KFont &f, uint16_t code) {
  int lo = 0, hi = f.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint16_t c = f.glyphs[mid].code;
    if (c == code) return &f.glyphs[mid];
    if (c < code) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

static int widthK(const KFont &f, const char *s) {
  int w = 0;
  while (*s) {
    const KGlyph *gl = findGlyph(f, nextCode(s));
    w += gl ? gl->adv : f.height / 2;
  }
  return w;
}

// x: 왼쪽, y: 글자 기준선. color 1=검정, 0=흰색
static int drawK(int x, int y, const char *s, const KFont &f, uint8_t color = 1) {
  g.setDrawColor(color);
  while (*s) {
    const KGlyph *gl = findGlyph(f, nextCode(s));
    if (!gl) {
      x += f.height / 2;
      continue;
    }
    const uint8_t *bits = f.bits + gl->off;
    int bitIndex = 0;
    for (int yy = 0; yy < gl->h; yy++) {
      for (int xx = 0; xx < gl->w; xx++, bitIndex++) {
        if (bits[bitIndex >> 3] & (0x80 >> (bitIndex & 7))) {
          g.drawPixel(x + gl->xo + xx, y + gl->yo + yy);
        }
      }
    }
    x += gl->adv;
  }
  g.setDrawColor(1);
  return x;
}

static void textCenter(int y, const char *s, const KFont &f, uint8_t color = 1) {
  drawK((200 - widthK(f, s)) / 2, y, s, f, color);
}

static void textRight(int xRight, int y, const char *s, const KFont &f, uint8_t color = 1) {
  drawK(xRight - widthK(f, s), y, s, f, color);
}

// ---------- 공통 부품 ----------

static void battery(int x, int y, int pct) {
  g.drawFrame(x, y, 21, 11);
  g.drawBox(x + 21, y + 3, 2, 5);
  const int w = constrain(pct, 0, 100) * 17 / 100;
  if (w > 0) g.drawBox(x + 2, y + 2, w, 7);
}

// 맨 위 줄: 왼쪽 제목, 오른쪽 배터리 (그 왼쪽에 rightText)
static void statusBar(int batteryPct, const char *title, const char *rightText = nullptr) {
  drawK(6, 18, title, KO18);
  if (rightText) textRight(164, 18, rightText, KO18);
  battery(172, 6, batteryPct);
  g.drawHLine(0, 24, 200);
}

// 맨 아래 줄: 버튼 안내 (nullptr이면 안 씀)
static void hintBar(const char *left, const char *right) {
  g.drawHLine(0, 170, 200);
  if (left) drawK(6, 192, left, KO20);
  if (right) textRight(194, 192, right, KO20);
}

static void mic(int cx, int cy) {
  const int ringY = cy + 2;
  // 받침 고리: 아래 반원 두 개를 겹쳐서 끊김 없는 두께 4 고리
  g.drawDisc(cx, ringY, 25, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  g.setDrawColor(0);
  g.drawDisc(cx, ringY, 21, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  g.setDrawColor(1);
  g.drawRBox(cx - 15, cy - 30, 31, 39, 14); // 몸통
  g.drawBox(cx - 1, cy + 20, 3, 11);        // 기둥
  g.drawBox(cx - 11, cy + 30, 23, 3);       // 받침
}

// 녹음 길이: 1분 미만은 "N초", 그 외는 "N분"
static void lengthText(char *out, size_t n, int seconds) {
  if (seconds < 60) {
    snprintf(out, n, "%d초", seconds);
  } else {
    snprintf(out, n, "%d분", (seconds + 30) / 60);
  }
}

// ---------- 화면들 ----------

void showMain(int noteCount, int pendingCount, int batteryPct, const char *topHint) {
  g.clearBuffer();
  statusBar(batteryPct, "VOICE NOTE");
  mic(100, 70);
  char line[40];
  snprintf(line, sizeof(line), "녹음 %d개", noteCount);
  textCenter(136, line, KO26);
  if (pendingCount > 0) {
    snprintf(line, sizeof(line), "변환 전 %d개", pendingCount);
  } else {
    snprintf(line, sizeof(line), "모두 변환됨");
  }
  textCenter(159, line, KO18);
  hintBar(topHint, "목록 ▼");
  flush(false);
}

void showList(const NoteRow *rows, int total, int selected, int batteryPct, bool partial, const char *topHint) {
  g.clearBuffer();
  char pos[16];
  snprintf(pos, sizeof(pos), "%d/%d", selected + 1, total);
  statusBar(batteryPct, "녹음 목록", pos);

  // 한 화면에 3줄. 선택된 줄이 들어 있는 3줄 묶음을 보여 준다
  const int first = (selected / 3) * 3;
  int y = 26;
  for (int i = first; i < total && i < first + 3; i++) {
    const NoteRow &r = rows[i];
    const bool sel = i == selected;
    const uint8_t color = sel ? 0 : 1;
    if (sel) g.drawRBox(3, y + 1, 194, 45, 6);

    char no[8];
    snprintf(no, sizeof(no), "#%03d", r.number);
    drawK(12, y + 23, no, KO26, color);
    textRight(188, y + 21, r.converted ? "✓" : "변환 전", KO18, color);

    char len[12], sub[40];
    lengthText(len, sizeof(len), r.seconds);
    snprintf(sub, sizeof(sub), "%s · %s", r.date, len);
    drawK(12, y + 41, sub, KO18, color);
    y += 48;
  }
  hintBar(topHint, "다음 ▼");
  flush(partial);
}

void showMessage(const char *title, const char *line) {
  g.clearBuffer();
  textCenter(95, title, KO26);
  if (line) textCenter(125, line, KO20);
  flush(false);
}

} // namespace screen
