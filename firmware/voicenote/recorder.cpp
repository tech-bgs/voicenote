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

// 마이크 감도. 녹음이 작으면 올리고, 소리가 찢어지면(클리핑) 내린다
static const es8311_mic_gain_t kMicGain = ES8311_MIC_GAIN_36DB;

static I2SClass i2s;
static es8311_handle_t es = nullptr;
static RingbufHandle_t ring = nullptr;
static File file;
static TaskHandle_t readerTask = nullptr;
static TaskHandle_t writerTask = nullptr;

static volatile bool running = false;
static volatile bool playDone = true; // 재생 작업이 끝났는지 (재생 코드는 아래쪽)
static volatile bool readerDone = false;
static volatile bool writerDone = false;
static volatile bool error = false;
static volatile uint32_t bytesWritten = 0;
static volatile uint8_t currentLevel = 0;
static volatile int maxPeak = 0;          // 이번 녹음에서 가장 큰 소리 (감도 맞출 때 참고)
static volatile uint32_t clippedSamples = 0; // 너무 커서 잘린 소리 개수
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
// 자동 음량 조절: 작은 말소리는 키우고(최대 8배), 큰 소리는 바로 줄여서 찢어지지 않게 한다
static const float kTargetPeak = 16000;  // 목표 크기 (최대의 약 절반)
static const float kMaxGain = 8.0f;      // 최대로 키우는 배수
static const float kSilenceGain = 2.0f;  // 조용할 땐 잡음이 커지지 않게 이 이상 안 키움
static const int kSilencePeak = 400;     // 이보다 작으면 '조용함'
static float gain = 1.0f;
static volatile float gainSum = 0;
static volatile uint32_t gainCount = 0;

static void applyAutoGain(int16_t *s, size_t n) {
  int rawPeak = 1;
  for (size_t i = 0; i < n; i++) rawPeak = max(rawPeak, abs((int)s[i]));
  float want = constrain(kTargetPeak / rawPeak, 1.0f, kMaxGain);
  if (rawPeak < kSilencePeak) want = min(want, kSilenceGain);
  if (want < gain) {
    gain = want; // 갑자기 큰 소리: 바로 줄임
  } else {
    gain += (want - gain) * 0.05f; // 조용해지면 천천히 키움 (약 0.6초)
  }
  for (size_t i = 0; i < n; i++) {
    const int v = (int)(s[i] * gain);
    s[i] = (int16_t)constrain(v, -32000, 32000);
  }
  gainSum += gain;
  gainCount++;
}

