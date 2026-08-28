#include "as5600.h"

#include <Wire.h>

namespace {

constexpr uint8_t kRegStatus = 0x0B;
constexpr uint8_t kRegRawAngle = 0x0C;
constexpr uint8_t kRegAgc = 0x1A;
constexpr uint8_t kRegMagnitude = 0x1B;

}  // namespace

bool As5600Sensor::readRegisters(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(addr_);
  Wire.write(reg);
  // Prefer STOP then restart — some AS5600 modules NACK repeated-start.
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  const size_t n = Wire.requestFrom(addr_, len);
  if (n != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; ++i) {
    buf[i] = Wire.read();
  }
  return true;
}

void As5600Sensor::begin(uint8_t sda, uint8_t scl, uint8_t addr) {
  addr_ = addr;
  Wire.begin(sda, scl);
  Wire.setClock(100000);  // 100 kHz is more tolerant on jumper-wire benches
  Wire.setTimeOut(10);    // default 50 ms; a NACK must not stall the motor loop
  ok_ = false;
  status_ok_ = false;
  raw_angle_ = 0;
  status_ = 0;
  agc_ = 0;
  magnitude_ = 0;
  field_div_ = 0;
  update();
}

bool As5600Sensor::update() {
  uint8_t head[3];
  if (!readRegisters(kRegStatus, head, 3)) {
    ok_ = false;
    status_ok_ = false;
    return false;
  }
  status_ = head[0];
  status_ok_ = true;
  raw_angle_ = static_cast<uint16_t>(((head[1] & 0x0F) << 8) | head[2]);
  ok_ = true;

  // AGC / magnitude are display-only. Skip most ticks so a weak bus cannot
  // double the I²C cost (and timeout) of every control loop.
  if ((++field_div_ & 0x0F) == 0) {
    uint8_t field[3];
    if (readRegisters(kRegAgc, field, 3)) {
      agc_ = field[0];
      magnitude_ = static_cast<uint16_t>(((field[1] & 0x0F) << 8) | field[2]);
    }
  }

  return true;
}
