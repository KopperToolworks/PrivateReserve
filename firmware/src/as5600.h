#pragma once

#include <Arduino.h>

// Minimal AS5600 magnetic rotary encoder (I²C). See docs/T-Display-S3_Pinout.md.
class As5600Sensor {
 public:
  void begin(uint8_t sda, uint8_t scl, uint8_t addr = 0x36);
  bool update();

  bool ok() const { return ok_; }
  uint16_t rawAngle() const { return raw_angle_; }  // 0..4095
  float angleDegrees() const { return raw_angle_ * (360.0f / 4096.0f); }

  // STATUS (0x0B): magnet detect / too-weak / too-strong
  bool statusOk() const { return status_ok_; }
  bool magnetDetected() const { return (status_ & 0x20) != 0; }
  bool magnetTooWeak() const { return (status_ & 0x10) != 0; }
  bool magnetTooStrong() const { return (status_ & 0x08) != 0; }
  uint8_t statusRaw() const { return status_; }

  // AGC (0x1A): 0..255 — mid-range (~128) is a healthy air gap
  uint8_t agc() const { return agc_; }
  // MAGNITUDE (0x1B): 12-bit field strength
  uint16_t magnitude() const { return magnitude_; }

 private:
  bool readRegisters(uint8_t reg, uint8_t* buf, uint8_t len);

  uint8_t addr_ = 0x36;
  uint16_t raw_angle_ = 0;
  uint8_t status_ = 0;
  uint8_t agc_ = 0;
  uint16_t magnitude_ = 0;
  bool ok_ = false;
  bool status_ok_ = false;
};
