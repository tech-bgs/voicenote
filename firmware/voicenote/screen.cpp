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
static void flush(bool partial, bool allowCleanup = true) {
  const uint8_t *src = g.getBufferPtr();
  memset(frame, 0xFF, sizeof(frame));
  for (int y = 0; y < epd::HEIGHT; y++) {
    const uint8_t *row = src + (y / 8) * epd::WIDTH;
    const uint8_t bit = 1 << (y % 8);
    for (int x = 0; x < epd::WIDTH; x++) {
      if (row[x] & bit) frame[y * 25 + x / 8] &= ~(0x80 >> (x % 8));
    }
  }
  // 화면을 바꿀 때도 빠른 부분 갱신을 쓴다 (전체 갱신은 2초쯤 걸리고 깜빡임).
  // 처음 켤 때와 부분 갱신 10번마다는 epd 쪽에서 알아서 전체 갱신으로 잔상을 지운다.
  (void)partial;
  epd::showPartial(frame, allowCleanup);
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

// ✓ 표시 (가로 14, 세로 11, 두께 3). 글꼴에 의존하지 않게 직접 그린다
static void checkMark(int x, int y, uint8_t color) {
  g.setDrawColor(color);
  for (int t = 0; t < 3; t++) {
    g.drawLine(x, y + 5 + t, x + 4, y + 9 + t);      // 짧은 획
    g.drawLine(x + 4, y + 9 + t, x + 13, y + t);     // 긴 획
  }
  g.setDrawColor(1);
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

void showMain(int noteCount, int pendingCount, int batteryPct, const char *topHint, const char *clock) {
  g.clearBuffer();
  statusBar(batteryPct, "VOICE NOTE", (clock && clock[0]) ? clock : nullptr);
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
    if (r.converted) {
      checkMark(174, y + 7, color);
    } else {
      textRight(188, y + 21, "변환 전", KO18, color);
    }

    char len[12], sub[40];
    lengthText(len, sizeof(len), r.seconds);
    snprintf(sub, sizeof(sub), "%s · %s", r.date, len);
    drawK(12, y + 41, sub, KO18, color);
    y += 48;
  }
  hintBar(topHint, "다음 ▼");
  flush(partial);
}

void showRecording(int number, uint32_t seconds, const uint8_t *levels, int levelCount, uint32_t freeMinutes,
                   bool partial) {
  g.clearBuffer();
  // 맨 위: 검은 띠에 "● 녹음 중"
  g.drawBox(0, 0, 200, 25);
  g.setDrawColor(0);
  g.drawDisc(13, 12, 5);
  g.setDrawColor(1);
  drawK(24, 18, "녹음 중", KO18, 0);
  char no[8];
  snprintf(no, sizeof(no), "#%03d", number);
  textRight(194, 18, no, KO18, 0);

  // 경과 시간
  char t[12];
  if (seconds >= 3600) {
    snprintf(t, sizeof(t), "%lu:%02lu:%02lu", (unsigned long)(seconds / 3600), (unsigned long)(seconds / 60 % 60),
             (unsigned long)(seconds % 60));
  } else {
    snprintf(t, sizeof(t), "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
  }
  textCenter(96, t, NUM48);

  // 소리 크기 막대 (오른쪽이 최신)
  const int barW = 5, gap = 3, maxH = 28, base = 140;
  const int totalW = levelCount * (barW + gap) - gap;
  int x = (200 - totalW) / 2;
  for (int i = 0; i < levelCount; i++, x += barW + gap) {
    const int h = max(2, levels[i] * maxH / 100);
    g.drawBox(x, base - h, barW, h);
  }

  // 남은 공간
  char left[40];
  if (freeMinutes >= 120) {
    snprintf(left, sizeof(left), "남은 공간 약 %lu시간", (unsigned long)(freeMinutes / 60));
  } else {
    snprintf(left, sizeof(left), "남은 공간 약 %lu분", (unsigned long)freeMinutes);
  }
  textCenter(161, left, KO18);

  hintBar("▲ 멈춤", nullptr);
  flush(partial, false); // 녹음 중엔 잔상 정리용 전체 갱신(깜빡임) 안 함
}

// ---------- 줄바꿈 ----------

static int advOf(const KFont &f, uint16_t code) {
  const KGlyph *gl = findGlyph(f, code);
  return gl ? gl->adv : f.height / 2;
}

// text를 maxW 폭으로 잘라 lines[]에 담는다. 넘치면 마지막 줄 끝에 "…"
static int wrapText(const char *text, const KFont &f, int maxW, char lines[][96], int maxLines) {
  int count = 0;
  const char *p = text;
  while (*p == ' ') p++;
  while (*p && count < maxLines) {
    const char *lineStart = p;
    const char *lastSpace = nullptr;
    const char *q = p;
    int w = 0;
    while (*q) {
      const char *before = q;
      const uint16_t code = nextCode(q);
      const int a = advOf(f, code);
      if (w + a > maxW) {
        q = before;
        break;
      }
      if (code == ' ') lastSpace = before;
      w += a;
    }
    const char *lineEnd = q;
    const char *next = q;
    // 띄어쓰기에서 끊을 수 있으면 거기서 (단어 중간에서 안 끊기게)
    if (*q && lastSpace && (lastSpace - lineStart) > (q - lineStart) / 2) {
      lineEnd = lastSpace;
      next = lastSpace + 1;
    }
    size_t len = min((size_t)(lineEnd - lineStart), (size_t)95);
    memcpy(lines[count], lineStart, len);
    lines[count][len] = 0;
    count++;
    p = next;
    while (*p == ' ') p++;
  }
  if (*p && count > 0) { // 다 못 담았으면 마지막 줄을 줄이고 "…"
    char *last = lines[count - 1];
    while (strlen(last) > 0 && widthK(f, last) + advOf(f, 0x2026) > maxW) {
      size_t l = strlen(last);
      do l--; while (l > 0 && ((uint8_t)last[l] & 0xC0) == 0x80);
      last[l] = 0;
    }
    strcat(last, "…");
  }
  return count;
}

static void lengthLong(char *out, size_t n, uint32_t seconds) {
  if (seconds < 60) {
    snprintf(out, n, "%lu초", (unsigned long)seconds);
  } else {
    snprintf(out, n, "%lu분 %lu초", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
  }
}

void showDetail(int number, uint32_t seconds, time_t when, bool converted, const char *preview, const char *clock) {
  g.clearBuffer();
  statusBar(80, "녹음 보기", (clock && clock[0]) ? clock : nullptr);

  char no[8];
  snprintf(no, sizeof(no), "#%03d", number);
  drawK(10, 52, no, KO26);
  char len[24];
  lengthLong(len, sizeof(len), seconds);
  textRight(190, 50, len, KO18);

  char date[40];
  struct tm t;
  localtime_r(&when, &t);
  if (when > 0 && t.tm_year >= 125) {
    const int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
    snprintf(date, sizeof(date), "%d월 %d일 %s %d:%02d", t.tm_mon + 1, t.tm_mday, t.tm_hour < 12 ? "오전" : "오후",
             h12, t.tm_min);
  } else {
    snprintf(date, sizeof(date), "날짜 없음");
  }
  drawK(10, 76, date, KO18);
  g.drawHLine(10, 88, 180);

  if (converted && preview && preview[0]) {
    static char lines[3][96];
    const int n = wrapText(preview, KO18, 180, lines, 3);
    for (int i = 0; i < n; i++) drawK(10, 108 + i * 22, lines[i], KO18);
  } else {
    textCenter(114, "아직 변환 전이에요", KO18);
    textCenter(140, "와이파이에 연결하면", KO18);
    textCenter(162, "글자로 바뀌어요", KO18);
  }
  hintBar("▲ 재생", "뒤로 ▼");
  flush(false);
}

void showPlaying(int number, uint32_t position, uint32_t total, bool partial) {
  g.clearBuffer();
  char no[8];
  snprintf(no, sizeof(no), "#%03d", number);
  statusBar(80, "재생 중", no);

  // 가운데 큰 원 + 일시정지 모양 (재생 중이라는 표시)
  const int cx = 100, cy = 72;
  g.drawDisc(cx, cy, 30);
  g.setDrawColor(0);
  g.drawBox(cx - 10, cy - 13, 7, 27);
  g.drawBox(cx + 4, cy - 13, 7, 27);
  g.setDrawColor(1);

  // 진행 막대
  const int x0 = 14, x1 = 186, y = 128;
  g.drawRFrame(x0, y - 4, x1 - x0 + 1, 9, 4);
  g.drawRFrame(x0 + 1, y - 3, x1 - x0 - 1, 7, 3);
  if (total > 0) {
    const int w = (int)((x1 - x0) * min(position, total) / total);
    if (w >= 8) g.drawRBox(x0, y - 4, w, 9, 4);
  }

  char a[12], b[12];
  snprintf(a, sizeof(a), "%lu:%02lu", (unsigned long)(position / 60), (unsigned long)(position % 60));
  snprintf(b, sizeof(b), "%lu:%02lu", (unsigned long)(total / 60), (unsigned long)(total % 60));
  drawK(14, 156, a, KO20);
  textRight(186, 156, b, KO20);

  hintBar("▲ 멈춤", "뒤로 ▼");
  flush(partial, false); // 재생 중엔 깜빡임 없이
}

void showMessage(const char *title, const char *line) {
  g.clearBuffer();
  textCenter(95, title, KO26);
  if (line) textCenter(125, line, KO20);
  flush(false);
}

// ---------- 전송 모드 ----------
void showTransferSteps(const Step *states, const char *const *labels, int count, int progressPct,
                       const char *clock, bool partial) {
  g.clearBuffer();
  statusBar(80, "전송 모드", clock);
  int y = 50;
  for (int i = 0; i < count; i++) {
    if (states[i] == Step::Done) {
      checkMark(12, y - 13, 1);
    } else if (states[i] == Step::Running) {
      g.drawTriangle(13, y - 14, 13, y - 2, 23, y - 8);
    } else {
      g.drawCircle(18, y - 8, 5);
      g.drawCircle(18, y - 8, 4);
    }
    drawK(32, y, labels[i], KO18);
    y += 26;
  }
  if (progressPct >= 0) {
    const int x0 = 32, x1 = 188, py = 158;
    g.drawRFrame(x0, py - 4, x1 - x0 + 1, 9, 4);
    g.drawRFrame(x0 + 1, py - 3, x1 - x0 - 1, 7, 3);
    const int w = (x1 - x0) * min(progressPct, 100) / 100;
    if (w >= 8) g.drawRBox(x0, py - 4, w, 9, 4);
  }
  hintBar(nullptr, "그만 ▼");
  flush(partial);
}

void showTransferReady(const char *host, const char *ip, const char *line1, const char *line2, const char *clock) {
  g.clearBuffer();
  statusBar(80, "전송 모드", clock);
  textCenter(46, "폰에서 열어 보세요", KO18);
  g.drawRBox(8, 54, 184, 63, 8);
  textCenter(80, host, KO20, 0);
  textCenter(108, ip, KO26, 0);
  if (line1) textCenter(140, line1, KO18);
  if (line2) textCenter(162, line2, KO18);
  hintBar(nullptr, "끝내기 ▼");
  flush(false);
}

void showNoWifi(const char *clock) {
  g.clearBuffer();
  statusBar(80, "전송 모드", clock);
  textCenter(70, "와이파이를 못 찾았어요", KO20);
  textCenter(104, "집 와이파이도", KO18);
  textCenter(128, "휴대폰 핫스팟도 안 잡혀요", KO18);
  textCenter(156, "녹음은 그대로 있어요", KO18);
  hintBar("▲ 다시", "끝내기 ▼");
  flush(false);
}

} // namespace screen
