#include "face.h"
#include <math.h>

// Nayuki's qrcodegen ships inside the ESP-IDF qrcode component, but only its
// esp_qrcode_generate() wrapper has a public header - and that wrapper always
// passes boostEcl=true, silently raising the error correction. Declare the
// underlying encoder directly so the code stays at the lowest level, L.
extern "C" {
bool qrcodegen_encodeText(const char *text, uint8_t tempBuffer[], uint8_t qrcode[],
                          int ecl, int minVersion, int maxVersion, int mask,
                          bool boostEcl);
int qrcodegen_getSize(const uint8_t qrcode[]);
bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);
}

namespace {

constexpr int16_t SCREEN = 240;
constexpr int16_t CX = SCREEN / 2;

constexpr float GAZE_RANGE_X = 20.0f;
constexpr float GAZE_RANGE_Y = 16.0f;

constexpr uint32_t BLINK_MS = 150;
constexpr uint32_t FRAME_MS = 30;

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

uint8_t clampu(int v, int lo, int hi) {
  return (uint8_t)(v < lo ? lo : (v > hi ? hi : v));
}

float easeInOut(float t) {
  t = clampf(t, 0.0f, 1.0f);
  return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float approach(float cur, float target, float rate) {
  return cur + (target - cur) * rate;
}

}  // namespace

const char *const EYE_NAMES[EYE_STYLE_COUNT] = {
    "rounded", "circle", "square", "oval", "happy", "sleepy"};
const char *const MOUTH_NAMES[MOUTH_STYLE_COUNT] = {
    "smile", "flat", "open", "cat", "frown", "grin", "squiggle"};
const char *const BROW_NAMES[BROW_STYLE_COUNT] = {
    "none", "flat", "angry", "sad", "raised"};
const char *const NOSE_NAMES[NOSE_STYLE_COUNT] = {
    "none", "dot", "triangle", "line"};

FaceConfig faceConfigDefault() {
  FaceConfig c{};
  c.version = FACE_CONFIG_VERSION;
  c.eye = EYE_ROUNDED;
  c.mouth = MOUTH_SMILE;
  c.brow = BROW_NONE;
  c.nose = NOSE_NONE;
  c.color = 0x5D7F;  // the pale cyan the robot started life with
  c.bg = 0x0000;
  c.eyeW = 62;
  c.eyeH = 80;
  c.eyeGap = 44;
  c.eyeY = 100;
  c.mouthW = 68;
  c.mouthY = 176;
  c.thickness = 4;
  c.blinkEvery = 4;
  c.lookEvery = 3;
  c.autoBlink = 1;
  c.autoLook = 1;
  c.tears = 0;
  return c;
}

void faceConfigClamp(FaceConfig &c) {
  c.version = FACE_CONFIG_VERSION;
  if (c.eye >= EYE_STYLE_COUNT) c.eye = EYE_ROUNDED;
  if (c.mouth >= MOUTH_STYLE_COUNT) c.mouth = MOUTH_SMILE;
  if (c.brow >= BROW_STYLE_COUNT) c.brow = BROW_NONE;
  if (c.nose >= NOSE_STYLE_COUNT) c.nose = NOSE_NONE;
  c.eyeW = clampu(c.eyeW, 16, 90);
  c.eyeH = clampu(c.eyeH, 16, 110);
  c.eyeGap = clampu(c.eyeGap, 20, 75);
  c.eyeY = clampu(c.eyeY, 50, 150);
  c.mouthW = clampu(c.mouthW, 16, 110);
  c.mouthY = clampu(c.mouthY, 130, 215);
  c.thickness = clampu(c.thickness, 2, 10);
  c.blinkEvery = clampu(c.blinkEvery, 1, 20);
  c.lookEvery = clampu(c.lookEvery, 1, 20);
  c.autoBlink = c.autoBlink ? 1 : 0;
  c.autoLook = c.autoLook ? 1 : 0;
  c.tears = c.tears ? 1 : 0;
}

// ---------------------------------------------------------------------------

bool Face::begin(Arduino_GFX *output) {
  _out = output;
  _cfg = faceConfigDefault();

  _c = new Arduino_Canvas(SCREEN, SCREEN, output);
  if (!_c || !_c->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    // Not enough RAM for a frame buffer - draw straight to the panel instead.
    delete _c;
    _c = nullptr;
  }
  _g = _c ? (Arduino_GFX *)_c : _out;
  _out->fillScreen(_cfg.bg);

  uint32_t now = millis();
  _nextBlink = now + random(1200, 3500);
  _gazeHold = now + 1200;
  return _c != nullptr;
}

void Face::setConfig(const FaceConfig &c) {
  _cfg = c;
  faceConfigClamp(_cfg);
  _dirty = true;
}

void Face::blinkNow() {
  if (_blinksLeft == 0) {
    _blinksLeft = 1;
    _blinkStart = millis();
  }
}

void Face::identify() {
  _identifyUntil = millis() + 3000;
  _dirty = true;
}

void Face::showMessage(const char *line1, const char *line2) {
  _dirty = true;
  _out->fillScreen(_cfg.bg);
  _out->setTextColor(_cfg.color);
  _out->setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  _out->getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  _out->setCursor((SCREEN - w) / 2, 100);
  _out->print(line1);
  if (line2 && *line2) {
    _out->getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    _out->setCursor((SCREEN - w) / 2, 130);
    _out->print(line2);
  }
}

// Centre one line of built-in-font text on the panel.
static void centredText(Arduino_GFX *g, const char *text, int16_t y, uint8_t size,
                        uint16_t colour) {
  int16_t x1, y1;
  uint16_t w, h;
  g->setTextColor(colour);
  g->setTextSize(size);
  g->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  g->setCursor((SCREEN - w) / 2, y);
  g->print(text);
}

void Face::showPairing(const char *title, const char *qrText,
                       const char *line1, const char *line2) {
  _dirty = true;
  _out->fillScreen(_cfg.bg);

  // The round panel is narrow at the top and bottom, so text sits where it is
  // widest and the QR code takes the middle.
  centredText(_out, title, 22, 2, _cfg.color);

  // Smallest version that fits, error correction L (the minimum a QR code
  // can have), no boosting. Version 5 covers a WiFi login comfortably.
  constexpr int MAX_VERSION = 5;
  constexpr int BUF = ((MAX_VERSION * 4 + 17) * (MAX_VERSION * 4 + 17) + 7) / 8 + 1;
  static uint8_t qr[BUF], tmp[BUF];
  constexpr int ECC_LOW = 0, MASK_AUTO = -1;

  if (qrcodegen_encodeText(qrText, tmp, qr, ECC_LOW, 1, MAX_VERSION, MASK_AUTO, false)) {
    const int size = qrcodegen_getSize(qr);
    const int quiet = 2;    // light border scanners need to find it
    const int maxBox = 132; // fits between the title and the two lines below
    const int top = 40;
    const int scale = max(1, maxBox / (size + 2 * quiet));
    const int box = (size + 2 * quiet) * scale;
    const int x0 = (SCREEN - box) / 2;
    const int y0 = top + (maxBox - box) / 2;

    // dark modules on white: an inverted code on the black background would
    // not scan on every phone
    _out->fillRect(x0, y0, box, box, RGB565_WHITE);
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        if (qrcodegen_getModule(qr, x, y)) {
          _out->fillRect(x0 + (x + quiet) * scale, y0 + (y + quiet) * scale,
                         scale, scale, RGB565_BLACK);
        }
      }
    }
  }

  centredText(_out, line1, 178, 2, _cfg.color);
  centredText(_out, line2, 200, 2, RGB565_WHITE);
}

