#include "door.h"

#include "config.h"

#include <cmath>
#include <esp_task_wdt.h>

namespace {
constexpr uint8_t kPwmCh = 0;
constexpr float kDecayAmps = 0.15f;
constexpr uint32_t kDecayMinMs = 50;
constexpr uint32_t kJogStepMinMs = 200;
constexpr uint32_t kJogStepMaxMs = 1000;
constexpr uint32_t kJogQueueMaxMs = 4000;

float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

float hermite(float t) {
  t = clampf(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
}  // namespace

Door door;

void Door::begin(As5600Sensor* enc) {
  enc_ = enc;
  pinMode(pins::kPwm, OUTPUT);
  digitalWrite(pins::kPwm, LOW);
  pinMode(pins::kLimitUpper, INPUT_PULLUP);
  pinMode(pins::kLimitLower, INPUT_PULLUP);
  pinMode(pins::kDirectionRelay, OUTPUT);
  setRelay(Travel::Close);
  ledcSetup(kPwmCh, kPwmFrequencyHz, kPwmResolutionBits);
  ledcAttachPin(pins::kPwm, kPwmCh);
  applyDuty(0);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(pins::kCurrentSense, ADC_11db);
  analogReadResolution(12);
  esp_task_wdt_init(kWatchdogTimeoutSec, true);
  esp_task_wdt_add(nullptr);
  delay(kAcs712CalDelayMs);
  captureZero();
  debounceMarks();
  if (enc_ && enc_->ok()) {
    last_raw_ = enc_->rawAngle();
  }
  if (!store.somReady()) {
    fault_ = FaultId::PositionUnknown;
  } else {
    fault_ = FaultId::PositionUnknown;
    trusted_ = false;
  }
  phase_ = DoorPhase::Idle;
}

bool Door::magnetOk() const {
  return enc_ && enc_->ok() && enc_->magnetDetected() && !enc_->magnetTooWeak() &&
         !enc_->magnetTooStrong();
}

bool Door::busOk() const {
  return enc_ && enc_->ok();
}

const char* Door::magnetLabel() const {
  if (!busOk()) {
    return "unread";
  }
  if (enc_->magnetTooWeak()) {
    return "too weak";
  }
  if (enc_->magnetTooStrong()) {
    return "too strong";
  }
  if (enc_->magnetDetected()) {
    return "detected";
  }
  return "lost";
}

uint8_t Door::agc() const {
  return enc_ ? enc_->agc() : 0;
}

uint16_t Door::rawAngle() const {
  return enc_ ? enc_->rawAngle() : 0;
}

void Door::captureZero() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kAcs712AdcSamples; ++i) {
    sum += analogRead(pins::kCurrentSense);
    delayMicroseconds(200);
  }
  const float avg = static_cast<float>(sum) / kAcs712AdcSamples;
  zero_v_ = (avg / 4095.0f) * 3.3f;
  current_adc_ = static_cast<uint16_t>(avg + 0.5f);
}

void Door::setOriginHere() {
  origin_ = counts_;
  trusted_ = true;
  if (fault_ == FaultId::PositionUnknown) {
    fault_ = FaultId::None;
  }
}

void Door::sampleJitter(int32_t& min_c, int32_t& max_c) {
  const int32_t p = counts_;
  if (p < min_c) {
    min_c = p;
  }
  if (p > max_c) {
    max_c = p;
  }
}

void Door::applyDuty(uint16_t d) {
  duty_ = d;
  const uint16_t ledc =
      kPwmInvertDuty ? static_cast<uint16_t>(kPwmMaxDuty - d) : d;
  ledcWrite(kPwmCh, ledc);
  if (d == 0) {
    pwm_on_ms_ = 0;
  }
}

void Door::setRelay(Travel dir) {
  if (duty_ != 0) {
    applyDuty(0);
  }
  dir_ = dir;
  bool open = dir == Travel::Open;
  if (store.settings.direction_invert) {
    open = !open;
  }
  const bool level = kDirectionRelayOpenHigh ? open : !open;
  digitalWrite(pins::kDirectionRelay, level ? HIGH : LOW);
}

void Door::setPhase(DoorPhase p) {
  phase_ = p;
  phase_t_ = millis();
}

void Door::latch(FaultId id) {
  applyDuty(0);
  fault_ = id;
  last_event_ = id;
  setPhase(DoorPhase::Fault);
  reversed_this_chain_ = false;
}

const LoadTable* Door::table() const { return store.armedTable(); }

