#include "stt.h"

#include <NetworkClientSecure.h>
#include <SD_MMC.h>

#include "certs.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h 가 없어요. 먼저 tools/setup_secrets.sh 를 실행하세요."
#endif

namespace stt {

static const char *kHost = "api.openai.com";
static const char *kBoundary = "----voicenoteBoundary7f3a";
static const char *kTmpPath = "/notes/convert.tmp";
static const uint32_t kMaxPartSeconds = 10 * 60;
static const uint32_t kMaxPartBytes = 24000000; // 25MB 제한보다 조금 작게
static const uint32_t kWaitResultMs = 180000;   // 다 보낸 뒤 결과 기다리는 최대 시간

static char err[96] = "";
static uint8_t buf[8192];

const char *lastError() { return err; }

// ---------- WAV 읽기 ----------
struct WavInfo {
  uint16_t format, channels, bits;
  uint32_t rate, dataOffset, dataBytes;
};

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }

static bool readWavInfo(File &f, WavInfo &w) {
  uint8_t h[12];
  f.seek(0);
  if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
  bool haveFmt = false;
  while (f.available() >= 8) {
    uint8_t ch[8];
    f.read(ch, 8);
    const uint32_t len = rd32(ch + 4);
    if (!memcmp(ch, "fmt ", 4)) {
      uint8_t fm[16];
      if (len < 16 || f.read(fm, 16) != 16) return false;
      w.format = rd16(fm);
      w.channels = rd16(fm + 2);
      w.rate = rd32(fm + 4);
      w.bits = rd16(fm + 14);
      f.seek(f.position() + len - 16 + (len & 1));
      haveFmt = true;
    } else if (!memcmp(ch, "data", 4)) {
      w.dataOffset = f.position();
      const uint32_t rest = f.size() - w.dataOffset;
      w.dataBytes = (len && len <= rest) ? len : rest; // 녹음 중 꺼져서 길이가 0이어도 살림
      return haveFmt;
    } else {
      f.seek(f.position() + len + (len & 1));
    }
  }
  return false;
}

static void makeWavHeader(uint8_t *h, const WavInfo &w, uint32_t dataBytes) {
  auto put32 = [&](int o, uint32_t v) { h[o] = v; h[o + 1] = v >> 8; h[o + 2] = v >> 16; h[o + 3] = v >> 24; };
  auto put16 = [&](int o, uint16_t v) { h[o] = v; h[o + 1] = v >> 8; };
  memcpy(h, "RIFF", 4);
  put32(4, 36 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1);
  put16(22, w.channels);
  put32(24, w.rate);
  put32(28, w.rate * w.channels * w.bits / 8);
  put16(32, w.channels * w.bits / 8);
  put16(34, w.bits);
  memcpy(h + 36, "data", 4);
  put32(40, dataBytes);
}

// ---------- 보내기 ----------
static bool writeAll(NetworkClientSecure &c, const uint8_t *p, size_t n) {
  uint32_t lastOk = millis();
  while (n > 0) {
    const size_t w = c.write(p, n);
    if (w > 0) {
      p += w;
      n -= w;
      lastOk = millis();
    } else if (!c.connected() || millis() - lastOk > 20000) {
      return false;
    } else {
      delay(5);
    }
  }
  return true;
}

// 한 줄 읽기 (\r\n 제외). 기다리는 동안 cb 로 그만둘 수 있게 함
static int readLine(NetworkClientSecure &c, String &line, uint32_t deadline, Progress cb, uint32_t total) {
  line = "";
  uint32_t lastCb = 0;
  while (true) {
    while (c.available()) {
      const char ch = c.read();
      if (ch == '\n') {
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        return 1;
      }
      line += ch;
    }
    if (!c.connected()) return line.length() ? 1 : 0;
    if ((int32_t)(millis() - deadline) > 0) return 0;
    if (cb && millis() - lastCb > 200) {
      lastCb = millis();
      if (!cb(total, total)) return -1;
    }
    delay(5);
  }
}

