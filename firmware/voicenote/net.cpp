#include "net.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

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

// 임시 첫 페이지. 녹음 목록·글 보기는 다음 단계에서 만든다.
static void handleRoot() {
  lastVisit = millis();
  server.send(200, "text/html; charset=utf-8",
              "<!doctype html><html><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>VOICE NOTE</title></head>"
              "<body style='font-family:sans-serif;padding:24px'>"
              "<h2>VOICE NOTE</h2><p>연결됐어요.</p>"
              "<p>녹음 목록은 다음 단계에서 이 페이지에 나와요.</p>"
              "</body></html>");
}

bool startServer() {
  snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());
  const bool mdnsOk = MDNS.begin("voicenote");
  if (mdnsOk) MDNS.addService("http", "tcp", 80);
  static bool routesAdded = false; // 두 번째 전송 모드에서 같은 주소를 또 등록하지 않게
  if (!routesAdded) {
    server.on("/", handleRoot);
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