int Door::binIndex() const {
  const int32_t span = store.cal.close_edge_counts;
  if (span <= 0 || !trusted_) {
    return 0;
  }
  int32_t p = position();
  if (p < 0) {
    p = 0;
  }
  if (p > span) {
    p = span;
  }
  int b = static_cast<int>((static_cast<int64_t>(p) * (kBinCount - 1)) / span);
  if (inCreepOrSnug() && dir_ == Travel::Close) {
    b = kBinCount - 1;
  }
  if (b < 0) {
    b = 0;
  }
  if (b >= kBinCount) {
    b = kBinCount - 1;
  }
  return b;
}

const LoadBin* Door::binNow() const {
  const LoadTable* t = table();
  if (!t) {
    return nullptr;
  }
  const int b = binIndex();
  return dir_ == Travel::Close ? &t->close[b] : &t->open[b];
}

float Door::binTrip() const {
  const LoadBin* b = binNow();
  if (!b || !b->valid) {
    return kOvercurrentTripAmps;
  }
  return b->trip;
}

bool Door::inCreepOrSnug() const {
  return phase_ == DoorPhase::CreepClose || phase_ == DoorPhase::Snug ||
         phase_ == DoorPhase::CreepOpen;
}

uint32_t Door::phaseElapsedMs() const { return millis() - phase_t_; }

float Door::readAmps() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kAcs712AdcSamples; ++i) {
    sum += analogRead(pins::kCurrentSense);
    delayMicroseconds(200);
  }
  const float avg = static_cast<float>(sum) / kAcs712AdcSamples;
  current_adc_ = static_cast<uint16_t>(avg + 0.5f);
  const float v_adc = (avg / 4095.0f) * 3.3f;
  const float module =
      (v_adc - zero_v_) / kAcs712DividerRatio + kAcs712ModuleZeroVolts;
  return (module - kAcs712ModuleZeroVolts) / (kAcs712MvPerAmp / 1000.0f);
}

void Door::unwrap() {
  if (!enc_ || !enc_->update()) {
    if (running() && !sensorFreeJog()) {
      latch(FaultId::SensorFault);
    }
    return;
  }
  if (!magnetOk()) {
    if (running() && !sensorFreeJog()) {
      latch(FaultId::SensorFault);
    }
    return;
  }
  const uint16_t raw = enc_->rawAngle();
  int32_t d = static_cast<int32_t>(raw) - static_cast<int32_t>(last_raw_);
  if (d > 2048) {
    d -= 4096;
  } else if (d < -2048) {
    d += 4096;
  }
  if (d > 2048 || d < -2048) {
    trusted_ = false;
    if (running() && !sensorFreeJog()) {
      latch(FaultId::SensorFault);
    }
    last_raw_ = raw;
    return;
  }
  const int dead = store.settings.as5600_jitter_counts;
  if (duty_ == 0 && std::abs(d) <= dead) {
    d = 0;
  }
  last_delta_ = d;
  counts_ += d;
  last_raw_ = raw;
  rps_acc_ += d;
  const uint32_t now = millis();
  if (now - rps_t_ >= 200) {
    rps_ = (static_cast<float>(rps_acc_) / 4096.0f) * (1000.0f / (now - rps_t_));
    rps_acc_ = 0;
    rps_t_ = now;
  }
}

void Door::debounceMarks() {
  const bool raw_open =
      digitalRead(pins::kLimitUpper) == (kLimitActiveLow ? LOW : HIGH);
  const bool raw_close =
      digitalRead(pins::kLimitLower) == (kLimitActiveLow ? LOW : HIGH);
  auto step = [](bool raw, bool& stable, bool& pending, uint8_t& n) {
    if (raw == stable) {
      n = 0;
      return;
    }
    if (raw == pending) {
      if (++n >= kLimitDebounceCount) {
        stable = raw;
        n = 0;
      }
    } else {
      pending = raw;
      n = 1;
    }
  };
  step(raw_open, open_mark_, open_pending_, open_n_);
  step(raw_close, close_mark_, close_pending_, close_n_);
}

void Door::enterBlank(Travel next) {
  pending_dir_ = next;
  applyDuty(0);
  setPhase(DoorPhase::DecayWait);
}

void Door::startMove(Travel dir, bool reverse) {
  next_is_cal_ = false;
  next_is_jog_ = false;
  if (bothMarks()) {
    latch(FaultId::LimitHit);
    snprintf(hint_, sizeof(hint_), "both markers active");
    return;
  }
  if (!reverse) {
    reversed_this_chain_ = false;
    resync_done_ = false;
    move_t0_ = millis();
  }
  enterBlank(dir);
}

void Door::requestOpen() { startMove(Travel::Open, running()); }
void Door::requestClose() { startMove(Travel::Close, running()); }