void Face::setDrive(float turn, float forward) {
  _driveTurn = clampf(turn, -1.0f, 1.0f);
  _driveFwd = clampf(forward, -1.0f, 1.0f);
}

void Face::pickNewGazeTarget(uint32_t now) {
  _gazeFromX = _gazeX;
  _gazeFromY = _gazeY;
  if (random(100) < 30) {
    _gazeToX = (random(2) ? 1.0f : -1.0f) * (0.6f + random(40) / 100.0f);
    _gazeToY = (random(60) - 30) / 100.0f;
  } else {
    _gazeToX = (random(120) - 60) / 100.0f;
    _gazeToY = (random(100) - 45) / 100.0f;
  }
  _gazeStart = now;
  _gazeEnd = now + random(180, 380);

  uint32_t base = (uint32_t)_cfg.lookEvery * 1000;
  _gazeHold = _gazeEnd + base / 2 + random(base);
}

void Face::update() {
  uint32_t now = millis();
  if (now - _lastFrame < FRAME_MS) return;
  _lastFrame = now;

  bool driving = fabsf(_driveTurn) > 0.12f || fabsf(_driveFwd) > 0.12f;
  if (driving) {
    // The display faces the viewer, so the robot's left is the screen's right:
    // mirror the steering to look where it is actually turning.
    _gazeX = approach(_gazeX, clampf(-_driveTurn * 1.1f, -1.0f, 1.0f), 0.18f);
    _gazeY = approach(_gazeY, clampf(-_driveFwd * 0.55f, -1.0f, 1.0f), 0.18f);
    _gazeHold = now + 800;
    _gazeFromX = _gazeToX = _gazeX;
    _gazeFromY = _gazeToY = _gazeY;
  } else if (_cfg.autoLook) {
    if (now >= _gazeHold) {
      pickNewGazeTarget(now);
    } else if (now < _gazeEnd) {
      float t = easeInOut((float)(now - _gazeStart) / (float)(_gazeEnd - _gazeStart));
      _gazeX = _gazeFromX + (_gazeToX - _gazeFromX) * t;
      _gazeY = _gazeFromY + (_gazeToY - _gazeFromY) * t;
    } else {
      _gazeX = _gazeToX;
      _gazeY = _gazeToY;
    }
  } else {
    _gazeX = approach(_gazeX, 0.0f, 0.15f);
    _gazeY = approach(_gazeY, 0.0f, 0.15f);
  }

  if (_cfg.autoBlink && _blinksLeft == 0 && now >= _nextBlink) {
    _blinksLeft = (random(100) < 25) ? 2 : 1;
    _blinkStart = now;
  }
  if (_blinksLeft > 0) {
    uint32_t dt = now - _blinkStart;
    if (dt >= BLINK_MS) {
      if (--_blinksLeft > 0) {
        _blinkStart = now;
        _openness = 1.0f;
      } else {
        _openness = 1.0f;
        uint32_t base = (uint32_t)_cfg.blinkEvery * 1000;
        _nextBlink = now + base / 2 + random(base);
      }
    } else {
      float p = (float)dt / (float)BLINK_MS;
      _openness = fabsf(p - 0.5f) * 2.0f;
      _openness = 0.06f + 0.94f * easeInOut(_openness);
    }
  }

  float speed = clampf(fabsf(_driveFwd) + fabsf(_driveTurn) * 0.6f, 0.0f, 1.0f);
  _smile = approach(_smile, 0.3f + 0.7f * speed, 0.08f);
  _squint = approach(_squint, 0.18f * speed, 0.08f);

  render();
}