static bool readN(NetworkClientSecure &c, String &out, size_t n, uint32_t deadline) {
  while (n > 0) {
    if (c.available()) {
      const int r = c.read(buf, min(n, sizeof(buf) - 1));
      if (r > 0) {
        buf[r] = 0;
        out.concat((const char *)buf, r);
        n -= r;
        continue;
      }
    }
    if (!c.connected() && !c.available()) return false;
    if ((int32_t)(millis() - deadline) > 0) return false;
    delay(5);
  }
  return true;
}

// 녹음 한 조각을 보내고 글을 받는다
static Result sendPart(File &f, const WavInfo &w, uint32_t offset, uint32_t len, uint32_t &done, uint32_t total,
                       Progress cb, String &text) {
  NetworkClientSecure c;
  c.setCACert(kRootCerts);
  c.setHandshakeTimeout(30);
  if (!c.connect(kHost, 443)) {
    snprintf(err, sizeof(err), "서버 연결 실패");
    return Result::Failed;
  }

  String pre;
  pre.reserve(512);
  auto field = [&](const char *name, const char *value) {
    pre += "--";
    pre += kBoundary;
    pre += "\r\nContent-Disposition: form-data; name=\"";
    pre += name;
    pre += "\"\r\n\r\n";
    pre += value;
    pre += "\r\n";
  };
  field("model", "whisper-1");
  field("language", "ko");
  field("response_format", "text");
  pre += "--";
  pre += kBoundary;
  pre += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"note.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String post = String("\r\n--") + kBoundary + "--\r\n";
  const uint32_t contentLength = pre.length() + 44 + len + post.length();

  String head;
  head.reserve(400);
  head += "POST /v1/audio/transcriptions HTTP/1.1\r\nHost: ";
  head += kHost;
  head += "\r\nAuthorization: Bearer " OPENAI_API_KEY "\r\n";
  head += "Content-Type: multipart/form-data; boundary=";
  head += kBoundary;
  head += "\r\nContent-Length: ";
  head += contentLength;
  head += "\r\nConnection: close\r\n\r\n";

  uint8_t wh[44];
  makeWavHeader(wh, w, len);
  if (!writeAll(c, (const uint8_t *)head.c_str(), head.length()) ||
      !writeAll(c, (const uint8_t *)pre.c_str(), pre.length()) || !writeAll(c, wh, 44)) {
    snprintf(err, sizeof(err), "보내기 실패 (시작)");
    return Result::Failed;
  }

  // 녹음 본문: SD에서 조금씩 읽어 바로 보냄 (메모리에 다 올리지 않음)
  f.seek(offset);
  uint32_t left = len;
  while (left > 0) {
    const size_t n = f.read(buf, min(left, (uint32_t)sizeof(buf)));
    if (n == 0) {
      snprintf(err, sizeof(err), "SD 읽기 실패");
      return Result::Failed;
    }
    if (!writeAll(c, buf, n)) {
      snprintf(err, sizeof(err), "보내기 끊김 (%lu KB 남음)", (unsigned long)(left / 1024));
      return Result::Failed;
    }
    left -= n;
    done += n;
    if (cb && !cb(done, total)) return Result::Cancelled;
  }
  if (!writeAll(c, (const uint8_t *)post.c_str(), post.length())) {
    snprintf(err, sizeof(err), "보내기 실패 (끝)");
    return Result::Failed;
  }

  // 응답 읽기
  const uint32_t deadline = millis() + kWaitResultMs;
  String line;
  int r = readLine(c, line, deadline, cb, total);
  if (r < 0) return Result::Cancelled;
  if (r == 0) {
    snprintf(err, sizeof(err), "응답 없음");
    return Result::Failed;
  }
  int status = 0;
  sscanf(line.c_str(), "HTTP/%*s %d", &status);
  bool chunked = false;
  long contentLen = -1;
  while (true) {
    r = readLine(c, line, deadline, cb, total);
    if (r < 0) return Result::Cancelled;
    if (r == 0 || line.length() == 0) break;
    String low = line;
    low.toLowerCase();
    if (low.startsWith("transfer-encoding:") && low.indexOf("chunked") > 0) chunked = true;
    if (low.startsWith("content-length:")) contentLen = low.substring(15).toInt();
  }

  String body;
  if (chunked) {
    while (true) {
      if (readLine(c, line, deadline, nullptr, 0) <= 0) break;
      const long size = strtol(line.c_str(), nullptr, 16);
      if (size <= 0) break;
      if (!readN(c, body, size, deadline)) break;
      readLine(c, line, deadline, nullptr, 0); // 조각 끝의 빈 줄
    }
  } else if (contentLen >= 0) {
    readN(c, body, contentLen, deadline);
  } else {
    while (c.connected() || c.available()) {
      if (!readN(c, body, 1, deadline)) break;
    }
  }
  c.stop();

  if (status != 200) {
    snprintf(err, sizeof(err), "HTTP %d", status);
    Serial.printf("[stt] error %d: %.300s\n", status, body.c_str());
    return Result::Failed;
  }
  body.trim();
  text = body;
  return Result::Ok;
}

