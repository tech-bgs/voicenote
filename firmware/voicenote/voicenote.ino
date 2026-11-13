// VOICE NOTE 펌웨어 — 1단계: 화면 + 버튼
// 아직 녹음·와이파이는 없음. 목록은 가짜 데이터.

#include "pins.h"
#include "screen.h"

enum class Page { Main, List };

static Page page = Page::Main;
static int selected = 0;

// 1단계 확인용 가짜 목록
static const NoteRow kDemoRows[] = {
    {13, "11/13 14:20", 312, "새 글"},
    {12, "11/13 09:05", 107, "변환됨"},
    {11, "11/12 21:40", 723, "변환됨"},
    {10, "11/12 18:11", 38, "변환 전"},
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

static void draw() {
  if (page == Page::Main) {
    screen::showMain(kDemoCount, 1, batteryPct());
  } else {
    screen::showList(kDemoRows, kDemoCount, selected, kDemoCount, batteryPct());
  }
}

void setup() {
  pinMode(PIN_VBAT_HOLD, OUTPUT);
  digitalWrite(PIN_VBAT_HOLD, HIGH); // 배터리로 켜져 있게 유지

  Serial.begin(115200);
  delay(300);
  Serial.println("[voicenote] boot step1 (screen + buttons)");

  pinMode(PIN_BTN_TOP, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);

  screen::begin();
  draw();
  Serial.println("[voicenote] main screen drawn");
}

void loop() {
  const Press t = readButton(top);
  const Press b = readButton(bottom);

  if (t != Press::None) Serial.printf("[btn] top %s\n", t == Press::Long ? "long" : "short");
  if (b != Press::None) Serial.printf("[btn] bottom %s\n", b == Press::Long ? "long" : "short");

  if (page == Page::Main) {
    if (b == Press::Short) {
      page = Page::List;
      selected = 0;
      draw();
    } else if (t == Press::Short) {
      screen::showMessage("녹음은 다음 단계", "위 버튼: 메인으로");
      page = Page::List; // 아무 버튼이나 누르면 아래에서 메인으로 돌아감
      selected = -1;
    }
  } else {
    if (selected < 0) {
      if (t != Press::None || b != Press::None) {
        page = Page::Main;
        selected = 0;
        draw();
      }
    } else if (b == Press::Short) {
      selected = (selected + 1) % kDemoCount;
      draw();
    } else if (t == Press::Short || b == Press::Long) {
      page = Page::Main;
      draw();
    }
  }
  delay(10);
}
