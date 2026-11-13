#include "recorder.h"

#include <ESP_I2S.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <freertos/ringbuf.h>

#include "es8311.h"
#include "pins.h"

namespace recorder {

static const uint32_t kRate = 16000;
static const uint32_t kBytesPerSec = kRate * 2;
static const size_t kReadChunk = 1024;        // 마이크에서 한 번에 읽는 양 (32ms)
static const size_t kWriteChunk = 16 * 1024;  // SD에 한 번에 쓰는 양
static const size_t kRingSize = 256 * 1024;   // 소리 임시 저장 (약 8초). SD가 잠깐 느려도 안 끊김

static I2SClass i2s;
static RingbufHandle_t ring = nullptr;
static File file;
static TaskHandle_t readerTask = nullptr;
static TaskHandle_t writerTask = nullptr;

static volatile bool running = false;
static volatile bool readerDone = false;
static volatile bool writerDone = false;
static volatile bool error = false;
static volatile uint32_t bytesWritten = 0;
static volatile uint8_t currentLevel = 0;
static uint32_t startedAt = 0;

static void writeWavHeader(File &f, uint32_t dataBytes) {
  uint8_t h[44];
  auto put32 = [&](int at, uint32_t v) {
    h[at] = v;
    h[at + 1] = v >> 8;
    h[at + 2] = v >> 16;
    h[at + 3] = v >> 24;
  };
  auto put16 = [&](int at, uint16_t v) {
    h[at] = v;
    h[at + 1] = v >> 8;
  };
  memcpy(h, "RIFF", 4);
  put32(4, 36 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1); // PCM
  put16(22, 1); // 모노
  put32(24, kRate);
  put32(28, kBytesPerSec);
  put16(32, 2);
  put16(34, 16);
  memcpy(h + 36, "data", 4);
  put32(40, dataBytes);
  f.seek(0);
  f.write(h, sizeof(h));
}

// 마이크 → 임시 저장소
static void readerLoop(void *) {
  static int16_t buf[kReadChunk / 2];
  while (running) {
    const size_t got = i2s.readBytes((char *)buf, kReadChunk);
    if (got == 0) continue;
    // 소리 크기 (화면 막대용): 가장 큰 값 기준
    int peak = 0;
    for (size_t i = 0; i < got / 2; i++) peak = max(peak, abs((int)buf[i]));
    currentLevel = min(100, peak * 100 / 12000);
    if (xRingbufferSend(ring, buf, got, pdMS_TO_TICKS(50)) != pdTRUE) {
      error = true; // SD가 너무 느려서 임시 저장소가 꽉 참
    }
  }
  readerDone = true;
  vTaskDelete(nullptr);
}

// 임시 저장소 → SD카드
static void writerLoop(void *) {
  static uint8_t out[kWriteChunk];
  size_t filled = 0;
  for (;;) {
    size_t len = 0;
    uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(ring, &len, pdMS_TO_TICKS(100), kWriteChunk - filled);
    if (item) {
      memcpy(out + filled, item, len);
      filled += len;
      vRingbufferReturnItem(ring, item);
    }
    const bool finishing = readerDone && !item;
    if (filled == kWriteChunk || (finishing && filled > 0)) {
      if (file.write(out, filled) != filled) error = true;
      bytesWritten += filled;
      filled = 0;
    }
    if (finishing) break;
  }
  writerDone = true;
  vTaskDelete(nullptr);
}

bool begin() {
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, LOW); // 오디오 전원 켬
  pinMode(PIN_SPK_AMP, OUTPUT);
  digitalWrite(PIN_SPK_AMP, LOW); // 녹음 땐 스피커 끔 (잡음 방지)
  delay(20);

  es8311_handle_t es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  if (!es) {
    Serial.println("[rec] es8311 create failed");
    return false;
  }
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = (int)(kRate * 256),
      .sample_frequency = (int)kRate,
  };
  if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
    Serial.println("[rec] es8311 init failed");
    return false;
  }
  es8311_microphone_config(es, false);
  es8311_microphone_gain_set(es, ES8311_MIC_GAIN_24DB);

  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("[rec] i2s begin failed");
    return false;
  }

  ring = xRingbufferCreateWithCaps(kRingSize, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
  if (!ring) {
    Serial.println("[rec] ring buffer alloc failed");
    return false;
  }
  Serial.println("[rec] ready");
  return true;
}

bool start(const char *path) {
  if (running) return false;
  file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("[rec] cannot open %s\n", path);
    return false;
  }
  writeWavHeader(file, 0); // 자리만 잡아 두고, 끝날 때 길이를 채운다
  bytesWritten = 0;
  error = false;
  readerDone = writerDone = false;
  currentLevel = 0;

  // 시작 직전 마이크에 쌓인 옛 소리 버리기
  char junk[512];
  for (int i = 0; i < 8; i++) i2s.readBytes(junk, sizeof(junk));

  running = true;
  startedAt = millis();
  xTaskCreatePinnedToCore(readerLoop, "rec_read", 4096, nullptr, 10, &readerTask, 0);
  xTaskCreatePinnedToCore(writerLoop, "rec_write", 4096, nullptr, 5, &writerTask, 0);
  Serial.printf("[rec] start %s\n", path);
  return true;
}

uint32_t stop() {
  if (!running) return 0;
  running = false;
  while (!readerDone || !writerDone) delay(5);
  writeWavHeader(file, bytesWritten);
  file.close();
  const uint32_t secs = bytesWritten / kBytesPerSec;
  Serial.printf("[rec] stop: %lu bytes, %lu s, error=%d\n", (unsigned long)bytesWritten, (unsigned long)secs,
                error ? 1 : 0);
  return secs;
}

bool isRecording() { return running; }
uint32_t elapsedSeconds() { return running ? (millis() - startedAt) / 1000 : 0; }
uint8_t level() { return currentLevel; }
bool hadError() { return error; }

} // namespace recorder