Result transcribe(const char *wavPath, const char *txtPath, Progress cb) {
  err[0] = 0;
  File f = SD_MMC.open(wavPath, FILE_READ);
  if (!f) {
    snprintf(err, sizeof(err), "파일 열기 실패");
    return Result::Failed;
  }
  WavInfo w;
  if (!readWavInfo(f, w) || w.format != 1 || w.rate == 0 || w.channels == 0 || w.bits == 0) {
    f.close();
    snprintf(err, sizeof(err), "WAV 형식이 아님");
    return Result::Failed;
  }
  const uint32_t frame = w.channels * w.bits / 8;
  uint32_t partBytes = min(kMaxPartSeconds * w.rate * frame, kMaxPartBytes - 44);
  partBytes -= partBytes % frame;
  const uint32_t parts = (w.dataBytes + partBytes - 1) / partBytes;
  if (parts == 0) {
    f.close();
    snprintf(err, sizeof(err), "소리가 없음");
    return Result::Failed;
  }

  SD_MMC.remove(kTmpPath);
  File out = SD_MMC.open(kTmpPath, FILE_WRITE);
  if (!out) {
    f.close();
    snprintf(err, sizeof(err), "글 파일 만들기 실패");
    return Result::Failed;
  }

  Serial.printf("[stt] %s: %lu bytes, %lu part(s)\n", wavPath, (unsigned long)w.dataBytes, (unsigned long)parts);
  uint32_t done = 0;
  Result res = Result::Ok;
  for (uint32_t i = 0; i < parts && res == Result::Ok; i++) {
    const uint32_t off = i * partBytes;
    const uint32_t len = min(partBytes, w.dataBytes - off);
    String text;
    res = sendPart(f, w, w.dataOffset + off, len, done, w.dataBytes, cb, text);
    if (res == Result::Ok) {
      if (i > 0) out.print("\n");
      out.print(text);
      Serial.printf("[stt] part %lu/%lu ok (%u chars)\n", (unsigned long)(i + 1), (unsigned long)parts, text.length());
    }
  }
  out.close();
  f.close();

  if (res != Result::Ok) {
    SD_MMC.remove(kTmpPath);
    if (res == Result::Failed) Serial.printf("[stt] failed: %s\n", err);
    return res;
  }
  SD_MMC.remove(txtPath);
  if (!SD_MMC.rename(kTmpPath, txtPath)) {
    snprintf(err, sizeof(err), "글 파일 저장 실패");
    return Result::Failed;
  }
  return Result::Ok;
}

} // namespace stt
