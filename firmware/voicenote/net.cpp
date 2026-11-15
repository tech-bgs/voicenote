#include "net.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <SD_MMC.h>
#include <WiFi.h>

#include "storage.h"
#include "web_page.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h 가 없어요. 먼저 tools/setup_secrets.sh 를 실행하세요."
#endif

namespace net {

static WebServer server(80);
static bool serverOn = false;
static char ip[16] = "";
static uint32_t lastVisit = 0;

bool hasSettings(Wifi w) {
  return strlen(w == Wifi::Home ? WIFI_HOME_SSID : WIFI_PHONE_SSID) > 0;
}

void startConnect(Wifi w) {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (w == Wifi::Home) {
    WiFi.begin(WIFI_HOME_SSID, WIFI_HOME_PASS);
  } else {
    WiFi.begin(WIFI_PHONE_SSID, WIFI_PHONE_PASS);
  }
  Serial.printf("[net] connecting (%s)\n", w == Wifi::Home ? "home" : "phone");
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

// ---------- 웹페이지 ----------
static uint8_t buf[4096];
static storage::Note list[200];

static void handleRoot() {
  lastVisit = millis();
  server.send(200, "text/html; charset=utf-8", kIndexHtml);
}

// JSON 문자열 안에 넣을 수 있게 " \ 와 제어 문자를 바꿈
static void appendJsonString(String &out, const char *s) {
  out += '"';
  for (; *s; s++) {
    const char c = *s;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  out += '"';
}

// 녹음 목록: [{"n":13,"when":1763190600,"sec":31,"conv":true,"prev":"글 앞부분"}, ...]
static void handleNotes() {
  lastVisit = millis();
  const int count = storage::list(list, 200);
  String json;
  json.reserve(count * 200 + 4);
  char prev[160]; // 한글 50자쯤
  json += "[";
  for (int i = 0; i < count; i++) {
    if (i) json += ",";
    json += "{\"n\":";
    json += list[i].number;
    json += ",\"when\":";
    json += (uint32_t)list[i].when;
    json += ",\"sec\":";
    json += list[i].seconds;
    json += ",\"conv\":";
    json += list[i].converted ? "true" : "false";
    json += ",\"prev\":";
    if (!list[i].converted || !storage::textPreview(list[i].number, prev, sizeof(prev))) prev[0] = 0;
    appendJsonString(json, prev);
    json += "}";
  }
  json += "]";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

static int argNumber() { return server.hasArg("n") ? server.arg("n").toInt() : -1; }

// 변환된 글. download=true 면 파일로 저장되게
static void sendText(bool download) {
  lastVisit = millis();
  const int n = argNumber();
  char path[48];
  if (n < 0 || !storage::textPath(n, path, sizeof(path))) {
    server.send(404, "text/plain; charset=utf-8", "글이 없어요");
    return;
  }
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    server.send(500, "text/plain; charset=utf-8", "파일을 못 열었어요");
    return;
  }
  if (download) {
    char cd[64];
    snprintf(cd, sizeof(cd), "attachment; filename=\"voicenote_%03d.txt\"", n);
    server.sendHeader("Content-Disposition", cd);
  }
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(f, "text/plain; charset=utf-8");
  f.close();
}

// 녹음 듣기. 아이폰은 파일을 조각(Range)으로 달라고 해서 그것도 처리
static void handleAudio() {
  lastVisit = millis();
  const int n = argNumber();
  char path[48];
  storage::wavPath(n, path, sizeof(path));
  File f = n >= 0 ? SD_MMC.open(path, FILE_READ) : File();
  if (!f) {
    server.send(404, "text/plain; charset=utf-8", "녹음이 없어요");
    return;
  }
  const uint32_t size = f.size();
  uint32_t from = 0, to = size ? size - 1 : 0;
  bool partial = false;
  const String range = server.header("Range");
  if (range.startsWith("bytes=") && size > 0) {
    const int dash = range.indexOf('-');
    const String a = range.substring(6, dash), b = range.substring(dash + 1);
    if (a.length()) {
      from = a.toInt();
      if (b.length()) to = min((uint32_t)b.toInt(), size - 1);
    } else if (b.length()) { // "bytes=-500" = 끝에서 500바이트
      from = size - min((uint32_t)b.toInt(), size);
    }
    if (from > to) {
      server.sendHeader("Content-Range", String("bytes */") + size);
      server.send(416, "text/plain", "");
      f.close();
      return;
    }
    partial = true;
  }
  const uint32_t len = size ? to - from + 1 : 0;
  server.sendHeader("Accept-Ranges", "bytes");
  if (partial) {
    char cr[64];
    snprintf(cr, sizeof(cr), "bytes %lu-%lu/%lu", (unsigned long)from, (unsigned long)to, (unsigned long)size);
    server.sendHeader("Content-Range", cr);
  }
  server.setContentLength(len);
  server.send(partial ? 206 : 200, "audio/wav", "");
  f.seek(from);
  NetworkClient client = server.client();
  uint32_t left = len;
  while (left > 0 && client.connected()) {
    const size_t r = f.read(buf, min(left, (uint32_t)sizeof(buf)));
    if (r == 0) break;
    if (client.write(buf, r) != r) break;
    left -= r;
  }
  f.close();
}

bool startServer() {
  snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());
  const bool mdnsOk = MDNS.begin("voicenote");
  if (mdnsOk) MDNS.addService("http", "tcp", 80);
  static bool routesAdded = false; // 두 번째 전송 모드에서 같은 주소를 또 등록하지 않게
  if (!routesAdded) {
    server.on("/", handleRoot);
    server.on("/api/notes", handleNotes);
    server.on("/api/text", [] { sendText(false); });
    server.on("/txt", [] { sendText(true); });
    server.on("/audio", handleAudio);
    static const char *headerKeys[] = {"Range"};
    server.collectHeaders(headerKeys, 1);
    server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "없는 페이지예요"); });
    routesAdded = true;
  }
  server.begin();
  serverOn = true;
  lastVisit = millis();
  Serial.printf("[net] ready ip=%s mdns=%d\n", ip, mdnsOk);
  return mdnsOk;
}

void handle() {
  if (serverOn) server.handleClient();
}

const char *ipText() { return ip; }
uint32_t lastVisitMs() { return lastVisit; }

void stop() {
  if (serverOn) {
    server.stop();
    MDNS.end();
    serverOn = false;
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  ip[0] = 0;
  Serial.println("[net] off");
}

} // namespace net
