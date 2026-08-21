#pragma once

#include <Arduino.h>

// SS49E on GPIO1 — bottle door key. Present or absent with hysteresis.
// Not a travel sensor. Door position is accumulated AS5600 counts.
class BottleKeySensor {
 public:
  enum class State : uint8_t { Unknown, Absent, Present };

  void begin();
  void update();

  uint16_t rawAdc() const { return last_adc_; }
  State state() const { return state_; }
  bool present() const { return state_ == State::Present; }
  bool calibrated() const { return calibrated_; }

  void captureAbsent();
  void capturePresent();
  void loadLevels(uint16_t absent_adc, uint16_t present_adc);

  uint16_t absentLevel() const { return absent_adc_; }
  uint16_t presentLevel() const { return present_adc_; }
  uint16_t enterThreshold() const { return enter_adc_; }
  uint16_t exitThreshold() const { return exit_adc_; }

 private:
  uint16_t readAdc();
  uint32_t adcReadIntervalMs() const;
  void recomputeThresholds();
  void classify();

  uint16_t absent_adc_ = 0;
  uint16_t present_adc_ = 0;
  uint16_t enter_adc_ = 0;
  uint16_t exit_adc_ = 0;
  uint16_t last_adc_ = 0;
  uint32_t last_read_ms_ = 0;
  State state_ = State::Unknown;
  bool calibrated_ = false;
};
