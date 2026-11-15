// 와이파이 연결 + 폰에서 여는 웹페이지 (voicenote.local)
#pragma once
#include <Arduino.h>

namespace net {

enum class Wifi : uint8_t { Home, Phone };

bool hasSettings(Wifi w);   // secrets.h 에 이름이 적혀 있는지
void startConnect(Wifi w);  // 연결 시작 (기다리지 않음)
bool isConnected();
void stop();                // 와이파이 끔

bool startServer();         // voicenote.local 이름 + 웹페이지 시작
void handle();              // loop 에서 계속 불러야 웹페이지가 응답함
const char *ipText();       // "172.20.10.3"
uint32_t lastVisitMs();     // 마지막으로 폰이 페이지를 연 시각 (millis)

} // namespace net
