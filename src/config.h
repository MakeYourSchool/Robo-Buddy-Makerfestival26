#pragma once

// ---------------------------------------------------------------- hardware --
// Continuous rotation servos (tank drive, dragwheel at the back).
// Verified on the bench: the chip produces a clean 50 Hz signal on both of
// these, with the correct duty for every pulse width.
#define PIN_SERVO_LEFT   19
#define PIN_SERVO_RIGHT  18

// A continuous servo spins one way for pulses > neutral and the other way for
// pulses < neutral. The two wheels face opposite directions, so one of them
// has to be mirrored.
#define SERVO_LEFT_INVERT   false
#define SERVO_RIGHT_INVERT  true

#define SERVO_NEUTRAL_US 1500  // "stop" pulse, per-servo trim is added on top
#define SERVO_SPAN_US     450  // deviation from neutral at full throttle

// GC9A01 1.28" round TFT, FPC connector pointing down.
//
// GPIO4-7 have nothing attached on the Super Mini. GPIO4/5 are strapping pins,
// but they only select SDIO slave timing and do not affect a normal boot;
// GPIO6/7 are the pad-JTAG pins, unused while JTAG runs over USB.
#define PIN_TFT_SCK   4
#define PIN_TFT_MOSI  5
#define PIN_TFT_DC    6
#define PIN_TFT_CS    7
#define PIN_TFT_RST   3  // drive reset from a GPIO, do NOT tie it to 5V

#define TFT_ROTATION 0  // 0/1/2/3 - bump by 2 if the face shows up upside down
#define TFT_SPI_HZ   40000000

// ------------------------------------------------------------------ network --
// Standalone: the robot always opens its own WiFi and never joins another one,
// so there are no home-network credentials anywhere in this project.
//
// The network is named AP_SSID_PREFIX plus the robot's five-character code,
// e.g. "MF26-0HMFS", so several robots in one room each get their own.
#define AP_SSID_PREFIX "MF26-"

// The password is not a secret worth keeping: it is shown as a QR code on the
// robot's own display to anyone standing in front of it. It only stops other
// people's phones joining by accident. WPA2 needs at least 8 characters.
#define AP_PASS "mf26-robot"

// ----------------------------------------------------------------- behaviour --
#define DRIVE_TIMEOUT_MS 700   // no command for this long -> stop
#define SERVO_IDLE_MS   2000   // stop pulsing after standing still this long
