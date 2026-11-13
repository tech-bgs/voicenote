// VOICE NOTE 펌웨어 — 1단계: 화면 + 버튼
// 아직 녹음·와이파이는 없음. 목록은 가짜 데이터.

#include "pins.h"
#include "screen.h"

enum class Page { Main, List, Message };

static Page page = Page::Main;
static int selected = 0;

// 1단계 확인용 가짜 목록
static const NoteRow kDemoRows[] = {
    {13, "11/13 14:20", 312, false},
    {12, "11/13 09:05", 107, true},
    {11, "11/12 21:40", 723, true},
    {10, "11/12 18:11", 38, true},
};
static const int kDemoCount = sizeof(kDemoRows) / sizeof(kDemoRows[0]);

// 버튼: 눌렀다 뗄 때 짧게/길게 구분
struct Button {
  uint8_t pin;
  bool down = false;
  uint32_t pressedAt = 0;
};
static Button top{PIN_BTN_TOP};
static Button bottom{PIN_BTN_PWR};

enum class Press { None, Short, Long };
static const uint32_t kLongMs = 1500;

static Press readButton(Button &b) {
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

static int batteryPct() { return 80; } // 배터리 읽기는 다음 단계

static int pendingCount() {
  int n = 0;
  for (int i = 0; i < kDemoCount; i++) n += kDemoRows[i].converted ? 0 : 1;
  return n;
}

// 아직 안 되는 기능은 안내에 띄우지 않는다 (녹음·재생은 다음 단계)
static const char *const kMainTopHint = nullptr;  // 녹음 붙이면 "▲ 녹음"
static const char *const kListTopHint = "▲ 뒤로"; // 재생 붙이면 "▲ 선택"

static void goMain() {
  page = Page::Main;
  screen::showMain(kDemoCount, pendingCount(), batteryPct(), kMainTopHint);
}

static void goList() {
  page = Page::List;
  selected = 0;
  screen::showList(kDemoRows, kDemoCount, selected, batteryPct(), false, kListTopHint); // 들어갈 땐 전체 갱신
}

void setup() {
  pinMode(PIN_VBAT_HOLD, OUTPUT);
  digitalWrite(PIN_VBAT_HOLD, HIGH); // 배터리로 켜져 있게 유지

  Serial.begin(115200);
  delay(300);
  Serial.println("[voicenote] boot step1c (v3 design)");

  pinMode(PIN_BTN_TOP, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);

  screen::begin();
  goMain();
  Serial.println("[voicenote] main screen drawn");
}

void loop() {
  const Press t = readButton(top);
  const Press b = readButton(bottom);

  if (t != Press::None) Serial.printf("[btn] top %s\n", t == Press::Long ? "long" : "short");
  if (b != Press::None) Serial.printf("[btn] bottom %s\n", b == Press::Long ? "long" : "short");

  switch (page) {
    case Page::Main:
      if (b == Press::Short) goList(); // 위 버튼(녹음)은 다음 단계에서 연결
      break;

    case Page::List:
      if (b == Press::Short) {
        selected = (selected + 1) % kDemoCount;
        const uint32_t start = millis();
        screen::showList(kDemoRows, kDemoCount, selected, batteryPct(), true, kListTopHint); // 깜빡임 없이
        Serial.printf("[list] move to %d (%lu ms)\n", selected, millis() - start);
      } else if (t == Press::Short || b == Press::Long) {
        goMain();
      }
      break;

    case Page::Message:
      if (t != Press::None || b != Press::None) goMain();
      break;
  }
  delay(10);
}