void Door::cancel() { stopMotionKeepK1(); }

void Door::stopMotionKeepK1() {
  applyDuty(0);
  jog_remaining_ms_ = 0;
  next_is_jog_ = false;
  if (phase_ != DoorPhase::Fault) {
    setPhase(DoorPhase::Idle);
  }
  reversed_this_chain_ = false;
}

void Door::acknowledgeFault() {
  if (phase_ != DoorPhase::Fault && fault_ != FaultId::PositionUnknown) {
    return;
  }
  if (std::fabs(amps_) >= kDecayAmps) {
    snprintf(hint_, sizeof(hint_), "current still high");
    return;
  }
  if (fault_ == FaultId::LimitHit && bothMarks()) {
    snprintf(hint_, sizeof(hint_), "both markers still active");
    return;
  }
  if (fault_ == FaultId::PositionUnknown) {
    return;
  }
  fault_ = trusted_ ? FaultId::None : FaultId::PositionUnknown;
  setPhase(DoorPhase::Idle);
  applyDuty(0);
}

bool Door::sensorFreeJog() const {
  return next_is_jog_ || phase_ == DoorPhase::Jog;
}

Travel Door::jogCommandDir() const {
  if (phase_ == DoorPhase::DecayWait || phase_ == DoorPhase::PwmBlank ||
      phase_ == DoorPhase::RelaySettle) {
    return pending_dir_;
  }
  return dir_;
}

uint32_t Door::jogStepMs() const {
  uint32_t step = store.settings.jog_step_ms;
  if (step < kJogStepMinMs) {
    step = kJogStepMinMs;
  }
  if (step > kJogStepMaxMs) {
    step = kJogStepMaxMs;
  }
  return step;
}

void Door::clearFaultForJog() {
  if (phase_ != DoorPhase::Fault) {
    return;
  }
  applyDuty(0);
  if (fault_ != FaultId::PositionUnknown) {
    fault_ = trusted_ ? FaultId::None : FaultId::PositionUnknown;
  }
  setPhase(DoorPhase::Idle);
}

void Door::startJog(Travel dir) {
  clearFaultForJog();
  const uint32_t step = jogStepMs();
  last_event_ = FaultId::None;
  next_is_cal_ = false;

  if (sensorFreeJog()) {
    if (jogCommandDir() == dir) {
      jog_remaining_ms_ += step;
      if (jog_remaining_ms_ > kJogQueueMaxMs) {
        jog_remaining_ms_ = kJogQueueMaxMs;
      }
      return;
    }
    if (jog_remaining_ms_ > step) {
      jog_remaining_ms_ -= step;
      return;
    }
    // Opposite detent used up the queue. Start the other way through the
    // same PWM-off / decay / blank / K1 / settle path as every other reverse.
    jog_remaining_ms_ = step;
    next_is_jog_ = true;
    enterBlank(dir);
    return;
  }

  jog_remaining_ms_ = step;
  next_is_jog_ = true;
  enterBlank(dir);
}

void Door::startCreep(Travel dir) {
  if (phase_ == DoorPhase::Fault && fault_ != FaultId::PositionUnknown) {
    return;
  }
  next_is_cal_ = true;
  next_is_jog_ = false;
  startMove(dir, running());
  next_is_cal_ = true;
}

void Door::holdCreep() {
  if (phase_ == DoorPhase::Idle || phase_ == DoorPhase::RelaySettle ||
      phase_ == DoorPhase::PwmBlank || phase_ == DoorPhase::DecayWait) {
    return;
  }
  setPhase(DoorPhase::Calibrating);
  applyDuty(store.settings.pwm_creep_duty);
}

void Door::checkHardCurrent(uint32_t now) {
  if (std::fabs(amps_) >= kOvercurrentTripAmps) {
    if (!oc_hold_) {
      oc_hold_ = true;
      oc_t_ = now;
    } else if (now - oc_t_ >= kOvercurrentHoldMs) {
      latch(FaultId::HardOvercurrent);
    }
  } else {
    oc_hold_ = false;
  }
}

