// 녹음: 마이크 → SD카드 WAV 파일 (16kHz, 16비트, 모노). 재생: SD카드 WAV → 스피커
// 화면을 그리는 동안에도 소리가 끊기지 않게, 녹음은 별도 작업(task)에서 돈다.
#pragma once
#include <Arduino.h>

namespace recorder {

bool begin(); // 오디오 칩 준비. 실패하면 false

bool start(const char *path); // 녹음 시작
uint32_t stop();              // 녹음 끝. 녹음 길이(초)를 돌려준다

bool isRecording();
uint32_t elapsedSeconds();

// 최근 소리 크기 0~100 (화면 막대용)
uint8_t level();

// 녹음 중 문제 (SD 쓰기 실패 등) 생겼으면 true
bool hadError();

// ---------- 재생 (스피커) ----------
// 이 기기에서 녹음한 형식(16kHz, 16비트, 모노) WAV만 재생. 아니면 false
bool playStart(const char *path);
void playStop();
bool isPlaying(); // 끝까지 재생되면 저절로 false
uint32_t playPositionSeconds();
uint32_t playLengthSeconds();

} // namespace recorder
