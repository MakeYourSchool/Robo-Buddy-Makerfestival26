// MF26 - small companion robot on an ESP32-C6.
//   * tank drive with two continuous rotation servos + a dragwheel
//   * GC9A01 round display showing an animated face
//   * driven from a web page (virtual joystick / arrow keys) over a websocket
//   * standalone: opens its own WiFi, no server and no other network needed

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

#include "config.h"
#include "face.h"
#include "webpage.h"
#include "apidocs.h"

// ------------------------------------------------------------------ display --
static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, GFX_NOT_DEFINED);
static Arduino_GFX *gfx = new Arduino_GC9A01(
    bus, PIN_TFT_RST, TFT_ROTATION, true /* IPS */);
static Face face;

// -------------------------------------------------------------------- drive --
// The websocket callbacks run on the AsyncTCP task, loop() reads the result, so
// the shared command is guarded by a spinlock.
static portMUX_TYPE cmdMux = portMUX_INITIALIZER_UNLOCKED;
static volatile float cmdTurn = 0.0f;     // -1 (left) .. 1 (right)
static volatile float cmdFwd = 0.0f;      // -1 (back) .. 1 (forward)
static volatile float cmdScale = 0.60f;   // speed slider
static volatile int32_t trimLeft = 0;     // microseconds
static volatile int32_t trimRight = 0;
static volatile uint32_t lastCmdMs = 0;

static Preferences prefs;
static bool servosLive = false;
static uint32_t movingSinceIdleMs = 0;

static const int SERVO_RES_BITS = 16;

static void servoWriteUs(uint8_t pin, int us) {
  // 50 Hz -> 20000 us period
  uint32_t duty = (uint32_t)(((uint64_t)us << SERVO_RES_BITS) / 20000ULL);
  ledcWrite(pin, duty);
}

static void servosDetach() {
  ledcWrite(PIN_SERVO_LEFT, 0);
  ledcWrite(PIN_SERVO_RIGHT, 0);
  servosLive = false;
}

static void driveSetup() {
  ledcAttach(PIN_SERVO_LEFT, 50, SERVO_RES_BITS);
  ledcAttach(PIN_SERVO_RIGHT, 50, SERVO_RES_BITS);
  servosDetach();
}

// Mix a joystick vector into two wheel speeds and push them to the servos.
static void driveApply(float turn, float fwd, float scale) {
  float l = fwd + turn;
  float r = fwd - turn;
  // keep the ratio when the mix overshoots instead of clipping the turn away
  float m = max(fabsf(l), fabsf(r));
  if (m > 1.0f) { l /= m; r /= m; }
  l *= scale;
  r *= scale;

  if (fabsf(l) < 0.04f) l = 0;
  if (fabsf(r) < 0.04f) r = 0;

  uint32_t now = millis();
  if (l == 0 && r == 0) {
    // let the servos rest once they have had time to actually stop
    if (movingSinceIdleMs == 0) movingSinceIdleMs = now;
    if (now - movingSinceIdleMs > SERVO_IDLE_MS) {
      if (servosLive) servosDetach();
      return;  // stay quiet until there is something to do again
    }
  } else {
    movingSinceIdleMs = 0;
  }

  if (SERVO_LEFT_INVERT) l = -l;
  if (SERVO_RIGHT_INVERT) r = -r;

  servoWriteUs(PIN_SERVO_LEFT,
               SERVO_NEUTRAL_US + trimLeft + (int)(l * SERVO_SPAN_US));
  servoWriteUs(PIN_SERVO_RIGHT,
               SERVO_NEUTRAL_US + trimRight + (int)(r * SERVO_SPAN_US));
  servosLive = true;
}

// ------------------------------------------------------------------ network --
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// -------------------------------------------------------------- robot code --
// A short name for this robot, derived from the last three bytes of its WiFi
// MAC and encoded with Crockford base32 (no I, L, O or U, so it cannot be
// misread aloud). It names the robot's own WiFi network.
static char robotCode[8] = "";
static char apSsid[24] = "";

// Pairing walks a phone through two QR codes: first join the robot's WiFi,
// then open its control page. The face takes over after the first drive.
enum PairStep : uint8_t { PAIR_JOIN_WIFI, PAIR_OPEN_PAGE, PAIR_DONE };
static PairStep pairStep = PAIR_JOIN_WIFI;
static bool pairDrawn = false;               // current step already on screen
static volatile bool pairRequested = false;  // /api/pair, handled in loop()

