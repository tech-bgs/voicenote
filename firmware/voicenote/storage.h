// SD카드: /notes/rec_013.wav 같은 녹음 파일 관리
// 글자로 바뀐 녹음은 같은 이름의 .txt 가 옆에 있다 (rec_013.txt 또는 rec_013.wav.txt)
#pragma once
#include <Arduino.h>

namespace storage {

struct Note {
  int number;
  uint32_t seconds; // 녹음 길이 (파일 크기로 계산)
  time_t when;      // 파일 날짜 (시계가 없으면 0)
  bool converted;
};

bool begin(); // SD카드 연결. 실패하면 false

// 최신 녹음이 앞에 오도록 notes[]에 채우고 개수를 돌려준다
int list(Note *notes, int max);

int nextNumber();
void wavPath(int number, char *out, size_t n);

// SD 남은 공간으로 녹음 가능한 시간(분)
uint32_t freeMinutes();

} // namespace storage