void Door::checkEnvelope(uint32_t now) {
  if (inCreepOrSnug()) {
    return;
  }
  if (phase_ == DoorPhase::Jog || phase_ == DoorPhase::Calibrating) {
    return;
  }
  if (pwm_on_ms_ < store.settings.obstruction_inrush_skip_ms) {
    return;
  }
  const LoadBin* b = binNow();
  if (!b || !b->valid) {
    return;
  }
  if (std::fabs(amps_) >= b->trip) {
    if (!env_hold_) {
      env_hold_ = true;
      env_t_ = now;
    } else if (now - env_t_ >= kOvercurrentHoldMs) {
      env_hold_ = false;
      if (dir_ == Travel::Open) {
        latch(FaultId::ObstructionWhileOpening);
        return;
      }
      if (reversed_this_chain_) {
        latch(FaultId::RepeatedObstructionClose);
        return;
      }
      last_event_ = FaultId::LearnedObstructionClose;
      reversed_this_chain_ = true;
      startMove(Travel::Open, true);
    }
  } else {
    env_hold_ = false;
  }
}

void Door::checkStall(uint32_t now) {
  if (duty_ < store.settings.pwm_min_duty) {
    stall_t_ = now;
    stall_pos_ = counts_;
    return;
  }
  const int32_t need = store.settings.as5600_min_progress_counts;
  if (std::abs(counts_ - stall_pos_) > need) {
    stall_t_ = now;
    stall_pos_ = counts_;
    return;
  }
  if (now - stall_t_ >= kMotionWatchdogMs) {
    latch(FaultId::MotionStall);
  }
}

void Door::checkTravelCap() {
  if (!trusted_ || !store.cal.open_marker_ok || !store.cal.close_marker_ok) {
    return;
  }
  const int32_t cap = store.settings.travel_cap_counts;
  if (dir_ == Travel::Open && position() < -cap) {
    latch(FaultId::TravelLimitExceeded);
  }
  if (dir_ == Travel::Close &&
      position() > store.cal.close_edge_counts + cap) {
    latch(FaultId::TravelLimitExceeded);
  }
}

uint16_t Door::smoothDuty(uint32_t elapsed, uint32_t dur, uint16_t from,
                          uint16_t to) const {
  if (dur == 0) {
    return to;
  }
  const float s = hermite(static_cast<float>(elapsed) / static_cast<float>(dur));
  const float v = static_cast<float>(from) +
                  s * (static_cast<float>(to) - static_cast<float>(from));
  return static_cast<uint16_t>(v + 0.5f);
}

void Door::advance(uint32_t now) {
  const Settings& s = store.settings;
  const uint32_t elapsed = now - phase_t_;

  switch (phase_) {
    case DoorPhase::DecayWait:
      if (elapsed < kDecayMinMs) {
        return;
      }
      if (std::fabs(amps_) > kDecayAmps) {
        if (elapsed <= s.current_decay_timeout_ms) {
          return;
        }
        if (!next_is_jog_) {
          latch(FaultId::CurrentDecayTimeout);
          return;
        }
        // Jog cannot depend on the ACS712. After the timeout, K1 still
        // waits for blank-before; it does not move in this phase.
      }
      setPhase(DoorPhase::PwmBlank);
      break;

    case DoorPhase::PwmBlank:
      if (elapsed >= s.pwm_blank_before_dir_ms) {
        setRelay(pending_dir_);
        setPhase(DoorPhase::RelaySettle);
      }
      break;

    case DoorPhase::RelaySettle: {
      const uint32_t after =
          s.pwm_blank_after_dir_ms > s.relay_settle_ms ? s.pwm_blank_after_dir_ms
                                                       : s.relay_settle_ms;
      if (elapsed >= after) {
        if (next_is_jog_) {
          setPhase(DoorPhase::Jog);
          applyDuty(s.pwm_jog_duty);
          break;
        }
        if (next_is_cal_) {
          setPhase(DoorPhase::Calibrating);
          applyDuty(s.pwm_creep_duty);
          pwm_on_ms_ = 0;
          stall_t_ = now;
          stall_pos_ = counts_;
          break;
        }
        setPhase(DoorPhase::RampUp);
        pwm_on_ms_ = 0;
        stall_t_ = now;
        stall_pos_ = counts_;
      }
      break;
    }

    case DoorPhase::RampUp:
      applyDuty(smoothDuty(elapsed, s.move_accel_ms, s.pwm_min_duty,
                           s.pwm_max_duty));
      pwm_on_ms_ += kControlLoopMs;
      if (elapsed >= s.move_accel_ms) {
        setPhase(DoorPhase::Cruise);
      }
      break;

    case DoorPhase::Cruise:
      applyDuty(s.pwm_max_duty);
      pwm_on_ms_ += kControlLoopMs;
      break;

    case DoorPhase::RampDown: {
      const uint16_t d =
          smoothDuty(elapsed, s.move_decel_ms, s.pwm_max_duty, 0);
      applyDuty(d);
      if (elapsed >= s.move_decel_ms) {
        applyDuty(0);
        setPhase(DoorPhase::Idle);
        reversed_this_chain_ = false;
      }
      break;
    }

    case DoorPhase::CreepClose:
      applyDuty(s.pwm_creep_duty);
      pwm_on_ms_ += kControlLoopMs;
      if (std::fabs(amps_) >= s.snug_amps) {
        setPhase(DoorPhase::Snug);
      }
      if (trusted_ &&
          position() > store.cal.close_edge_counts + s.close_limit_to_seated) {
        applyDuty(0);
        setPhase(DoorPhase::Idle);
        reversed_this_chain_ = false;
      }
      break;

    case DoorPhase::Snug:
      applyDuty(s.pwm_creep_duty);
      if (elapsed > 1500 ||
          (trusted_ &&
           position() > store.cal.close_edge_counts + s.close_limit_to_seated)) {
        applyDuty(0);
        setPhase(DoorPhase::Idle);
        reversed_this_chain_ = false;
      }
      break;

    case DoorPhase::CreepOpen:
      applyDuty(s.pwm_creep_duty);
      pwm_on_ms_ += kControlLoopMs;
      if (trusted_ && position() < -s.open_limit_to_open) {
        applyDuty(0);
        setPhase(DoorPhase::Idle);
        reversed_this_chain_ = false;
      }
      if (elapsed > 2500) {
        applyDuty(0);
        setPhase(DoorPhase::Idle);
        reversed_this_chain_ = false;
      }
      break;

    case DoorPhase::Jog:
      if (jog_remaining_ms_ <= kControlLoopMs) {
        stopMotionKeepK1();
        break;
      }
      jog_remaining_ms_ -= kControlLoopMs;
      applyDuty(s.pwm_jog_duty);
      pwm_on_ms_ += kControlLoopMs;
      break;

    case DoorPhase::Calibrating:
      applyDuty(s.pwm_creep_duty);
      pwm_on_ms_ += kControlLoopMs;
      break;

    default:
      break;
  }

  if (running() && phase_ != DoorPhase::Jog &&
      phase_ != DoorPhase::Calibrating &&
      now - move_t0_ > s.move_timeout_ms && move_t0_ != 0) {
    latch(FaultId::MoveTimeout);
  }
}

