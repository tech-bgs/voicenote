// VOICE NOTE 펌웨어 — 4단계(1): 녹음 + 목록 + 재생 + 전송 모드(와이파이 연결, 접속 주소 안내)
// 아직 글자 변환·웹 목록은 없음.

#include <Wire.h>

#include "input.h"
#include "net.h"
#include "pins.h"
#include "recorder.h"
#include "rtc.h"
#include "screen.h"
#include "storage.h"

enum class Page { Main, List, Detail, Playing, Recording, Message, Connecting, Ready, NoWifi };

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

// ---------- 전송 모드 (메인에서 ▼ 길게) ----------
static const uint32_t kConnectTimeoutMs = 15000; // 와이파이 하나당 기다리는 시간
static const uint32_t kReadyIdleMs = 5UL * 60 * 1000; // 폰이 5분 동안 안 열면 와이파이 끔
static net::Wifi tryList[2];
static int tryCount = 0;
static int tryIndex = 0;
static uint32_t tryStartedAt = 0;
static uint32_t readyAt = 0;

static const int kStepCount = 3;
static screen::Step steps[kStepCount];
static const char *stepLabels[kStepCount];

static void drawSteps(bool partial) {
  char clock[8];
  rtc::timeText(clock, sizeof(clock));
  screen::showTransferSteps(steps, stepLabels, kStepCount, -1, clock, partial);
}

static void tryNextWifi() {
  const net::Wifi w = tryList[tryIndex];
  net::startConnect(w);
  tryStartedAt = millis();
  stepLabels[0] = (w == net::Wifi::Home) ? "와이파이 찾는 중" : "핫스팟 찾는 중";
}

static void enterTransfer() {
  tryCount = 0;
  if (net::hasSettings(net::Wifi::Home)) tryList[tryCount++] = net::Wifi::Home;
  if (net::hasSettings(net::Wifi::Phone)) tryList[tryCount++] = net::Wifi::Phone;
  char clock[8];
  rtc::timeText(clock, sizeof(clock));
  if (tryCount == 0) {
    page = Page::NoWifi;
    screen::showNoWifi(clock);
    return;
  }
  page = Page::Connecting;
  steps[0] = screen::Step::Running;
  steps[1] = screen::Step::Todo;
  steps[2] = screen::Step::Todo;
  stepLabels[1] = "글자로 변환";
  stepLabels[2] = "폰에서 보기 준비";
  tryIndex = 0;
  tryNextWifi();
  drawSteps(false);
}

static void leaveTransfer() {
  net::stop();
  goMain();
}

static void showReady() {
  page = Page::Ready;
  readyAt = millis();
  char clock[8], line1[32];
  rtc::timeText(clock, sizeof(clock));
  snprintf(line1, sizeof(line1), "녹음 %d개", noteCount);
  screen::showTransferReady("voicenote.local", net::ipText(), line1, "5분 뒤 와이파이를 꺼요", clock);
}

static void updateConnecting() {
  if (net::isConnected()) {
    steps[0] = screen::Step::Done;
    stepLabels[0] = (tryList[tryIndex] == net::Wifi::Home) ? "와이파이: 집" : "와이파이: 휴대폰";
    steps[1] = screen::Step::Done; // 변환은 다음 단계에서. 지금은 건너뜀
    steps[2] = screen::Step::Running;
    drawSteps(true);
    net::startServer();
    showReady();
    return;
  }
  if (millis() - tryStartedAt < kConnectTimeoutMs) return;
  if (++tryIndex < tryCount) {
    tryNextWifi();
    drawSteps(true);
    return;
  }
  net::stop();
  page = Page::NoWifi;
  char clock[8];
  rtc::timeText(clock, sizeof(clock));
  screen::showNoWifi(clock);
}

// ---------- 시작 ----------
void setup() {
  pinMode(PIN_VBAT_HOLD, OUTPUT);
  digitalWrite(PIN_VBAT_HOLD, HIGH); // 배터리로 켜져 있게 유지

  Serial.begin(115200);
  delay(300);
  Serial.println("[voicenote] boot step4-1 (wifi + transfer screens)");

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
  const Press t = readButton(top, false); // 위 버튼은 길게 누르기 없음 → 누르는 순간 반응
  const bool bottomHasLong = (page == Page::Main || page == Page::List);
  const Press b = readButton(bottom, bottomHasLong);

  if (t != Press::None) Serial.printf("[btn] top %s\n", t == Press::Long ? "long" : "short");
  if (b != Press::None) Serial.printf("[btn] bottom %s\n", b == Press::Long ? "long" : "short");

  switch (page) {
    case Page::Main:
      if (t == Press::Short && sdOk && recOk) {
        startRecording();
      } else if (b == Press::Short) {
        goList();
      } else if (b == Press::Long && sdOk) {
        enterTransfer();
      }
      break;

    case Page::Connecting:
      if (b == Press::Short) {
        leaveTransfer();
      } else {
        updateConnecting();
      }
      break;

    case Page::Ready: {
      net::handle();
      const uint32_t since = max(readyAt, net::lastVisitMs());
      if (b == Press::Short || millis() - since > kReadyIdleMs) leaveTransfer();
      break;
    }

    case Page::NoWifi:
      if (t == Press::Short) {
        enterTransfer();
      } else if (b == Press::Short) {
        goMain();
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
