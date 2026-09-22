#pragma once
#include <Arduino_GFX_Library.h>

// ---------------------------------------------------------------------------
// Everything about how the face looks. Persisted to NVS as one blob, and
// editable over the API, so bump CONFIG_VERSION if the layout ever changes.
// ---------------------------------------------------------------------------

#define FACE_CONFIG_VERSION 2

enum EyeStyle : uint8_t {
  EYE_ROUNDED = 0,  // Cozmo-ish rounded block
  EYE_CIRCLE,
  EYE_SQUARE,
  EYE_OVAL,         // tall ellipse
  EYE_HAPPY,        // upward arc, ^ ^
  EYE_SLEEPY,       // bottom sliver only
  EYE_STYLE_COUNT
};

enum MouthStyle : uint8_t {
  MOUTH_SMILE = 0,
  MOUTH_FLAT,
  MOUTH_OPEN,       // filled ellipse
  MOUTH_CAT,        // w shape
  MOUTH_FROWN,
  MOUTH_GRIN,       // filled block with a tooth line
  MOUTH_SQUIGGLE,
  MOUTH_STYLE_COUNT
};

enum BrowStyle : uint8_t {
  BROW_NONE = 0,
  BROW_FLAT,
  BROW_ANGRY,       // inner ends low
  BROW_SAD,         // inner ends high
  BROW_RAISED,      // curved
  BROW_STYLE_COUNT
};

enum NoseStyle : uint8_t {
  NOSE_NONE = 0,
  NOSE_DOT,
  NOSE_TRIANGLE,
  NOSE_LINE,
  NOSE_STYLE_COUNT
};

struct FaceConfig {
  uint8_t  version;
  uint8_t  eye;
  uint8_t  mouth;
  uint8_t  brow;
  uint8_t  nose;
  uint16_t color;      // RGB565, the features
  uint16_t bg;         // RGB565, the background
  uint8_t  eyeW;       // 16..90
  uint8_t  eyeH;       // 16..110
  uint8_t  eyeGap;     // 20..75, centre to each eye
  uint8_t  eyeY;       // 50..150
  uint8_t  mouthW;     // 16..110
  uint8_t  mouthY;     // 130..215
  uint8_t  thickness;  // 2..10, stroke weight for drawn features
  uint8_t  blinkEvery; // seconds between blinks, 1..20
  uint8_t  lookEvery;  // seconds between glances, 1..20
  uint8_t  autoBlink;  // 0/1
  uint8_t  autoLook;   // 0/1
  uint8_t  tears;      // 0/1, animated tears squirting from both eyes
};

FaceConfig faceConfigDefault();
void faceConfigClamp(FaceConfig &c);

// Human-readable style names, indexed by the enums above.
extern const char *const EYE_NAMES[EYE_STYLE_COUNT];
extern const char *const MOUTH_NAMES[MOUTH_STYLE_COUNT];
extern const char *const BROW_NAMES[BROW_STYLE_COUNT];
extern const char *const NOSE_NAMES[NOSE_STYLE_COUNT];

// ---------------------------------------------------------------------------

class Face {
 public:
  bool begin(Arduino_GFX *output);

  void setConfig(const FaceConfig &c);
  FaceConfig config() const { return _cfg; }

  // Current drive command (-1..1). The face looks where the robot is going.
  void setDrive(float turn, float forward);

  // Blink right now, whatever the schedule says.
  void blinkNow();

  // "Welcher Roboter ist das?" - kurzer Blitz-Effekt (weiss/Hintergrund im
  // Wechsel), damit man ihn am Tisch findet. Nur wirksam, waehrend das
  // normale Gesicht laeuft (update() wird auf dem Pairing-Screen nicht
  // aufgerufen, siehe main.cpp loop()).
  void identify();

  void update();
  void showMessage(const char *line1, const char *line2);

  // Pairing screen: `title` on top, a QR code for `qrText`, and two short
  // lines underneath. Each line fits about 12 characters on the round panel.
  void showPairing(const char *title, const char *qrText,
                   const char *line1, const char *line2);

 private:
  void render();
  void pickNewGazeTarget(uint32_t now);
  void drawEye(int16_t cx, int16_t cy, float openness);
  void drawBrow(int16_t cx, int16_t cy, bool rightSide);
  void drawMouth();
  void drawNose();
  void drawTears();
  void strokePath(const int16_t *xs, const int16_t *ys, int n);

  Arduino_GFX *_out = nullptr;
  Arduino_Canvas *_c = nullptr;
  Arduino_GFX *_g = nullptr;  // canvas when buffered, panel otherwise

  FaceConfig _cfg;

  uint32_t _lastFrame = 0;

  float _gazeX = 0, _gazeY = 0;
  float _gazeFromX = 0, _gazeFromY = 0;
  float _gazeToX = 0, _gazeToY = 0;
  uint32_t _gazeStart = 0, _gazeEnd = 0, _gazeHold = 0;

  float _openness = 1.0f;
  uint32_t _blinkStart = 0, _nextBlink = 0;
  uint8_t _blinksLeft = 0;

  float _driveTurn = 0, _driveFwd = 0;
  float _smile = 0.35f;
  float _squint = 0;

  bool _dirty = true;
  int32_t _lastKey = INT32_MIN;

  uint32_t _identifyUntil = 0;
};
