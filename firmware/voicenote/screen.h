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

// 위 막대에 보일 배터리 잔량 (0~100). 바뀔 때마다 알려 줌
void setBattery(int pct);

// topHint: 위 버튼 안내 (예: "▲ 녹음"). 아직 안 되는 기능이면 nullptr로 숨김
// clock: "14:32" (시계가 없으면 빈 문자열)
void showMain(int noteCount, int pendingCount, int batteryPct, const char *topHint, const char *clock);

// partial=true 면 깜빡임 없이 바뀐 곳만 다시 그림 (목록 안에서 이동할 때)
void showList(const NoteRow *rows, int total, int selected, int batteryPct, bool partial, const char *topHint);

// 녹음 중 화면. levels: 소리 크기 0~100 (오른쪽이 최신)
void showRecording(int number, uint32_t seconds, const uint8_t *levels, int levelCount, uint32_t freeMinutes,
                   bool partial);

// 녹음 하나 열어 보기. preview: 변환된 글 앞부분 (없으면 nullptr)
void showDetail(int number, uint32_t seconds, time_t when, bool converted, const char *preview, const char *clock);

// 재생 중 화면 (1초마다 partial=true 로 다시 그림)
void showPlaying(int number, uint32_t position, uint32_t total, bool partial);

void showMessage(const char *title, const char *line);

// ---------- 전송 모드 ----------
enum class Step : uint8_t { Todo, Running, Done };

// 단계 목록 (✓ 끝남 / ▶ 하는 중 / ○ 아직). progressPct 가 0~100 이면 아래에 진행 막대, -1 이면 숨김
void showTransferSteps(const Step *states, const char *const *labels, int count, int progressPct,
                       const char *clock, bool partial);

// 폰에서 열 주소 안내. line1·line2: 아래 안내 두 줄 (없으면 nullptr)
void showTransferReady(const char *host, const char *ip, const char *line1, const char *line2, const char *clock);

// 저절로 꺼질 때 마지막으로 남기는 화면 (전원이 꺼져도 그대로 보임)
void showOff(int noteCount, int batteryPct);

// 와이파이를 못 찾았을 때
void showNoWifi(const char *clock);

} // namespace screen
