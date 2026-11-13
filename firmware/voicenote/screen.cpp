#include "screen.h"

#include <U8g2lib.h>

#include "epd.h"

namespace screen {

// U8g2는 그림 그리는 도구로만 쓴다. 실제 화면 전송은 epd.cpp가 한다.
static U8G2 g;
static uint8_t frame[epd::BUF_SIZE];

static uint8_t noopByte(u8x8_t *, uint8_t, uint8_t, void *) { return 1; }
static uint8_t noopGpio(u8x8_t *, uint8_t, uint8_t, void *) { return 1; }

// 한글 글꼴 (unifont 16px)
static const uint8_t *const FONT_KO = u8g2_font_unifont_t_korean2;
static const uint8_t *const FONT_SMALL = u8g2_font_helvR10_tr;
static const uint8_t *const FONT_BOLD = u8g2_font_helvB12_tr;
static const uint8_t *const FONT_TITLE = u8g2_font_helvB18_tr;

void begin() {
  u8g2_Setup_ssd1607_200x200_f(g.getU8g2(), U8G2_R0, noopByte, noopGpio);
  g.enableUTF8Print();
  g.setFontMode(1);
  epd::begin();
}

// U8g2 버퍼(세로 8픽셀 묶음, 1=검정) → 화면 칩 형식(가로 8픽셀 묶음, 1=흰색)
static void flush() {
  const uint8_t *src = g.getBufferPtr();
  memset(frame, 0xFF, sizeof(frame));
  for (int y = 0; y < epd::HEIGHT; y++) {
    const uint8_t *row = src + (y / 8) * epd::WIDTH;
    const uint8_t bit = 1 << (y % 8);
    for (int x = 0; x < epd::WIDTH; x++) {
      if (row[x] & bit) frame[y * 25 + x / 8] &= ~(0x80 >> (x % 8));
    }
  }
  epd::showFull(frame);
}

// ---------- 공통 부품 ----------

static void textCenter(int y, const char *s) {
  const int w = g.getUTF8Width(s);
  g.drawUTF8((200 - w) / 2, y, s);
}

static void textRight(int xRight, int y, const char *s) {
  g.drawUTF8(xRight - g.getUTF8Width(s), y, s);
}

static void battery(int x, int y, int pct) {
  g.drawFrame(x, y, 21, 11);
  g.drawBox(x + 21, y + 3, 2, 5);
  const int w = constrain(pct, 0, 100) * 17 / 100;
  if (w > 0) g.drawBox(x + 2, y + 2, w, 7);
}

static void statusBar(int batteryPct) {
  g.setFont(FONT_BOLD);
  g.drawStr(6, 15, "--:--"); // 시계는 다음 단계에서 연결
  battery(172, 5, batteryPct);
  g.drawHLine(0, 20, 200);
}

static void hintBar(const char *left, const char *right) {
  g.drawHLine(0, 178, 200);
  g.setFont(FONT_KO);
  if (left) g.drawUTF8(6, 196, left);
  if (right) textRight(194, 196, right);
}

static void mic(int cx, int cy) {
  g.drawRBox(cx - 17, cy - 30, 34, 38, 16);
  g.drawCircle(cx, cy - 6, 27, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  g.drawCircle(cx, cy - 6, 26, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  g.drawBox(cx - 1, cy + 20, 3, 12);
  g.drawBox(cx - 13, cy + 31, 27, 3);
}

// ---------- 화면들 ----------

void showMain(int noteCount, int newCount, int batteryPct) {
  g.clearBuffer();
  statusBar(batteryPct);
  mic(100, 70);
  g.setFont(FONT_TITLE);
  textCenter(132, "VOICE NOTE");
  g.setFont(FONT_KO);
  char line[48];
  snprintf(line, sizeof(line), "녹음 %d개 · 새 글 %d개", noteCount, newCount);
  textCenter(160, line);
  hintBar("위: 녹음", "아래: 목록");
  flush();
}

void showList(const NoteRow *rows, int rowCount, int selected, int total, int batteryPct) {
  g.clearBuffer();
  statusBar(batteryPct);
  g.setFont(FONT_KO);
  g.drawUTF8(6, 40, "녹음 목록");
  char pos[16];
  snprintf(pos, sizeof(pos), "%d/%d", selected + 1, total);
  g.setFont(FONT_SMALL);
  textRight(194, 38, pos);

  int y = 46;
  for (int i = 0; i < rowCount && i < 4; i++) {
    const NoteRow &r = rows[i];
    const bool sel = i == selected;
    if (sel) {
      g.drawBox(2, y, 196, 31);
      g.setDrawColor(0);
    }
    char no[8];
    snprintf(no, sizeof(no), "#%03d", r.number);
    g.setFont(FONT_BOLD);
    g.drawStr(8, y + 14, no);
    g.setFont(FONT_SMALL);
    g.drawStr(60, y + 13, r.date);
    char len[8];
    snprintf(len, sizeof(len), "%02d:%02d", r.seconds / 60, r.seconds % 60);
    g.drawStr(8, y + 28, len);
    g.setFont(FONT_KO);
    textRight(190, y + 29, r.state);
    g.setDrawColor(1);
    if (!sel) g.drawHLine(8, y + 31, 184);
    y += 32;
  }
  hintBar("위: 선택", "아래: 다음");
  flush();
}

void showMessage(const char *title, const char *line) {
  g.clearBuffer();
  g.setFont(FONT_KO);
  textCenter(90, title);
  if (line) textCenter(115, line);
  flush();
}

} // namespace screen
