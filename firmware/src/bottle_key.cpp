#include "bottle_key.h"

#include "adc_util.h"
#include "config.h"

void BottleKeySensor::begin() {
  absent_adc_ = kHallAdcMin;
  present_adc_ = kHallAdcMax;
  last_read_ms_ = 0;
  state_ = State::Unknown;
  recomputeThresholds();
  update();
}

uint32_t BottleKeySensor::adcReadIntervalMs() const {
  if (kEnableWebPortal && isAdc2Pin(pins::kHallAnalog)) {
    return kWebSampleMs;
  }
  return 0;
}

uint16_t BottleKeySensor::readAdc() {
  return readAnalogPin(pins::kHallAnalog, kHallAdcSamples);
}

void BottleKeySensor::recomputeThresholds() {
  const int32_t lo = static_cast<int32_t>(
      absent_adc_ < present_adc_ ? absent_adc_ : present_adc_);
  const int32_t hi = static_cast<int32_t>(
      absent_adc_ > present_adc_ ? absent_adc_ : present_adc_);
  const int32_t gap = hi - lo;
  calibrated_ = gap > 50;
  if (!calibrated_) {
    enter_adc_ = 0;
    exit_adc_ = 0;
    return;
  }

  // Enter present about 60% of the way across the gap. Leave about 40%.
  const int32_t enter_offset =
      static_cast<int32_t>(gap * (0.5f + kHallDeviationFraction));
  const int32_t exit_offset =
      static_cast<int32_t>(gap * (0.5f - kHallDeviationFraction));
  if (present_adc_ > absent_adc_) {
    enter_adc_ = static_cast<uint16_t>(absent_adc_ + enter_offset);
    exit_adc_ = static_cast<uint16_t>(absent_adc_ + exit_offset);
  } else {
    enter_adc_ = static_cast<uint16_t>(absent_adc_ - enter_offset);
    exit_adc_ = static_cast<uint16_t>(absent_adc_ - exit_offset);
  }
}

void BottleKeySensor::classify() {
  if (!calibrated_) {
    state_ = State::Unknown;
    return;
  }

  const bool rising = present_adc_ > absent_adc_;
  if (state_ != State::Present) {
    const bool enter =
        rising ? last_adc_ >= enter_adc_ : last_adc_ <= enter_adc_;
    if (enter) {
      state_ = State::Present;
    } else {
      state_ = State::Absent;
    }
    return;
  }

  const bool leave =
      rising ? last_adc_ <= exit_adc_ : last_adc_ >= exit_adc_;
  if (leave) {
    state_ = State::Absent;
  }
}

void BottleKeySensor::update() {
  const uint32_t now_ms = millis();
  const uint32_t interval = adcReadIntervalMs();
  if (interval > 0 && (now_ms - last_read_ms_) < interval) {
    return;
  }
  last_read_ms_ = now_ms;
  last_adc_ = readAdc();
  classify();
}

void BottleKeySensor::captureAbsent() {
  last_read_ms_ = 0;
  update();
  absent_adc_ = last_adc_;
  recomputeThresholds();
  classify();
  Serial.print(F("[bottle-key] absent adc="));
  Serial.println(absent_adc_);
}

void BottleKeySensor::loadLevels(uint16_t absent_adc, uint16_t present_adc) {
  absent_adc_ = absent_adc;
  present_adc_ = present_adc;
  recomputeThresholds();
  classify();
}

void BottleKeySensor::capturePresent() {
  last_read_ms_ = 0;
  update();
  present_adc_ = last_adc_;
  recomputeThresholds();
  classify();
  Serial.print(F("[bottle-key] present adc="));
  Serial.println(present_adc_);
}