// ---------------------------------------------------------------- drawing --

// A thick polyline. Walks along every segment rather than only stamping the
// sample points, or a two-point path (a flat brow, a straight mouth) would come
// out as two dots joined by a hairline.
void Face::strokePath(const int16_t *xs, const int16_t *ys, int n) {
  const int16_t r = max<int16_t>(1, _cfg.thickness / 2);
  if (n == 1) {
    _g->fillCircle(xs[0], ys[0], r, _cfg.color);
    return;
  }
  for (int i = 0; i + 1 < n; i++) {
    const int dx = xs[i + 1] - xs[i];
    const int dy = ys[i + 1] - ys[i];
    int steps = max(abs(dx), abs(dy));
    if (steps < 1) steps = 1;
    for (int s = 0; s <= steps; s++) {
      _g->fillCircle(xs[i] + dx * s / steps, ys[i] + dy * s / steps, r, _cfg.color);
    }
  }
}

void Face::drawEye(int16_t cx, int16_t cy, float openness) {
  const int16_t w = _cfg.eyeW;
  int16_t h = (int16_t)(_cfg.eyeH * clampf(openness, 0.0f, 1.0f));
  if (h < 6) h = 6;

  switch (_cfg.eye) {
    case EYE_CIRCLE: {
      int16_t rx = w / 2, ry = h / 2;
      _g->fillEllipse(cx, cy, rx, min(rx, ry), _cfg.color);
      break;
    }
    case EYE_SQUARE:
      _g->fillRect(cx - w / 2, cy - h / 2, w, h, _cfg.color);
      break;
    case EYE_OVAL:
      _g->fillEllipse(cx, cy, w / 2, h / 2, _cfg.color);
      break;
    case EYE_HAPPY: {
      // an upward arc - a closed, cheerful eye that still blinks
      const int steps = 14;
      int16_t xs[steps + 1], ys[steps + 1];
      int16_t span = w / 2;
      int16_t rise = (int16_t)(h * 0.45f);
      for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        xs[i] = cx - span + (int16_t)(t * 2 * span);
        ys[i] = cy + (int16_t)(rise * (4.0f * (t - 0.5f) * (t - 0.5f) - 0.5f));
      }
      strokePath(xs, ys, steps + 1);
      break;
    }
    case EYE_SLEEPY: {
      int16_t sh = max<int16_t>(6, h / 3);
      _g->fillRoundRect(cx - w / 2, cy + h / 2 - sh, w, sh,
                        min<int16_t>(sh / 2, w / 2), _cfg.color);
      break;
    }
    case EYE_ROUNDED:
    default: {
      int16_t r = min<int16_t>(min(w, h) / 3, h / 2);
      _g->fillRoundRect(cx - w / 2, cy - h / 2, w, h, r, _cfg.color);
      break;
    }
  }
}