// Cleared by the first command that actually moves the robot. Not by merely
// connecting, and not by a zero vector: the control page opens its websocket
// and starts sending "stopped" the moment it loads, either of which would wipe
// the pairing screen before anyone had finished with it.
static volatile bool driveSeen = false;

static inline void noteDrive(float x, float y) {
  if (fabsf(x) > 0.05f || fabsf(y) > 0.05f) driveSeen = true;
}

static void makeRobotCode() {
  // Read from eFuse: valid before WiFi starts, and the same station MAC the
  // networked build derives its code from, so a robot keeps its code.
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  uint32_t v = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];

  static const char *ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
  for (int i = 0; i < 5; i++) {
    robotCode[i] = ALPHABET[(v >> ((4 - i) * 5)) & 0x1F];
  }
  robotCode[5] = 0;
}

// ------------------------------------------------------------- face config --
static void faceSave(const FaceConfig &c) {
  prefs.putBytes("face", &c, sizeof(c));
}

static FaceConfig faceLoad() {
  FaceConfig c{};
  size_t n = prefs.getBytes("face", &c, sizeof(c));
  if (n != sizeof(c) || c.version != FACE_CONFIG_VERSION) c = faceConfigDefault();
  faceConfigClamp(c);
  return c;
}

static void faceToJson(const FaceConfig &c, JsonObject o) {
  o["eye"] = EYE_NAMES[c.eye];
  o["mouth"] = MOUTH_NAMES[c.mouth];
  o["brow"] = BROW_NAMES[c.brow];
  o["nose"] = NOSE_NAMES[c.nose];
  o["color"] = c.color;
  o["background"] = c.bg;
  o["eyeWidth"] = c.eyeW;
  o["eyeHeight"] = c.eyeH;
  o["eyeGap"] = c.eyeGap;
  o["eyeY"] = c.eyeY;
  o["mouthWidth"] = c.mouthW;
  o["mouthY"] = c.mouthY;
  o["thickness"] = c.thickness;
  o["blinkEvery"] = c.blinkEvery;
  o["lookEvery"] = c.lookEvery;
  o["autoBlink"] = (bool)c.autoBlink;
  o["autoLook"] = (bool)c.autoLook;
  o["tears"] = (bool)c.tears;
}

// Accepts a style either by name ("happy") or by index (4).
static bool styleFromJson(JsonVariantConst v, const char *const *names,
                          uint8_t count, uint8_t &out) {
  if (v.isNull()) return true;
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    for (uint8_t i = 0; i < count; i++) {
      if (strcasecmp(s, names[i]) == 0) { out = i; return true; }
    }
    return false;
  }
  if (v.is<int>()) {
    int i = v.as<int>();
    if (i < 0 || i >= count) return false;
    out = (uint8_t)i;
    return true;
  }
  return false;
}

static bool faceFromJson(JsonObjectConst o, FaceConfig &c, String &err) {
  if (!styleFromJson(o["eye"], EYE_NAMES, EYE_STYLE_COUNT, c.eye)) {
    err = "unknown eye style"; return false;
  }
  if (!styleFromJson(o["mouth"], MOUTH_NAMES, MOUTH_STYLE_COUNT, c.mouth)) {
    err = "unknown mouth style"; return false;
  }
  if (!styleFromJson(o["brow"], BROW_NAMES, BROW_STYLE_COUNT, c.brow)) {
    err = "unknown brow style"; return false;
  }
  if (!styleFromJson(o["nose"], NOSE_NAMES, NOSE_STYLE_COUNT, c.nose)) {
    err = "unknown nose style"; return false;
  }
  if (o["color"].is<int>())       c.color = (uint16_t)o["color"].as<int>();
  if (o["background"].is<int>())  c.bg = (uint16_t)o["background"].as<int>();
  if (o["eyeWidth"].is<int>())    c.eyeW = o["eyeWidth"];
  if (o["eyeHeight"].is<int>())   c.eyeH = o["eyeHeight"];
  if (o["eyeGap"].is<int>())      c.eyeGap = o["eyeGap"];
  if (o["eyeY"].is<int>())        c.eyeY = o["eyeY"];
  if (o["mouthWidth"].is<int>())  c.mouthW = o["mouthWidth"];
  if (o["mouthY"].is<int>())      c.mouthY = o["mouthY"];
  if (o["thickness"].is<int>())   c.thickness = o["thickness"];
  if (o["blinkEvery"].is<int>())  c.blinkEvery = o["blinkEvery"];
  if (o["lookEvery"].is<int>())   c.lookEvery = o["lookEvery"];
  if (o["autoBlink"].is<bool>())  c.autoBlink = o["autoBlink"] ? 1 : 0;
  if (o["autoLook"].is<bool>())   c.autoLook = o["autoLook"] ? 1 : 0;
  if (o["tears"].is<bool>())      c.tears = o["tears"] ? 1 : 0;
  faceConfigClamp(c);
  return true;
}

