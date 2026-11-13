// 버튼 읽기: 눌렀다 뗄 때 짧게/길게 구분
#pragma once
#include <Arduino.h>

enum class Press { None, Short, Long };

struct Button {
  uint8_t pin;
  bool down = false;
  uint32_t pressedAt = 0;
};

static const uint32_t kLongMs = 1500;

inline Press readButton(Button &b) {
  const bool isDown = digitalRead(b.pin) == LOW;
  const uint32_t now = millis();
  if (isDown && !b.down) {
    b.down = true;
    b.pressedAt = now;
  } else if (!isDown && b.down) {
    b.down = false;
    const uint32_t held = now - b.pressedAt;
    if (held < 30) return Press::None; // 잡음
    return held >= kLongMs ? Press::Long : Press::Short;
  }
  return Press::None;
}
