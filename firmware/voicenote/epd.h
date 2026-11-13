// 1.54인치 200x200 흑백 e-paper 드라이버 (SSD1681 계열 화면 칩)
#pragma once
#include <Arduino.h>

namespace epd {

constexpr int WIDTH = 200;
constexpr int HEIGHT = 200;
constexpr int BUF_SIZE = WIDTH * HEIGHT / 8; // 5000바이트

// 화면 전원을 켜고 칩을 준비한다
void begin();

// image: 한 줄에 25바이트씩, 왼쪽 비트가 먼저, 1 = 흰색, 0 = 검정
// 전체를 새로 그린다 (깜빡이며 약 2초)
void showFull(const uint8_t *image);

// 바뀐 곳만 다시 그린다 (깜빡임 거의 없음). 10번마다 한 번은 잔상 지우려고 전체 갱신
void showPartial(const uint8_t *image);

// 화면 칩을 잠재운다 (그림은 화면에 그대로 남음)
void sleep();

} // namespace epd