static void handleCommand(const char *msg) {
  int a = 0, b = 0;
  switch (msg[0]) {
    case 'D':  // D,<x*1000>,<y*1000>
      if (sscanf(msg + 1, ",%d,%d", &a, &b) == 2) {
        portENTER_CRITICAL(&cmdMux);
        cmdTurn = constrain(a / 1000.0f, -1.0f, 1.0f);
        cmdFwd = constrain(b / 1000.0f, -1.0f, 1.0f);
        lastCmdMs = millis();
        portEXIT_CRITICAL(&cmdMux);
        noteDrive(cmdTurn, cmdFwd);
      }
      break;
    case 'S':  // S,<percent>
      if (sscanf(msg + 1, ",%d", &a) == 1)
        cmdScale = constrain(a, 10, 100) / 100.0f;
      break;
    case 'T':  // T,<leftUs>,<rightUs>
      if (sscanf(msg + 1, ",%d,%d", &a, &b) == 2) {
        trimLeft = constrain(a, -120, 120);
        trimRight = constrain(b, -120, 120);
        prefs.putInt("trimL", trimLeft);
        prefs.putInt("trimR", trimRight);
      }
      break;
    default:
      break;
  }
}

static void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      break;
    case WS_EVT_DISCONNECT:
      portENTER_CRITICAL(&cmdMux);
      cmdTurn = 0;
      cmdFwd = 0;
      portEXIT_CRITICAL(&cmdMux);
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len &&
          info->opcode == WS_TEXT && len < 48) {
        char buf[48];
        memcpy(buf, data, len);
        buf[len] = 0;
        handleCommand(buf);
      }
      break;
    }
    default:
      break;
  }
}

// A WiFi login as a QR code: phones join the network straight from the camera.
// Backslash-escape the characters the format reserves.
static String wifiQr(const char *ssid, const char *pass) {
  auto esc = [](const char *in) {
    String out;
    for (const char *c = in; *c; c++) {
      if (strchr("\\;,:\"", *c)) out += '\\';
      out += *c;
    }
    return out;
  };
  return "WIFI:T:WPA;S:" + esc(ssid) + ";P:" + esc(pass) + ";;";
}

static void drawPairStep() {
  if (pairStep == PAIR_JOIN_WIFI) {
    String qr = wifiQr(apSsid, AP_PASS);
    face.showPairing("1. WLAN", qr.c_str(), "QR scannen", apSsid);
  } else {
    String ip = WiFi.softAPIP().toString();
    String url = "http://" + ip + "/";
    face.showPairing("2. Steuern", url.c_str(), "QR scannen", ip.c_str());
  }
}

