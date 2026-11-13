#include "rtc.h"

#include <Wire.h>
#include <sys/time.h>
#include <time.h>

namespace rtc {

static const uint8_t kAddr = 0x51;
static bool ok = false;

static uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

bool begin() {
  // 시계 칩을 못 읽어도 한국 시간으로 표시되게 시간대는 항상 먼저 정한다
  setenv("TZ", "KST-9", 1);
  tzset();

  Wire.beginTransmission(kAddr);
  Wire.write(0x04); // 초 레지스터부터
  const uint8_t err = Wire.endTransmission(true);
  if (err != 0) {
    Serial.printf("[rtc] no answer (err=%u)\n", err);
    return false;
  }
  const uint8_t got = Wire.requestFrom(kAddr, (uint8_t)7);
  if (got != 7) {
    Serial.printf("[rtc] read failed (got %u bytes)\n", got);
    return false;
  }
  const uint8_t sec = Wire.read();
  const uint8_t min = Wire.read();
  const uint8_t hour = Wire.read();
  const uint8_t day = Wire.read();
  Wire.read(); // 요일
  const uint8_t mon = Wire.read();
  const uint8_t year = Wire.read();

  if (sec & 0x80) { // 시계가 멈췄던 적 있음 → 시간 못 믿음
    Serial.println("[rtc] clock was stopped, time not trusted");
    return false;
  }
  struct tm t = {};
  t.tm_sec = bcd(sec & 0x7F);
  t.tm_min = bcd(min & 0x7F);
  t.tm_hour = bcd(hour & 0x3F);
  t.tm_mday = bcd(day & 0x3F);
  t.tm_mon = bcd(mon & 0x1F) - 1;
  t.tm_year = bcd(year) + 100; // 2000년 기준
  if (t.tm_year < 125) { // 2025년 이전이면 설정 안 된 시계
    Serial.println("[rtc] time not set");
    return false;
  }
  // 시계 칩에는 세계 표준시(UTC)가 들어 있다 (실제 확인: 칩 14:19, 한국 23:19)
  setenv("TZ", "UTC0", 1);
  tzset();
  const time_t now = mktime(&t);
  setenv("TZ", "KST-9", 1);
  tzset();
  struct timeval tv = {now, 0};
  settimeofday(&tv, nullptr);
  ok = true;
  Serial.printf("[rtc] chip (UTC) %04d-%02d-%02d %02d:%02d\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                t.tm_min);
  return true;
}

bool valid() { return ok; }

void timeText(char *out, size_t n) {
  const time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (t.tm_year < 125) { // 2025년 이전 = 시간 모름
    out[0] = 0;
    return;
  }
  snprintf(out, n, "%02d:%02d", t.tm_hour, t.tm_min);
}

} // namespace rtc