void Door::update() {
  esp_task_wdt_reset();
  unwrap();
  debounceMarks();
  current_tick_ = !current_tick_;
  if (current_tick_) {
    amps_ = readAmps();
  }

  if (phase_ == DoorPhase::Fault) {
    applyDuty(0);
    return;
  }

  if (bothMarks() && (running() || idle()) && !sensorFreeJog()) {
    if (running()) {
      latch(FaultId::LimitHit);
      snprintf(hint_, sizeof(hint_), "both markers active");
      return;
    }
  }

  if (running() && !sensorFreeJog() && phase_ != DoorPhase::DecayWait &&
      phase_ != DoorPhase::PwmBlank && phase_ != DoorPhase::RelaySettle) {
    checkHardCurrent(millis());
    if (phase_ == DoorPhase::Fault) {
      return;
    }
    checkEnvelope(millis());
    if (phase_ == DoorPhase::Fault) {
      return;
    }
    checkStall(millis());
    if (phase_ == DoorPhase::Fault) {
      return;
    }
    checkTravelCap();
    if (phase_ == DoorPhase::Fault) {
      return;
    }

    const bool dest_open = dir_ == Travel::Open && openMark();
    const bool dest_close = dir_ == Travel::Close && closeMark();
    if (phase_ != DoorPhase::Calibrating && phase_ != DoorPhase::Jog) {
      if (dest_open && !inCreepOrSnug()) {
        if (trusted_ && store.cal.open_marker_ok && !resync_done_) {
          const int32_t drift = position() - 0;
          store.cal.drift_open = drift;
          if (std::abs(drift) > store.settings.as5600_resync_drift_counts) {
            latch(FaultId::ReSyncDrift);
            return;
          }
          origin_ = counts_;
          resync_done_ = true;
        }
        setPhase(DoorPhase::CreepOpen);
      } else if (dest_close && !inCreepOrSnug()) {
        if (trusted_ && store.cal.close_marker_ok && !resync_done_) {
          const int32_t drift = position() - store.cal.close_edge_counts;
          store.cal.drift_close = drift;
          if (std::abs(drift) > store.settings.as5600_resync_drift_counts) {
            latch(FaultId::ReSyncDrift);
            return;
          }
          origin_ = counts_ - store.cal.close_edge_counts;
          resync_done_ = true;
        }
        setPhase(DoorPhase::CreepClose);
      }
    }
  }

  advance(millis());
}