void setup() {
  driveSetup();

  gfx->begin(TFT_SPI_HZ);
  face.begin(gfx);
  face.showMessage("MF26", "start...");

  prefs.begin("mf26", false);
  trimLeft = prefs.getInt("trimL", 0);
  trimRight = prefs.getInt("trimR", 0);
  face.setConfig(faceLoad());

  // Standalone: never look for another network, always open our own.
  makeRobotCode();
  snprintf(apSsid, sizeof(apSsid), "%s%s", AP_SSID_PREFIX, robotCode);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(apSsid, AP_PASS);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  // ------------------------------------------------------------------ API --
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument d;
    d["ip"] = WiFi.softAPIP().toString();
    d["mode"] = "ap";
    d["ssid"] = apSsid;
    d["stations"] = WiFi.softAPgetStationNum();
    d["code"] = robotCode;
    d["rssi"] = WiFi.RSSI();
    d["clients"] = ws.count();
    d["uptimeMs"] = millis();
    d["freeHeap"] = ESP.getFreeHeap();
    d["speed"] = (int)lroundf(cmdScale * 100);
    d["trimLeft"] = trimLeft;
    d["trimRight"] = trimRight;
    JsonObject drive = d["drive"].to<JsonObject>();
    drive["x"] = cmdTurn;
    drive["y"] = cmdFwd;
    String out;
    serializeJson(d, out);
    req->send(200, "application/json", out);
  });

  // Order matters: ESPAsyncWebServer matches a route as a prefix, so the
  // specific /api/face/* routes have to be registered before /api/face
  // or that handler answers them too.
  server.on("/api/face/options", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument d;
    JsonArray e = d["eye"].to<JsonArray>();
    for (uint8_t i = 0; i < EYE_STYLE_COUNT; i++) e.add(EYE_NAMES[i]);
    JsonArray m = d["mouth"].to<JsonArray>();
    for (uint8_t i = 0; i < MOUTH_STYLE_COUNT; i++) m.add(MOUTH_NAMES[i]);
    JsonArray b = d["brow"].to<JsonArray>();
    for (uint8_t i = 0; i < BROW_STYLE_COUNT; i++) b.add(BROW_NAMES[i]);
    JsonArray n = d["nose"].to<JsonArray>();
    for (uint8_t i = 0; i < NOSE_STYLE_COUNT; i++) n.add(NOSE_NAMES[i]);
    String out;
    serializeJson(d, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/face/reset", HTTP_POST, [](AsyncWebServerRequest *req) {
    FaceConfig c = faceConfigDefault();
    face.setConfig(c);
    faceSave(c);
    JsonDocument d;
    faceToJson(c, d.to<JsonObject>());
    String out;
    serializeJson(d, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/blink", HTTP_POST, [](AsyncWebServerRequest *req) {
    face.blinkNow();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // "Welcher Roboter ist das?" fuers Team - kurzer Blitz-Effekt statt des
  // Gesichts, siehe Face::identify(). Wirkt nicht waehrend des Pairing-
  // Screens (dort ruft loop() Face::update() gar nicht auf).
  server.on("/api/identify", HTTP_POST, [](AsyncWebServerRequest *req) {
    face.identify();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // Roboter zurueck auf den Pairing-Screen setzen (WLAN-QR, dann Seiten-QR),
  // z.B. wenn die naechste Gruppe den Roboter bekommt - dieselbe Anzeige wie
  // beim allerersten Start.
  server.on("/api/pair", HTTP_POST, [](AsyncWebServerRequest *req) {
    // Only flag it: drawing belongs to loop(), not to the network task.
    pairRequested = true;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/face", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument d;
    faceToJson(face.config(), d.to<JsonObject>());
    String out;
    serializeJson(d, out);
    req->send(200, "application/json", out);
  });

  // PUT /api/face - partial update, only the given fields change
  auto *faceHandler = new AsyncCallbackJsonWebHandler(
      "/api/face", [](AsyncWebServerRequest *req, JsonVariant &json) {
        if (!json.is<JsonObject>()) {
          req->send(400, "application/json", "{\"error\":\"expected a JSON object\"}");
          return;
        }
        FaceConfig c = face.config();
        String err;
        if (!faceFromJson(json.as<JsonObjectConst>(), c, err)) {
          JsonDocument d;
          d["error"] = err;
          String out;
          serializeJson(d, out);
          req->send(400, "application/json", out);
          return;
        }
        face.setConfig(c);
        faceSave(c);
        JsonDocument d;
        faceToJson(c, d.to<JsonObject>());
        String out;
        serializeJson(d, out);
        req->send(200, "application/json", out);
      });
  faceHandler->setMethod(HTTP_PUT | HTTP_POST);
  server.addHandler(faceHandler);

  // POST /api/drive - {"x":-1..1,"y":-1..1}
  auto *driveHandler = new AsyncCallbackJsonWebHandler(
      "/api/drive", [](AsyncWebServerRequest *req, JsonVariant &json) {
        JsonObjectConst o = json.as<JsonObjectConst>();
        if (o.isNull()) {
          req->send(400, "application/json", "{\"error\":\"expected a JSON object\"}");
          return;
        }
        float x = o["x"].is<float>() ? o["x"].as<float>() : 0.0f;
        float y = o["y"].is<float>() ? o["y"].as<float>() : 0.0f;
        portENTER_CRITICAL(&cmdMux);
        cmdTurn = constrain(x, -1.0f, 1.0f);
        cmdFwd = constrain(y, -1.0f, 1.0f);
        lastCmdMs = millis();
        portEXIT_CRITICAL(&cmdMux);
        noteDrive(x, y);
        req->send(200, "application/json", "{\"ok\":true}");
      });
  driveHandler->setMethod(HTTP_POST);
  server.addHandler(driveHandler);

  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *req) {
    portENTER_CRITICAL(&cmdMux);
    cmdTurn = 0;
    cmdFwd = 0;
    lastCmdMs = millis();
    portEXIT_CRITICAL(&cmdMux);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // PUT /api/settings - {"speed":10..100,"trimLeft":-120..120,"trimRight":...}
  auto *setHandler = new AsyncCallbackJsonWebHandler(
      "/api/settings", [](AsyncWebServerRequest *req, JsonVariant &json) {
        JsonObjectConst o = json.as<JsonObjectConst>();
        if (o.isNull()) {
          req->send(400, "application/json", "{\"error\":\"expected a JSON object\"}");
          return;
        }
        if (o["speed"].is<int>())
          cmdScale = constrain(o["speed"].as<int>(), 10, 100) / 100.0f;
        if (o["trimLeft"].is<int>()) {
          trimLeft = constrain(o["trimLeft"].as<int>(), -120, 120);
          prefs.putInt("trimL", trimLeft);
        }
        if (o["trimRight"].is<int>()) {
          trimRight = constrain(o["trimRight"].as<int>(), -120, 120);
          prefs.putInt("trimR", trimRight);
        }
        JsonDocument d;
        d["speed"] = (int)lroundf(cmdScale * 100);
        d["trimLeft"] = trimLeft;
        d["trimRight"] = trimRight;
        String out;
        serializeJson(d, out);
        req->send(200, "application/json", out);
      });
  setHandler->setMethod(HTTP_PUT | HTTP_POST);
  server.addHandler(setHandler);

  server.on("/openapi.json", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", OPENAPI_JSON);
  });
  // Nur der Gesichtseditor, ohne Reiterleiste - gleiche Seite, anderer Pfad.
  server.on("/gesicht", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/docs", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html; charset=utf-8", API_DOCS_HTML);
  });

  server.onNotFound([](AsyncWebServerRequest *req) { req->redirect("/"); });
  server.begin();

  drawPairStep();
  pairDrawn = true;

  lastCmdMs = millis();
}

void loop() {
  ws.cleanupClients();

  float turn, fwd;
  portENTER_CRITICAL(&cmdMux);
  bool stale = millis() - lastCmdMs > DRIVE_TIMEOUT_MS;
  turn = stale ? 0.0f : cmdTurn;
  fwd = stale ? 0.0f : cmdFwd;
  portEXIT_CRITICAL(&cmdMux);

  driveApply(turn, fwd, cmdScale);

  if (pairRequested) {
    pairRequested = false;
    driveSeen = false;
    pairStep = PAIR_JOIN_WIFI;
    pairDrawn = false;
  }

  if (pairStep != PAIR_DONE) {
    if (driveSeen) {
      pairStep = PAIR_DONE;  // the face redraws itself on the next update()
    } else {
      // Step 2 as soon as a phone has joined, back to step 1 if it leaves.
      static uint32_t lastPoll = 0;
      if (millis() - lastPoll > 250) {
        lastPoll = millis();
        PairStep want = WiFi.softAPgetStationNum() > 0 ? PAIR_OPEN_PAGE : PAIR_JOIN_WIFI;
        if (want != pairStep) {
          pairStep = want;
          pairDrawn = false;
        }
      }
      if (!pairDrawn) {
        drawPairStep();
        pairDrawn = true;
      }
    }
  }

  if (pairStep == PAIR_DONE) {
    face.setDrive(turn, fwd);
    face.update();
  }

  delay(2);
}
