// VOICE NOTE 펌웨어 — 3단계: 녹음 + 목록 + 내용 보기 + 스피커 재생
// 아직 와이파이·변환은 없음.

#include <Wire.h>

#include "input.h"
#include "pins.h"
#include "recorder.h"
#include "rtc.h"
#include "screen.h"
#include "storage.h"

enum class Page { Main, List, Detail, Playing, Recording, Message };

static Page page = Page::Main;
static int selected = 0;
static bool sdOk = false;
static bool recOk = false;
static Page afterMessage = Page::Main; // 안내 화면에서 버튼 누르면 돌아갈 곳

// ---------- 녹음 목록 (SD카드에서 읽음) ----------
static const int kMaxNotes = 200;
static storage::Note notes[kMaxNotes];
static NoteRow rows[kMaxNotes];
static char dates[kMaxNotes][16];
static int noteCount = 0;

static void refreshNotes() {
  noteCount = sdOk ? storage::list(notes, kMaxNotes) : 0;
  for (int i = 0; i < noteCount; i++) {
    struct tm t;
    localtime_r(&notes[i].when, &t);
    if (notes[i].when > 0 && t.tm_year >= 125) {
      snprintf(dates[i], sizeof(dates[i]), "%d/%d %02d:%02d", t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    } else {
      snprintf(dates[i], sizeof(dates[i]), "날짜 없음");
    }
    rows[i] = {notes[i].number, dates[i], (int)notes[i].seconds, notes[i].converted};
  }
  Serial.printf("[notes] %d found\n", noteCount);
}

static int pendingCount() {
  int n = 0;
  for (int i = 0; i < noteCount; i++) n += notes[i].converted ? 0 : 1;
  return n;
}

// ---------- 버튼 ----------
static Button top{PIN_BTN_TOP};
static Button bottom{PIN_BTN_PWR};

static int batteryPct() { return 80; } // 배터리 읽기는 다음 단계

// ---------- 화면 이동 ----------
static const char *mainTopHint() { return (sdOk && recOk) ? "▲ 녹음" : nullptr; }
static const char *const kListTopHint = "▲ 선택";

static void goMain() {
  page = Page::Main;
  if (!sdOk) {
    page = Page::Message;
    screen::showMessage("SD카드가 없어요", "카드를 넣고 다시 켜 주세요");
    return;
  }
  char clock[8];
  rtc::timeText(clock, sizeof(clock));
  screen::showMain(noteCount, pendingCount(), batteryPct(), mainTopHint(), clock);
}

static void showMessage(const char *title, const char *line, Page returnTo) {
  page = Page::Message;
  afterMessage = returnTo;
  screen::showMessage(title, line);
}

// 목록 다시 보이기 (선택 위치 유지)
static void showListAgain() {
  page = Page::List;
  if (selected >= noteCount) selected = 0;
  screen::showList(rows, noteCount, selected, batteryPct(), false, kListTopHint); // 들어갈 땐 전체 갱신
}

static void goList() {
  if (noteCount == 0) {
    showMessage("녹음이 없어요", "아무 버튼: 돌아가기", Page::Main);
    return;
  }
  selected = 0;
  showListAgain();
}

// ---------- 내용 보기 · 재생 ----------
static char preview[400];
static uint32_t lastPlaySec = 0;

static void openDetail() {
  page = Page::Detail;
  const storage::Note &n = notes[selected];
  const bool hasText = n.converted && storage::textPreview(n.number, preview, sizeof(preview));
  char clock[8];
  rtc::timeText(clock, sizeof(clock));
  screen::showDetail(n.number, n.seconds, n.when, n.converted, hasText ? preview : nullptr, clock);
}

static void startPlay() {
  const storage::Note &n = notes[selected];
  char path[48];
  storage::wavPath(n.number, path, sizeof(path));
  if (!recorder::playStart(path)) {
    showMessage("재생할 수 없어요", "이 기기에서 녹음한 것만 돼요", Page::Detail);
    return;
  }
  page = Page::Playing;
  lastPlaySec = 0;
  screen::showPlaying(n.number, 0, recorder::playLengthSeconds(), false);
}

static void updatePlayingScreen() {
  const uint32_t sec = recorder::playPositionSeconds();
  if (sec == lastPlaySec) return;
  lastPlaySec = sec;
  screen::showPlaying(notes[selected].number, sec, recorder::playLengthSeconds(), true);
}

// ---------- 녹음 ----------
static const int kLevelCount = 20;
static uint8_t levels[kLevelCount];
static uint8_t peakSinceDraw = 0;
static int recNumber = 0;
static uint32_t lastDrawnSec = 0;
static uint32_t freeMin = 0;

static void startRecording() {
  recNumber = storage::nextNumber();
  char path[48];
  storage::wavPath(recNumber, path, sizeof(path));
  freeMin = storage::freeMinutes();
  if (!recorder::start(path)) {
    showMessage("녹음을 시작 못 했어요", "아무 버튼: 돌아가기", Page::Main);
    return;
  }
  page = Page::Recording;
  memset(levels, 0, sizeof(levels));
  peakSinceDraw = 0;
  lastDrawnSec = 0;
  screen::showRecording(recNumber, 0, levels, kLevelCount, freeMin, false);
}

static void stopRecording() {
  const uint32_t secs = recorder::stop();
  const bool err = recorder::hadError();
  refreshNotes();
  if (err) {
    showMessage("저장 중 문제가 있었어요", "일부 소리가 빠졌을 수 있어요", Page::Main);
    Serial.printf("[rec] saved #%03d with errors (%lu s)\n", recNumber, (unsigned long)secs);
    return;
  }
  Serial.printf("[rec] saved #%03d (%lu s)\n", recNumber, (unsigned long)secs);
  goMain();
}

static void updateRecordingScreen() {
  peakSinceDraw = max(peakSinceDraw, recorder::level());
  const uint32_t sec = recorder::elapsedSeconds();
  if (sec == lastDrawnSec) return;
  lastDrawnSec = sec;
  memmove(levels, levels + 1, kLevelCount - 1);
  levels[kLevelCount - 1] = peakSinceDraw;
  peakSinceDraw = 0;
  screen::showRecording(recNumber, sec, levels, kLevelCount, freeMin, true);
}

// ---------- 시작 ----------
void setup() {
  pinMode(PIN_VBAT_HOLD, OUTPUT);
  digitalWrite(PIN_VBAT_HOLD, HIGH); // 배터리로 켜져 있게 유지

  Serial.begin(115200);
  delay(300);
  Serial.println("[voicenote] boot step3 (detail + playback)");

  pinMode(PIN_BTN_TOP, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  screen::begin();            // 화면 전원 켬
  sdOk = storage::begin();
  recOk = recorder::begin();  // 오디오 전원 켬
  rtc::begin();               // 시계 칩은 위 전원들이 켜진 뒤에 읽어야 응답함 (공식 예제 순서)
  refreshNotes();
  goMain();
  Serial.printf("[voicenote] ready (sd=%d rec=%d)\n", sdOk, recOk);
}

void loop() {
  const Press t = readButton(top);
  const Press b = readButton(bottom);

  if (t != Press::None) Serial.printf("[btn] top %s\n", t == Press::Long ? "long" : "short");
  if (b != Press::None) Serial.printf("[btn] bottom %s\n", b == Press::Long ? "long" : "short");

  switch (page) {
    case Page::Main:
      if (t == Press::Short && sdOk && recOk) {
        startRecording();
      } else if (b == Press::Short) {
        goList();
      }
      break;

    case Page::Recording:
      if (t == Press::Short) {
        stopRecording();
      } else {
        updateRecordingScreen();
      }
      break;

    case Page::List:
      if (b == Press::Short) {
        selected = (selected + 1) % noteCount;
        screen::showList(rows, noteCount, selected, batteryPct(), true, kListTopHint); // 깜빡임 없이
      } else if (t == Press::Short) {
        openDetail();
      } else if (b == Press::Long) {
        goMain();
      }
      break;

    case Page::Detail:
      if (t == Press::Short) {
        startPlay();
      } else if (b == Press::Short) {
        showListAgain();
      }
      break;

    case Page::Playing:
      if (t == Press::Short) {
        recorder::playStop();
        openDetail();
      } else if (b == Press::Short) {
        recorder::playStop();
        showListAgain();
      } else if (!recorder::isPlaying()) {
        openDetail(); // 끝까지 재생됨
      } else {
        updatePlayingScreen();
      }
      break;

    case Page::Message:
      if (sdOk && (t != Press::None || b != Press::None)) {
        if (afterMessage == Page::Detail) {
          openDetail();
        } else {
          goMain();
        }
      }
      break;
  }
  delay(10);
}
