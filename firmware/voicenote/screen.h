// 화면 그리기: 화면마다 함수 하나. 그린 뒤 epd로 내보낸다.
#pragma once
#include <Arduino.h>

struct NoteRow {
  int number;       // 13 → "#013"
  const char *date; // "11/13 14:20" (화면에는 "11/13 14:20 · 5분")
  int seconds;      // 녹음 길이
  bool converted;   // 글자로 바뀌었으면 ✓, 아니면 "변환 전"
};

namespace screen {

void begin();

// topHint: 위 버튼 안내 (예: "▲ 녹음"). 아직 안 되는 기능이면 nullptr로 숨김
// clock: "14:32" (시계가 없으면 빈 문자열)
void showMain(int noteCount, int pendingCount, int batteryPct, const char *topHint, const char *clock);

// partial=true 면 깜빡임 없이 바뀐 곳만 다시 그림 (목록 안에서 이동할 때)
void showList(const NoteRow *rows, int total, int selected, int batteryPct, bool partial, const char *topHint);

// 녹음 중 화면. levels: 소리 크기 0~100 (오른쪽이 최신)
void showRecording(int number, uint32_t seconds, const uint8_t *levels, int levelCount, uint32_t freeMinutes,
                   bool partial);

void showMessage(const char *title, const char *line);

} // namespace screen
