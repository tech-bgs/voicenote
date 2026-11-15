#include "power.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "pins.h"

namespace power {

static const int kBatteryPin = 4; // 배터리 전압이 1/2로 나뉘어 들어옴 (보드 회로)

void begin() {
  analogSetPinAttenuation(kBatteryPin, ADC_11db);
}

uint32_t batteryMilliVolts() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(kBatteryPin);
  return sum / 16 * 2;
}

// 리튬 배터리 전압 → 잔량 (대략적인 표, 사이는 직선으로 이음)
int batteryPercent() {
  static const uint16_t mv[] = {3300, 3500, 3650, 3750, 3850, 4000, 4150};
  static const uint8_t pct[] = {0, 5, 20, 40, 60, 80, 100};
  const uint32_t v = batteryMilliVolts();
  if (v <= mv[0]) return 0;
  for (int i = 1; i < 7; i++) {
    if (v <= mv[i]) return pct[i - 1] + (int)(v - mv[i - 1]) * (pct[i] - pct[i - 1]) / (mv[i] - mv[i - 1]);
  }
  return 100;
}

void off() {
  Serial.println("[power] off");
  Serial.flush();
  digitalWrite(PIN_AUDIO_PWR, HIGH); // 오디오 전원 끔
  digitalWrite(PIN_EPD_PWR, HIGH);   // 화면 전원 끔 (그림은 그대로 남음)
  digitalWrite(PIN_VBAT_HOLD, LOW);  // 배터리 전원 놓음 → 배터리만이면 여기서 꺼짐
  delay(1500);

  // 아직 살아 있음 = USB 전원. 버튼(위·아래)을 누를 때까지 깊은 잠
  Serial.println("[power] still on (USB) → deep sleep");
  Serial.flush();
  while (digitalRead(PIN_BTN_TOP) == LOW || digitalRead(PIN_BTN_PWR) == LOW) delay(10); // 누르고 있으면 뗄 때까지
  for (gpio_num_t pin : {(gpio_num_t)PIN_BTN_TOP, (gpio_num_t)PIN_BTN_PWR}) { // 잠자는 동안에도 버튼 핀을 HIGH로 유지
    rtc_gpio_pullup_en(pin);
    rtc_gpio_pulldown_dis(pin);
  }
  esp_sleep_enable_ext1_wakeup((1ULL << PIN_BTN_TOP) | (1ULL << PIN_BTN_PWR), ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

} // namespace power