void Face::drawBrow(int16_t cx, int16_t cy, bool rightSide) {
  if (_cfg.brow == BROW_NONE) return;

  const int16_t half = _cfg.eyeW / 2;
  const int16_t gap = _cfg.eyeH / 2 + 12;
  const int16_t inner = rightSide ? cx - half : cx + half;  // toward the nose
  const int16_t outer = rightSide ? cx + half : cx - half;
  const int16_t tilt = 9;

  int16_t xs[9], ys[9];
  int n = 2;

  switch (_cfg.brow) {
    case BROW_ANGRY:
      xs[0] = inner; ys[0] = cy - gap + tilt;
      xs[1] = outer; ys[1] = cy - gap - tilt;
      break;
    case BROW_SAD:
      xs[0] = inner; ys[0] = cy - gap - tilt;
      xs[1] = outer; ys[1] = cy - gap + tilt;
      break;
    case BROW_RAISED: {
      n = 9;
      for (int i = 0; i < n; i++) {
        float t = (float)i / (n - 1);
        xs[i] = cx - half + (int16_t)(t * 2 * half);
        ys[i] = cy - gap - 4 - (int16_t)(8 * sinf(t * (float)M_PI));
      }
      break;
    }
    case BROW_FLAT:
    default:
      xs[0] = cx - half; ys[0] = cy - gap;
      xs[1] = cx + half; ys[1] = cy - gap;
      break;
  }
  strokePath(xs, ys, n);
}

void Face::drawNose() {
  if (_cfg.nose == NOSE_NONE) return;

  const int16_t nx = CX + (int16_t)(_gazeX * 6.0f);
  const int16_t ny = (_cfg.eyeY + _cfg.mouthY) / 2 + (int16_t)(_gazeY * 4.0f);
  const int16_t s = 6 + _cfg.thickness;

  switch (_cfg.nose) {
    case NOSE_TRIANGLE:
      _g->fillTriangle(nx, ny + s / 2, nx - s / 2, ny - s / 2,
                       nx + s / 2, ny - s / 2, _cfg.color);
      break;
    case NOSE_LINE: {
      int16_t xs[2] = {nx, nx};
      int16_t ys[2] = {(int16_t)(ny - s / 2), (int16_t)(ny + s / 2)};
      strokePath(xs, ys, 2);
      break;
    }
    case NOSE_DOT:
    default:
      _g->fillCircle(nx, ny, s / 2, _cfg.color);
      break;
  }
}

// Purely a function of millis() - no particle state to carry between
// frames, no allocation, resets itself for free every cycle.
void Face::drawTears() {
  if (!_cfg.tears) return;

  constexpr float CYCLE_MS = 850.0f;
  constexpr int DROPS_PER_EYE = 2;
  const uint32_t now = millis();

  int16_t ox = (int16_t)(_gazeX * GAZE_RANGE_X);
  int16_t oy = (int16_t)(_gazeY * GAZE_RANGE_Y);
  int16_t ly = _cfg.eyeY + oy;

  for (int side = 0; side < 2; side++) {
    int16_t ex = CX + (side ? 1 : -1) * _cfg.eyeGap + ox;
    int16_t ey = ly + _cfg.eyeH / 2;
    for (int d = 0; d < DROPS_PER_EYE; d++) {
      // Staggered start per drop and per eye so the two sides don't spritz
      // in lockstep - reads as a real, slightly chaotic sob instead of a
      // mechanical loop.
      float offset = side * 130.0f + d * (CYCLE_MS / DROPS_PER_EYE);
      float t = fmodf((float)now + offset, CYCLE_MS) / CYCLE_MS;

      // First third: a sideways burst away from the face. Rest: gravity
      // takes over and the drop accelerates down past the cheek.
      float outward = (side ? 1.0f : -1.0f) * (14.0f + 30.0f * fminf(t * 3.0f, 1.0f));
      float fall = -6.0f + 130.0f * t * t;
      int16_t dx = ex + (int16_t)outward;
      int16_t dy = ey + (int16_t)fall;
      if (dy > SCREEN + 8) continue;  // past the panel edge this cycle

      int16_t r = max<int16_t>(2, (int16_t)(4.0f * (1.0f - t * 0.4f)));
      _g->fillCircle(dx, dy, r, _cfg.color);
    }
  }
}

