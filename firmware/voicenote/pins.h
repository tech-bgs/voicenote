// 보드 핀 번호: Waveshare ESP32-S3-ePaper-1.54 (V2). 근거는 PINS.md
#pragma once

// 화면 (e-paper, SPI)
#define PIN_EPD_MOSI 13
#define PIN_EPD_SCK 12
#define PIN_EPD_CS 11
#define PIN_EPD_DC 10
#define PIN_EPD_RST 9
#define PIN_EPD_BUSY 8
#define PIN_EPD_PWR 6 // LOW = 화면 전원 켬

// 버튼 (누르면 LOW)
#define PIN_BTN_TOP 0  // BOOT 버튼 = 1번(위)
#define PIN_BTN_PWR 18 // PWR 버튼 = 2번(아래)

// 전원
#define PIN_VBAT_HOLD 17 // HIGH = 배터리로 켜진 상태 유지
#define PIN_AUDIO_PWR 42 // LOW = 오디오 전원 켬

#define PIN_LED 3
