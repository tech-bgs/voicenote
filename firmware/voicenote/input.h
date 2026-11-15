// 버튼 읽기
// - 길게 누르기가 없는 곳: 누르는 순간 Short (손 뗄 때까지 안 기다림)
// - 길게 누르기가 있는 곳: 1.5초가 되는 순간 Long, 그 전에 떼면 Short
#pragma once
#include <Arduino.h>

enum class Press { None, Short, Long };

struct Button {
  uint8_t pin;
  bool down = false;
  bool fired = false; // 이번 누름에서 이미 알렸으면 손 뗄 때 또 알리지 않음
  uint32_t pressedAt = 0;
};

static const uint32_t kLongMs = 1500;
static const uint32_t kNoiseMs = 30; // 이보다 짧은 건 잡음

inline Press readButton(Button &b, bool hasLong) {
  const bool isDown = digitalRead(b.pin) == LOW;
  const uint32_t now = millis();
  if (isDown && !b.down) {
    b.down = true;
    b.fired = false;
    b.pressedAt = now;
    return Press::None;
  }
  if (isDown && b.down && !b.fired) {
    const uint32_t held = now - b.pressedAt;
    if (!hasLong && held >= kNoiseMs) {
      b.fired = true;
      return Press::Short;
    }
    if (hasLong && held >= kLongMs) {
      b.fired = true;
      return Press::Long;
    }
    return Press::None;
  }
  if (!isDown && b.down) {
    b.down = false;
    const uint32_t held = now - b.pressedAt;
    if (b.fired || held < kNoiseMs) return Press::None;
    return Press::Short; // 길게 누르기가 있는 곳에서 1.5초 전에 뗌
  }
  return Press::None;
}
