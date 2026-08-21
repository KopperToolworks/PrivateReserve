#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "app.h"
#include "as5600.h"
#include "bottle_key.h"
#include "config.h"
#include "door.h"
#include "lcd.h"
#include "net.h"
#include "store.h"

namespace {
#define GFX_BL 38

Arduino_GFX* output_display = new Arduino_ST7789(
    new Arduino_ESP32PAR8Q(7, 6, 8, 9, 39, 40, 41, 42, 45, 46, 47, 48), 5, 0,
    true, 170, 320, 35, 0, 35, 0);
Arduino_Canvas* canvas =
    new Arduino_Canvas(320, 170, output_display, 0, 0, 0);
// Canvas rotation stays 0. Landscape and operator-upright come from
// output_display setRotation(kDisplayRotation).

BottleKeySensor bottle_key;
As5600Sensor as5600;

volatile int32_t rotary_count = 0;
volatile uint8_t rotary_state = 0;
int32_t rotary_seen = 0;
uint32_t last_frame_ms = 0;

constexpr int8_t kRotaryTable[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0,
};

void IRAM_ATTR onRotaryChange() {
  const uint8_t clk = digitalRead(pins::kRotaryClk);
  const uint8_t dt = digitalRead(pins::kRotaryDt);
  const uint8_t ns = static_cast<uint8_t>((clk << 1) | dt);
  const uint8_t idx = static_cast<uint8_t>((rotary_state << 2) | ns);
  rotary_count += kRotaryTable[idx];
  rotary_state = ns;
}
}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(200);

  pinMode(pins::kOpenButton, INPUT_PULLUP);
  pinMode(pins::kCloseButton, INPUT_PULLUP);
  pinMode(pins::kRotaryClk, INPUT_PULLUP);
  pinMode(pins::kRotaryDt, INPUT_PULLUP);
  pinMode(pins::kRotarySw, INPUT_PULLUP);
  pinMode(pins::kUserButton, INPUT_PULLUP);
  rotary_state = static_cast<uint8_t>((digitalRead(pins::kRotaryClk) << 1) |
                                      digitalRead(pins::kRotaryDt));
  attachInterrupt(digitalPinToInterrupt(pins::kRotaryClk), onRotaryChange,
                  CHANGE);
  attachInterrupt(digitalPinToInterrupt(pins::kRotaryDt), onRotaryChange,
                  CHANGE);

  store.begin();
  as5600.begin(pins::kAs5600Sda, pins::kAs5600Scl, kAs5600I2cAddr);
  bottle_key.begin();
  door.begin(&as5600);
  app.begin(&bottle_key);

  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  output_display->begin();
  output_display->setRotation(kDisplayRotation);
  canvas->begin(GFX_SKIP_OUTPUT_BEGIN);
  lcdBegin(canvas);

  netBegin();
  webBegin();

  Serial.println(F("private-reserve production"));
  Serial.println(F("Status is the default screen. EC11 or web to navigate."));
}

void loop() {
  netLoop();
  webLoop();
  door.update();

  noInterrupts();
  const int32_t raw = rotary_count;
  interrupts();
  const int32_t delta = raw - rotary_seen;
  rotary_seen = raw;
  if (delta) {
    app.addRotary(delta);
  }

  app.update();

  const uint32_t now = millis();
  if (now - last_frame_ms >= 100) {
    last_frame_ms = now;
    if (app.displayOn()) {
      lcdDraw(canvas);
      canvas->flush();
    }
  }
}
