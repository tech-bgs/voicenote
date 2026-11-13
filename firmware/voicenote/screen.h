// 화면 그리기: 화면마다 함수 하나. 그린 뒤 epd로 내보낸다.
#pragma once
#include <Arduino.h>

struct NoteRow {
  int number;      // 13 → "#013"
  const char *date; // "11/13 14:20" (화면에는 "11/13 14:20 · 5분")
  int seconds;      // 녹음 길이
  const char *state; // "새 글" / "변환됨" / "변환 전"
};

namespace screen {

void begin();

void showMain(int noteCount, int newCount, int batteryPct);
// partial=true 면 깜빡임 없이 바뀐 곳만 다시 그림 (목록 안에서 이동할 때)
void showList(const NoteRow *rows, int total, int selected, int batteryPct, bool partial);
void showMessage(const char *title, const char *line);

} // namespace screen
