// 화면 그리기: 화면마다 함수 하나. 그린 뒤 epd로 내보낸다.
#pragma once
#include <Arduino.h>

struct NoteRow {
  int number;      // 13 → "#013"
  const char *date; // "11/13 14:20"
  int seconds;      // 녹음 길이
  const char *state; // "새 글" / "변환됨" / "변환 전"
};

namespace screen {

void begin();

void showMain(int noteCount, int newCount, int batteryPct);
void showList(const NoteRow *rows, int rowCount, int selected, int total, int batteryPct);
void showMessage(const char *title, const char *line);

} // namespace screen