void Face::drawMouth() {
  const int16_t half = _cfg.mouthW / 2;
  const int16_t mx = CX + (int16_t)(_gazeX * 5.0f);
  const int16_t my = _cfg.mouthY + (int16_t)(_gazeY * 3.0f);
  const int steps = 24;
  int16_t xs[steps + 1], ys[steps + 1];

  switch (_cfg.mouth) {
    case MOUTH_FLAT: {
      int16_t lx[2] = {(int16_t)(mx - half), (int16_t)(mx + half)};
      int16_t ly[2] = {my, my};
      strokePath(lx, ly, 2);
      break;
    }
    case MOUTH_OPEN:
      _g->fillEllipse(mx, my, half, (int16_t)(half * (0.5f + 0.5f * _smile)),
                      _cfg.color);
      break;
    case MOUTH_CAT: {
      // two little arcs meeting in the middle
      for (int side = 0; side < 2; side++) {
        int16_t x0 = side ? mx : mx - half;
        for (int i = 0; i <= steps; i++) {
          float t = (float)i / steps;
          xs[i] = x0 + (int16_t)(t * half);
          ys[i] = my - (int16_t)(half * 0.35f * sinf(t * (float)M_PI));
        }
        strokePath(xs, ys, steps + 1);
      }
      break;
    }
    case MOUTH_FROWN: {
      float depth = 8.0f + 12.0f * (1.0f - _smile);
      for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        xs[i] = mx - half + (int16_t)(t * 2 * half);
        ys[i] = my - (int16_t)(depth * (1.0f - 4.0f * (t - 0.5f) * (t - 0.5f)));
      }
      strokePath(xs, ys, steps + 1);
      break;
    }
    case MOUTH_GRIN: {
      int16_t h = (int16_t)(10 + 16 * _smile);
      _g->fillRoundRect(mx - half, my - h / 2, half * 2, h, h / 2, _cfg.color);
      _g->drawFastHLine(mx - half + 3, my, half * 2 - 6, _cfg.bg);
      break;
    }
    case MOUTH_SQUIGGLE: {
      for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        xs[i] = mx - half + (int16_t)(t * 2 * half);
        ys[i] = my + (int16_t)(7.0f * sinf(t * 3.0f * (float)M_PI));
      }
      strokePath(xs, ys, steps + 1);
      break;
    }
    case MOUTH_SMILE:
    default: {
      float depth = 8.0f + 16.0f * _smile;
      for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        xs[i] = mx - half + (int16_t)(t * 2 * half);
        ys[i] = my + (int16_t)(depth * (1.0f - 4.0f * (t - 0.5f) * (t - 0.5f)));
      }
      strokePath(xs, ys, steps + 1);
      break;
    }
  }
}

void Face::render() {
  // "Welcher Roboter ist das?" - ein reiner Blitz-Effekt statt des Gesichts,
  // solange identify() das noch verlangt. Kein Blick auf _cfg noetig, darum
  // vor allem anderen behandelt und fruehzeitig zurueckgekehrt.
  if (millis() < _identifyUntil) {
    _dirty = true;
    bool on = (millis() / 150) % 2 == 0;
    _g->fillScreen(on ? RGB565_WHITE : _cfg.bg);
    if (_c) _c->flush();
    return;
  }

  int16_t ox = (int16_t)(_gazeX * GAZE_RANGE_X);
  int16_t oy = (int16_t)(_gazeY * GAZE_RANGE_Y);
  float open = clampf(_openness - _squint, 0.0f, 1.0f);

  // Pushing a 240x240 frame costs real time on a single-core C6, so only
  // redraw when something actually moved - the webserver needs the cycles.
  int32_t key = (int32_t)(ox + 64) | ((int32_t)(oy + 64) << 8) |
                ((int32_t)(open * 100) << 16) | ((int32_t)(_smile * 60) << 24);
  // Tears move on every frame from millis() alone, not from anything the key
  // above tracks, so the key would otherwise think nothing changed and skip
  // the redraw entirely while crying.
  if (_cfg.tears) _dirty = true;
  if (!_dirty && key == _lastKey) return;
  _lastKey = key;
  _dirty = false;

  _g->fillScreen(_cfg.bg);

  int16_t ly = _cfg.eyeY + oy;
  drawEye(CX - _cfg.eyeGap + ox, ly, open);
  drawEye(CX + _cfg.eyeGap + ox, ly, open);
  drawBrow(CX - _cfg.eyeGap + ox, ly, false);
  drawBrow(CX + _cfg.eyeGap + ox, ly, true);
  drawNose();
  drawMouth();
  drawTears();

  if (_c) _c->flush();
}
