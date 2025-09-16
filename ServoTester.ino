#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <Bounce2.h>
#include <ESP32Servo.h>

// ---------- Pins ----------
#define PIN_OLED_SDA 8
#define PIN_OLED_SCL 9
#define PIN_ENC_A    4
#define PIN_ENC_B    5
#define PIN_ENC_SW   6
#define PIN_SERVO   10

// ---------- Display ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- Encoder ----------
RotaryEncoder encoder(PIN_ENC_A, PIN_ENC_B, RotaryEncoder::LatchMode::FOUR3);

// ---------- Button ----------
Bounce2::Button btn;

// ---------- Servo ----------
Servo servo;

// ---------- Values / easing ----------
const int VALUE_MIN = 0;
const int VALUE_MAX = 180;
int value = 90;              // target angle shown/rounded
int currentAngle = value;    // actual servo angle (eased)

// Easing params
const int EASE_STEP = 2;     // degrees per easing tick
const uint16_t EASE_MS = 8;  // ms between easing ticks
uint32_t lastEase = 0;

// ---------- Increment handling ----------
int increment = 1;           // cycles 1 -> 5 -> 10 -> 1
bool showingIncrement = false;

inline int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

inline int roundToIncrement(int v, int inc) {
  // Round to nearest multiple of inc
  int r = (v + inc / 2) / inc;   // integer rounding
  return r * inc;
}

void showValue() {
  showingIncrement = false;
  display.clearDisplay();
  display.setTextSize(3);              // big number for 128x32
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(value);
  display.display();
}

void showIncrement() {
  showingIncrement = true;
  display.clearDisplay();
  display.setTextSize(2);              // readable text on 128x32
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Increment:");
  display.setCursor(0, 16);
  display.print(increment);
  display.display();
}

void setup() {
  // I2C + OLED
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;) delay(1000);
  }
  // Flip display 180°
  display.setRotation(2);

  // Encoder pins
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);

  // Button
  btn.attach(PIN_ENC_SW, INPUT_PULLUP);
  btn.interval(15);
  btn.setPressedState(LOW);

  // Servo
  servo.attach(PIN_SERVO, 500, 2500);
  currentAngle = value;
  servo.write(value);

  encoder.setPosition(value);
  showValue();
}

void loop() {
  // ----- Encoder movement -----
  encoder.tick();
  static int lastPos = encoder.getPosition();
  int newPos = encoder.getPosition();

  if (newPos != lastPos) {
    // If we were showing the increment screen, switch back to value now
    if (showingIncrement) {
      showingIncrement = false;
      // (fall through to process movement)
    }

    int detents = newPos - lastPos;
    long nv = (long)value + (long)detents * increment;
    nv = clampInt(roundToIncrement((int)nv, increment), VALUE_MIN, VALUE_MAX);

    if ((int)nv != value) {
      value = (int)nv;
      showValue();                  // show angle after any turn
      // easing loop below will move the servo smoothly
    }
    lastPos = newPos;
  }

  // ----- Button: cycle increment 1 -> 5 -> 10 -> 1 -----
  btn.update();
  if (btn.released()) {
    if (increment == 1)      increment = 5;
    else if (increment == 5) increment = 10;
    else                     increment = 1;

    // Snap current value to the new increment (always rounded)
    value = clampInt(roundToIncrement(value, increment), VALUE_MIN, VALUE_MAX);

    // Keep encoder in sync AND prevent immediate flicker back to value:
    encoder.setPosition(value);
    // Also sync our delta reference so the next loop doesn't see a false movement.
    // Re-read current position after setPosition:
    lastPos = encoder.getPosition();

    showIncrement();                // persist until next actual rotation
  }

  // ----- Easing: glide servo toward target value -----
  if (millis() - lastEase >= EASE_MS) {
    lastEase = millis();
    if (currentAngle != value) {
      int dir = (value > currentAngle) ? +1 : -1;
      int next = currentAngle + dir * EASE_STEP;
      if ((dir > 0 && next > value) || (dir < 0 && next < value)) next = value;
      currentAngle = next;
      servo.write(currentAngle); // using degrees is fine with ESP32Servo
    }
  }

  delay(1);
}
