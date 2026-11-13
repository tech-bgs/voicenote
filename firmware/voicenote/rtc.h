// 보드 시계 칩 PCF85063 읽기. 읽은 시간을 ESP32 시스템 시간에도 맞춰서
// SD카드 파일 날짜가 제대로 찍히게 한다.
#pragma once
#include <Arduino.h>

namespace rtc {

// Wire가 먼저 시작돼 있어야 한다. 시계가 정상이면 true
bool begin();

// 시계가 믿을 만한지 (배터리 끊김 등으로 멈췄으면 false)
bool valid();

// "14:32" (시계가 없으면 빈 문자열)
void timeText(char *out, size_t n);

} // namespace rtc