// 마이크 → 임시 저장소
static void readerLoop(void *) {
  static int16_t buf[kReadChunk / 2];
  while (running) {
    const size_t got = i2s.readBytes((char *)buf, kReadChunk);
    if (got == 0) continue;
    applyAutoGain(buf, got / 2);
    // 소리 크기 (화면 막대용): 가장 큰 값 기준
    int peak = 0;
    for (size_t i = 0; i < got / 2; i++) {
      const int a = abs((int)buf[i]);
      peak = max(peak, a);
      if (a >= 32000) clippedSamples++;
    }
    if (peak > maxPeak) maxPeak = peak;
    currentLevel = min(100, peak * 100 / 20000);
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

  es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
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
  es8311_microphone_gain_set(es, kMicGain);

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
  if (running || !playDone) return false;
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
  maxPeak = 0;
  clippedSamples = 0;
  gain = 1.0f;
  gainSum = 0;
  gainCount = 0;

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
  Serial.printf("[rec] stop: %lu bytes, %lu s, error=%d, peak=%d%%, clipped=%lu, avg gain x%.1f\n",
                (unsigned long)bytesWritten, (unsigned long)secs, error ? 1 : 0, maxPeak * 100 / 32767,
                (unsigned long)clippedSamples, gainCount ? gainSum / gainCount : 0.0f);
  return secs;
}

// ---------- 재생 (스피커) ----------

static const int kSpeakerVolume = 75; // 스피커 소리 크기 0~100
static File playFile;
static volatile bool playing = false;
static volatile uint32_t playDataBytes = 0;
static volatile uint32_t playedBytes = 0;

// WAV 파일 앞부분을 읽어 형식 확인 후 소리 데이터 시작 위치로 옮긴다
static bool openWav(File &f, uint32_t &dataBytes) {
  uint8_t h[12];
  if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
  bool fmtOk = false;
  for (int guard = 0; guard < 16; guard++) {
    uint8_t ch[8];
    if (f.read(ch, 8) != 8) return false;
    const uint32_t len = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((uint32_t)ch[7] << 24);
    if (!memcmp(ch, "fmt ", 4)) {
      uint8_t fm[16];
      if (len < 16 || f.read(fm, 16) != 16) return false;
      const uint16_t channels = fm[2] | (fm[3] << 8);
      const uint32_t rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | ((uint32_t)fm[7] << 24);
      const uint16_t bits = fm[14] | (fm[15] << 8);
      Serial.printf("[play] format %lu Hz, %u ch, %u bit\n", (unsigned long)rate, channels, bits);
      fmtOk = (rate == kRate && channels == 1 && bits == 16);
      if (len > 16) f.seek(f.position() + len - 16);
    } else if (!memcmp(ch, "data", 4)) {
      // 녹음 중 꺼져서 길이가 0으로 남은 파일은 파일 크기로 계산
      dataBytes = len ? len : (f.size() - f.position());
      dataBytes = min(dataBytes, (uint32_t)(f.size() - f.position()));
      return fmtOk;
    } else {
      f.seek(f.position() + len + (len & 1));
    }
  }
  return false;
}

static void playLoop(void *) {
  static uint8_t buf[4096];
  while (playing && playedBytes < playDataBytes) {
    const size_t want = min((uint32_t)sizeof(buf), playDataBytes - playedBytes);
    const size_t got = playFile.read(buf, want);
    if (got == 0) break;
    i2s.write(buf, got);
    playedBytes += got;
  }
  // 끝에 '딱' 소리 안 나게 조용한 소리를 조금 보낸다
  memset(buf, 0, sizeof(buf));
  i2s.write(buf, sizeof(buf));
  digitalWrite(PIN_SPK_AMP, LOW);
  playFile.close();
  playing = false;
  playDone = true;
  vTaskDelete(nullptr);
}

bool playStart(const char *path) {
  if (running || !playDone) return false;
  playFile = SD_MMC.open(path, FILE_READ);
  if (!playFile) {
    Serial.printf("[play] cannot open %s\n", path);
    return false;
  }
  uint32_t dataBytes = 0;
  if (!openWav(playFile, dataBytes)) {
    Serial.println("[play] unsupported wav");
    playFile.close();
    return false;
  }
  playDataBytes = dataBytes;
  playedBytes = 0;
  es8311_voice_mute(es, false);
  es8311_voice_volume_set(es, kSpeakerVolume, nullptr);
  digitalWrite(PIN_SPK_AMP, HIGH); // 스피커 앰프 켬
  playing = true;
  playDone = false;
  xTaskCreatePinnedToCore(playLoop, "play", 4096, nullptr, 10, nullptr, 0);
  Serial.printf("[play] start %s (%lu s)\n", path, (unsigned long)(dataBytes / kBytesPerSec));
  return true;
}

void playStop() {
  if (playDone) return;
  playing = false;
  while (!playDone) delay(5);
  Serial.println("[play] stop");
}

bool isPlaying() { return !playDone; }
uint32_t playPositionSeconds() { return playedBytes / kBytesPerSec; }
uint32_t playLengthSeconds() { return playDataBytes / kBytesPerSec; }

bool isRecording() { return running; }
uint32_t elapsedSeconds() { return running ? (millis() - startedAt) / 1000 : 0; }
uint8_t level() { return currentLevel; }
bool hadError() { return error; }

} // namespace recorder
