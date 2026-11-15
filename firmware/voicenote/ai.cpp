#include "ai.h"

#include <ArduinoJson.h>
#include <NetworkClientSecure.h>
#include <SD_MMC.h>

#include "certs.h"
#include "storage.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h 가 없어요. 먼저 tools/setup_secrets.sh 를 실행하세요."
#endif

namespace ai {

static const char *kOpenAI = "api.openai.com";
static const char *kAssembly = "api.assemblyai.com";
static const char *kSummaryModel = "gpt-6-luna"; // 2026-11 기준 OpenAI의 가장 싼 글 모델
static uint8_t buf[8192];

bool hasSpeakerKey() { return strlen(ASSEMBLYAI_API_KEY) > 0; }

void summaryPath(int n, char *out, size_t len) { snprintf(out, len, "/notes/rec_%03d.sum.txt", n); }
void speakersPath(int n, char *out, size_t len) { snprintf(out, len, "/notes/rec_%03d.spk.txt", n); }
void jobPath(int n, char *out, size_t len) { snprintf(out, len, "/notes/rec_%03d.job", n); }

// ---------- HTTPS 공통 ----------
// HTTP/1.0 으로 보내서 응답이 조각(chunked)으로 오지 않게 한다 → 바로 JSON으로 읽을 수 있음
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

static bool startRequest(NetworkClientSecure &c, const char *host, const char *method, const String &path,
                         const String &auth, const char *contentType, uint32_t contentLength) {
  c.setCACert(kRootCerts);
  c.setHandshakeTimeout(30);
  if (!c.connect(host, 443)) return false;
  String h;
  h.reserve(300);
  h += method;
  h += " ";
  h += path;
  h += " HTTP/1.0\r\nHost: ";
  h += host;
  h += "\r\n";
  h += auth;
  if (contentType) {
    h += "Content-Type: ";
    h += contentType;
    h += "\r\nContent-Length: ";
    h += contentLength;
    h += "\r\n";
  }
  h += "Connection: close\r\n\r\n";
  return writeAll(c, (const uint8_t *)h.c_str(), h.length());
}

static bool readLine(NetworkClientSecure &c, String &line, uint32_t deadline) {
  line = "";
  while ((int32_t)(millis() - deadline) < 0) {
    while (c.available()) {
      const char ch = c.read();
      if (ch == '\n') {
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        return true;
      }
      line += ch;
    }
    if (!c.connected() && !c.available()) return line.length() > 0;
    delay(5);
  }
  return false;
}

// 응답 첫 줄과 머리글을 읽고 상태 번호를 돌려줌 (0 = 응답 없음)
static int readHead(NetworkClientSecure &c, uint32_t waitMs) {
  const uint32_t deadline = millis() + waitMs;
  String line;
  if (!readLine(c, line, deadline)) return 0;
  int status = 0;
  sscanf(line.c_str(), "HTTP/%*s %d", &status);
  while (readLine(c, line, deadline) && line.length() > 0) {
  }
  c.setTimeout(10000); // 본문 읽을 때 기다리는 시간
  return status;
}

static bool readFile(const char *path, String &out) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

static bool writeFile(const char *path, const String &text) {
  const char *tmp = "/notes/ai.tmp";
  SD_MMC.remove(tmp);
  File f = SD_MMC.open(tmp, FILE_WRITE);
  if (!f) return false;
  const size_t w = f.print(text);
  f.close();
  if (w != text.length()) return false;
  SD_MMC.remove(path);
  return SD_MMC.rename(tmp, path);
}

// ---------- 요약 ----------
static const char *kSummaryPrompt =
    "다음은 음성 메모를 받아 적은 글이다. 한국어로 짧게 정리해라.\n"
    "형식:\n"
    "첫 줄: 한 줄 요약\n"
    "그 다음: '- '로 시작하는 핵심 내용 3~6개\n"
    "할 일이나 약속이 있으면 마지막에 '할 일:' 줄 아래 '- '로 적기 (없으면 생략)\n"
    "글에 없는 내용은 지어내지 말 것. 마크다운 굵은 글씨(**)는 쓰지 말 것.";

bool summarize(int n, String &err) {
  char path[48];
  String text;
  if (!storage::textPath(n, path, sizeof(path)) || !readFile(path, text) || text.length() == 0) {
    err = "글이 없어요";
    return false;
  }
  String body;
  {
    JsonDocument req;
    req["model"] = kSummaryModel;
    req["reasoning_effort"] = "low";
    JsonArray msgs = req["messages"].to<JsonArray>();
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = kSummaryPrompt;
    JsonObject user = msgs.add<JsonObject>();
    user["role"] = "user";
    user["content"] = text;
    text = String(); // 메모리 아끼기
    serializeJson(req, body);
  }

  NetworkClientSecure c;
  if (!startRequest(c, kOpenAI, "POST", "/v1/chat/completions", "Authorization: Bearer " OPENAI_API_KEY "\r\n",
                    "application/json", body.length()) ||
      !writeAll(c, (const uint8_t *)body.c_str(), body.length())) {
    err = "OpenAI 연결 실패";
    return false;
  }
  body = String();
  const int status = readHead(c, 90000);

  JsonDocument filter;
  filter["choices"][0]["message"]["content"] = true;
  filter["error"]["message"] = true;
  JsonDocument res;
  const DeserializationError je = deserializeJson(res, c, DeserializationOption::Filter(filter));
  c.stop();
  if (status != 200 || je) {
    err = String("OpenAI 오류 ") + status;
    const char *m = res["error"]["message"];
    if (m) Serial.printf("[ai] summary error %d: %.200s\n", status, m);
    else Serial.printf("[ai] summary error %d (%s)\n", status, je.c_str());
    return false;
  }
  String summary = res["choices"][0]["message"]["content"] | "";
  summary.trim();
  if (!summary.length()) {
    err = "요약이 비어 있어요";
    return false;
  }
  summaryPath(n, path, sizeof(path));
  if (!writeFile(path, summary)) {
    err = "요약 저장 실패";
    return false;
  }
  Serial.printf("[ai] summary #%03d ok (%u bytes)\n", n, summary.length());
  return true;
}

// 응답 본문 전체를 PSRAM에 받는다 (큰 응답을 바로 JSON으로 읽으면 중간에 끊기는 문제가 있었음)
// 다 받으면 길이를, 실패하면 -1. 다 쓴 뒤 free(*out)
static long readBodyToPsram(NetworkClientSecure &c, char **out, size_t cap) {
  char *mem = (char *)ps_malloc(cap + 1);
  if (!mem) return -1;
  size_t len = 0;
  uint32_t lastData = millis();
  while (millis() - lastData < 15000) {
    const int a = c.available();
    if (a > 0) {
      if (len >= cap) break; // 너무 큼
      const int r = c.read((uint8_t *)mem + len, min((size_t)a, cap - len));
      if (r > 0) {
        len += r;
        lastData = millis();
        continue;
      }
    } else if (!c.connected()) {
      break;
    }
    delay(2);
  }
  mem[len] = 0;
  *out = mem;
  return (long)len;
}

// ---------- 화자 나누기 ----------
static String assemblyAuth() { return String("authorization: ") + ASSEMBLYAI_API_KEY + "\r\n"; }

// ---------- 올릴 때 압축 (μ-law) ----------
// AssemblyAI로 가는 길은 기기 쪽 보내기 칸(6KB)과 거리 때문에 느리다(약 40KB/s).
// 16비트 소리를 8비트 μ-law(전화 음질)로 바꿔 크기를 반으로 줄여 보낸다. SD의 녹음은 그대로.
static const bool kCompressForSpeakers = true;

static uint8_t linearToUlaw(int16_t sample) {
  const int kBias = 0x84, kClip = 32635;
  int v = sample;
  const int sign = v < 0 ? 0x80 : 0;
  if (sign) v = -v;
  if (v > kClip) v = kClip;
  v += kBias;
  int exponent = 7;
  for (int mask = 0x4000; (v & mask) == 0 && exponent > 0; exponent--, mask >>= 1) {
  }
  const int mantissa = (v >> (exponent + 3)) & 0x0F;
  return ~(sign | (exponent << 4) | mantissa);
}

// 이 기기의 녹음(44바이트 머리 + 16비트 PCM)인지 확인하고 소리 정보를 꺼냄
static bool readPcmHeader(File &f, uint16_t &channels, uint32_t &rate, uint32_t &dataBytes) {
  uint8_t h[44];
  f.seek(0);
  if (f.read(h, 44) != 44) return false;
  if (memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVEfmt ", 8) || memcmp(h + 36, "data", 4)) return false;
  const uint16_t format = h[20] | (h[21] << 8);
  const uint16_t bits = h[34] | (h[35] << 8);
  channels = h[22] | (h[23] << 8);
  rate = h[24] | (h[25] << 8) | (h[26] << 16) | ((uint32_t)h[27] << 24);
  dataBytes = f.size() - 44; // 녹음 중 꺼져 머리의 길이가 틀려도 실제 크기로
  dataBytes -= dataBytes % (2 * channels);
  return format == 1 && bits == 16 && channels >= 1 && rate > 0;
}

// μ-law WAV 머리 (fmt 18바이트 + fact 청크) = 58바이트
static void makeUlawHeader(uint8_t *h, uint16_t channels, uint32_t rate, uint32_t samples) {
  auto put32 = [&](int o, uint32_t v) { h[o] = v; h[o + 1] = v >> 8; h[o + 2] = v >> 16; h[o + 3] = v >> 24; };
  auto put16 = [&](int o, uint16_t v) { h[o] = v; h[o + 1] = v >> 8; };
  const uint32_t dataBytes = samples; // 1바이트 = 1샘플
  memcpy(h, "RIFF", 4);
  put32(4, 50 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(16, 18);
  put16(20, 7); // μ-law
  put16(22, channels);
  put32(24, rate);
  put32(28, rate * channels);
  put16(32, channels);
  put16(34, 8);
  put16(36, 0);
  memcpy(h + 38, "fact", 4);
  put32(42, 4);
  put32(46, samples / channels);
  memcpy(h + 50, "data", 4);
  put32(54, dataBytes);
}

// 녹음 파일을 AssemblyAI에 올리고 작업 번호를 받아 저장
static bool startSpeakers(int n, String &err, Progress progress) {
  const uint32_t t0 = millis();
  char path[48];
  storage::wavPath(n, path, sizeof(path));
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    err = "녹음이 없어요";
    return false;
  }
  uint16_t channels = 1;
  uint32_t rate = 0, pcmBytes = 0;
  const bool compress = kCompressForSpeakers && readPcmHeader(f, channels, rate, pcmBytes);
  const uint32_t size = compress ? 58 + pcmBytes / 2 : f.size();
  String uploadUrl;
  {
    NetworkClientSecure c;
    if (!startRequest(c, kAssembly, "POST", "/v2/upload", assemblyAuth(), "application/octet-stream", size)) {
      f.close();
      err = "AssemblyAI 연결 실패";
      Serial.printf("[ai] speakers #%03d: upload connect failed\n", n);
      return false;
    }
    // SD에서 조금씩 읽어 바로 보냄 (압축하면 16비트 2바이트 → 1바이트)
    uint32_t left = size;
    if (compress) {
      uint8_t h[58];
      makeUlawHeader(h, channels, rate, pcmBytes / 2);
      if (!writeAll(c, h, sizeof(h))) left = 1;
      else left -= sizeof(h);
      f.seek(44);
    } else {
      f.seek(0);
    }
    while (left > 0) {
      size_t r;
      if (compress) {
        const size_t want = min((uint32_t)(sizeof(buf) / 2) * 2, left * 2);
        const size_t got = f.read(buf, want) & ~(size_t)1;
        r = got / 2;
        for (size_t i = 0; i < r; i++) buf[i] = linearToUlaw((int16_t)(buf[2 * i] | (buf[2 * i + 1] << 8)));
      } else {
        r = f.read(buf, min(left, (uint32_t)sizeof(buf)));
      }
      if (r == 0 || !writeAll(c, buf, r)) break;
      left -= r;
      if (progress && !progress(size - left, size)) {
        f.close();
        err = "그만둠";
        Serial.printf("[ai] speakers #%03d: upload cancelled\n", n);
        return false;
      }
    }
    f.close();
    if (left > 0) {
      err = "녹음 올리기 끊김";
      Serial.printf("[ai] speakers #%03d: upload cut (%lu bytes left)\n", n, (unsigned long)left);
      return false;
    }
    const int status = readHead(c, 60000);
    JsonDocument res;
    deserializeJson(res, c);
    c.stop();
    uploadUrl = res["upload_url"] | "";
    if (status != 200 || !uploadUrl.length()) {
      err = String("올리기 오류 ") + status;
      Serial.printf("[ai] upload error %d: %.200s\n", status, (const char *)(res["error"] | ""));
      return false;
    }
  }

  String body;
  {
    JsonDocument req;
    req["audio_url"] = uploadUrl;
    req["language_code"] = "ko";
    req["speech_models"].to<JsonArray>().add("universal-2"); // 한국어는 universal-2 에서 지원
    req["speaker_labels"] = true;
    serializeJson(req, body);
  }
  NetworkClientSecure c;
  if (!startRequest(c, kAssembly, "POST", "/v2/transcript", assemblyAuth(), "application/json", body.length()) ||
      !writeAll(c, (const uint8_t *)body.c_str(), body.length())) {
    err = "AssemblyAI 연결 실패";
    return false;
  }
  const int status = readHead(c, 30000);
  JsonDocument filter;
  filter["id"] = true;
  filter["error"] = true;
  JsonDocument res;
  deserializeJson(res, c, DeserializationOption::Filter(filter));
  c.stop();
  const char *id = res["id"];
  if (status != 200 || !id) {
    const char *m = res["error"] | "";
    err = String("작업 시작 오류 ") + status;
    if (strlen(m)) err += String(": ") + m;
    Serial.printf("[ai] submit error %d: %.200s\n", status, m);
    return false;
  }
  jobPath(n, path, sizeof(path));
  if (!writeFile(path, id)) {
    err = "작업 번호 저장 실패";
    return false;
  }
  Serial.printf("[ai] speakers #%03d started (%lu ms, %lu bytes, %s)\n", n, (unsigned long)(millis() - t0),
                (unsigned long)size, compress ? "ulaw" : "wav");
  return true;
}

// 작업 진행 확인. 끝났으면 화자별 글을 저장
static Speakers checkSpeakers(int n, const String &id, String &err) {
  NetworkClientSecure c;
  if (!startRequest(c, kAssembly, "GET", String("/v2/transcript/") + id, assemblyAuth(), nullptr, 0)) {
    // 잠깐 연결이 안 된 것일 수 있음 → 실패로 끝내지 않고 다음 확인 때 다시
    Serial.printf("[ai] speakers #%03d check: connect failed (retry)\n", n);
    return Speakers::Working;
  }
  const int status = readHead(c, 30000);
  JsonDocument filter;
  filter["status"] = true;
  filter["error"] = true;
  filter["utterances"][0]["speaker"] = true;
  filter["utterances"][0]["text"] = true;
  char *body = nullptr;
  const long bodyLen = readBodyToPsram(c, &body, 4 * 1024 * 1024);
  c.stop();
  JsonDocument res;
  DeserializationError je = DeserializationError::EmptyInput;
  if (bodyLen > 0) je = deserializeJson(res, body, bodyLen, DeserializationOption::Filter(filter));
  free(body);
  const String st = res["status"] | "";
  Serial.printf("[ai] speakers #%03d check: http %d, %ld bytes, status=%s, json=%s\n", n, status, bodyLen, st.c_str(),
                je.c_str());
  if (status == 0 || status == 429 || status >= 500 || (status == 200 && je)) {
    return Speakers::Working; // 일시적인 문제 → 다음 확인 때 다시
  }
  if (status != 200) {
    const char *m = res["error"] | "";
    err = String("확인 오류 ") + status + (strlen(m) ? String(": ") + m : String());
    char jp[48];
    jobPath(n, jp, sizeof(jp));
    SD_MMC.remove(jp); // 작업 번호가 잘못됨 → 다음엔 처음부터
    return Speakers::Failed;
  }
  if (st == "queued" || st == "processing") return Speakers::Working;

  char path[48];
  jobPath(n, path, sizeof(path));
  SD_MMC.remove(path); // 끝났거나 실패 → 작업 번호는 더 필요 없음
  if (st != "completed") {
    err = String("화자 나누기 실패: ") + (res["error"] | st.c_str());
    Serial.printf("[ai] speakers #%03d %s\n", n, err.c_str());
    return Speakers::Failed;
  }
  String out;
  for (JsonObject u : res["utterances"].as<JsonArray>()) {
    out += u["speaker"] | "?";
    out += ": ";
    out += u["text"] | "";
    out += "\n";
  }
  if (!out.length()) {
    err = "화자를 찾지 못했어요";
    Serial.printf("[ai] speakers #%03d: no utterances\n", n);
    return Speakers::Failed;
  }
  speakersPath(n, path, sizeof(path));
  if (!writeFile(path, out)) {
    err = "결과 저장 실패";
    Serial.printf("[ai] speakers #%03d: save failed\n", n);
    return Speakers::Failed;
  }
  Serial.printf("[ai] speakers #%03d done\n", n);
  return Speakers::Done;
}

Speakers speakers(int n, String &err, Progress progress) {
  if (!hasSpeakerKey()) {
    err = "AssemblyAI 키가 없어요";
    return Speakers::Failed;
  }
  char path[48];
  speakersPath(n, path, sizeof(path));
  if (SD_MMC.exists(path)) return Speakers::Done;
  jobPath(n, path, sizeof(path));
  String id;
  if (readFile(path, id) && id.length()) return checkSpeakers(n, id, err);
  return startSpeakers(n, err, progress) ? Speakers::Working : Speakers::Failed;
}

} // namespace ai
