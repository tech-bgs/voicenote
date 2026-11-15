// 배터리 잔량 · 전원 끄기
#pragma once
#include <Arduino.h>

namespace power {

void begin();
uint32_t batteryMilliVolts(); // 배터리 전압 (mV)
int batteryPercent();         // 0~100 (대략)

// 화면·오디오·배터리 전원을 끈다. USB가 꽂혀 있어 안 꺼지면 깊은 잠에 들고, 버튼을 누르면 다시 켜진다.
void off();

} // namespace power
