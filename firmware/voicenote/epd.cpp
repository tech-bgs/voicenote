#include "epd.h"

#include <SPI.h>

#include "pins.h"

namespace epd {

static SPIClass spi(FSPI);
static const SPISettings kSpi(10000000, MSBFIRST, SPI_MODE0);

static void waitIdle() {
  // BUSY 핀이 HIGH인 동안 화면 칩이 일하는 중
  const uint32_t start = millis();
  while (digitalRead(PIN_EPD_BUSY) == HIGH) {
    if (millis() - start > 10000) {
      Serial.println("[epd] busy timeout");
      return;
    }
    delay(2);
  }
}

static void command(uint8_t c) {
  digitalWrite(PIN_EPD_DC, LOW);
  digitalWrite(PIN_EPD_CS, LOW);
  spi.beginTransaction(kSpi);
  spi.transfer(c);
  spi.endTransaction();
  digitalWrite(PIN_EPD_CS, HIGH);
}

static void data(uint8_t d) {
  digitalWrite(PIN_EPD_DC, HIGH);
  digitalWrite(PIN_EPD_CS, LOW);
  spi.beginTransaction(kSpi);
  spi.transfer(d);
  spi.endTransaction();
  digitalWrite(PIN_EPD_CS, HIGH);
}

static void dataBlock(const uint8_t *buf, size_t len) {
  digitalWrite(PIN_EPD_DC, HIGH);
  digitalWrite(PIN_EPD_CS, LOW);
  spi.beginTransaction(kSpi);
  for (size_t i = 0; i < len; i++) spi.transfer(buf[i]);
  spi.endTransaction();
  digitalWrite(PIN_EPD_CS, HIGH);
}

static void reset() {
  digitalWrite(PIN_EPD_RST, LOW);
  delay(10);
  digitalWrite(PIN_EPD_RST, HIGH);
  delay(10);
  waitIdle();
}

static void initChip() {
  reset();
  command(0x12); // 소프트 리셋
  waitIdle();

  command(0x01); // 화면 줄 수: 200줄
  data(0xC7);
  data(0x00);
  data(0x01);

  command(0x11); // 데이터 쓰는 방향
  data(0x01);

  command(0x44); // 가로 범위 (8픽셀 단위) 0~24
  data(0x00);
  data(0x18);

  command(0x45); // 세로 범위 199 → 0
  data(0xC7);
  data(0x00);
  data(0x00);
  data(0x00);

  command(0x3C); // 테두리
  data(0x01);

  command(0x18); // 내장 온도 센서 사용
  data(0x80);

  command(0x4E); // 쓰기 시작 위치
  data(0x00);
  command(0x4F);
  data(0xC7);
  data(0x00);
  waitIdle();
}

void begin() {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, LOW); // 화면 전원 켬
  delay(10);

  pinMode(PIN_EPD_CS, OUTPUT);
  pinMode(PIN_EPD_DC, OUTPUT);
  pinMode(PIN_EPD_RST, OUTPUT);
  pinMode(PIN_EPD_BUSY, INPUT);
  digitalWrite(PIN_EPD_CS, HIGH);

  spi.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
}

void showFull(const uint8_t *image) {
  initChip();
  command(0x24); // 새 그림
  dataBlock(image, BUF_SIZE);
  command(0x26); // 이전 그림 (다음 부분 갱신 대비)
  dataBlock(image, BUF_SIZE);

  command(0x22); // 전체 갱신
  data(0xF7);
  command(0x20);
  waitIdle();
}

void sleep() {
  command(0x10); // 깊은 잠
  data(0x01);
  delay(5);
}

} // namespace epd
