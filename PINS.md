# 보드 핀 배치 — Waveshare ESP32-S3-ePaper-1.54 (V2)

출처: Waveshare 공식 예제 (github.com/waveshareteam/ESP32-S3-ePaper-1.54).
V1·V2 예제는 핀이 같고 메모리 설정만 다름. V2 = ESP32-S3-PICO-1-N8R8 (플래시 8MB, PSRAM 8MB).

| 용도 | 핀 |
|---|---|
| 화면 SPI MOSI / SCK / CS / DC / RST / BUSY | 13 / 12 / 11 / 10 / 9 / 8 |
| 화면 전원 | 6 |
| I2C SDA / SCL (오디오 칩, 시계, 온습도) | 47 / 48 |
| I2S MCLK / BCLK / WS / DOUT / DIN | 14 / 15 / 38 / 45 / 16 |
| 스피커 앰프 | 46 |
| 오디오 전원 | 42 |
| SD카드 (SDMMC 1선) CLK / CMD / D0 | 39 / 41 / 40 |
| BOOT 버튼 | 0 |
| PWR 버튼 | 18 |
| 배터리 전원 유지 | 17 |
| 시계(RTC) 알람 | 5 |
| LED | 3 |
| 배터리 전압 (ADC1 채널 3) | 4 |

오디오 칩 ES8311, 시계 PCF85063, 온습도 SHTC3.
