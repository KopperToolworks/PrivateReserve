#include "app.h"

#include "config.h"
#include "ec11_calibration.h"

#include <WiFi.h>
#include <cmath>
#include <cstring>

namespace {
float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

void copy(char* dst, size_t n, const char* s) {
  strncpy(dst, s ? s : "", n - 1);
  dst[n - 1] = 0;
}

uint16_t adcMv(uint16_t adc) {
  return static_cast<uint16_t>((static_cast<uint32_t>(adc) * 3300u) / 4095u);
}
}  // namespace

App app;

void App::begin(BottleKeySensor* key) {
  key_ = key;
  last_activity_ = millis();
  if (store.cal.bottle_key_ok) {
    key_->loadLevels(store.settings.bottle_key_absent_adc,
                     store.settings.bottle_key_present_adc);
  }
  goStatus();
}

void App::setDisplayOn(bool on) {
  display_on_ = on;
  digitalWrite(kPanelPowerPin, on ? HIGH : LOW);
  digitalWrite(kBacklightPin, on ? HIGH : LOW);
}

uint32_t App::displayRemainMs() const {
  const uint32_t to = store.settings.display_timeout_ms;
  if (to == 0 || screen_ != Screen::Status) {
    return to;
  }
  const uint32_t elapsed = millis() - last_activity_;
  return elapsed >= to ? 0 : to - elapsed;
}

void App::wake() {
  last_activity_ = millis();
  if (!display_on_) {
    setDisplayOn(true);
    goStatus();
  }
}

void App::maybeBlank(uint32_t now) {
  // Needs-setup / PositionUnknown is the Status screen, not a prompt.
  // Only a latched Fault phase, a live move, or a non-Status screen
  // should hold the panel on.
  if (screen_ != Screen::Status || door.running() ||
      door.phase() == DoorPhase::Fault) {
    last_activity_ = now;
    return;
  }
  if (store.settings.display_timeout_ms == 0) {
    return;
  }
  if (now - last_activity_ >= store.settings.display_timeout_ms) {
    setDisplayOn(false);
  }
}

void App::addRotary(int32_t steps) {
  rotary_raw_ += steps;
  const bool sw_down = digitalRead(pins::kRotarySw) ==
                       (kRotarySwitchActiveLow ? LOW : HIGH);
  const int32_t next =
      (rotary_raw_ * kEc11DetentScaleNumerator) / kEc11DetentScaleDenominator;
  if (sw_down) {
    detents_ = next;
    return;
  }
  const int32_t d = next - detents_;
  detents_ = next;
  if (d) {
    handle(Cmd::Turn, d > 0 ? 1 : -1);
  }
}

void App::noteMenuTurn(Screen s0, uint8_t c0) {
  if (screen_ == s0 && cursor_ == c0) {
    return;
  }
  prev_screen_ = s0;
  prev_cursor_ = c0;
  last_turn_ms_ = millis();
}

void App::revertRecentTurn() {
  if (last_turn_ms_ == 0) {
    return;
  }
  if (millis() - last_turn_ms_ >= kEc11PressRevertMs) {
    return;
  }
  screen_ = prev_screen_;
  cursor_ = prev_cursor_;
  last_turn_ms_ = 0;
}

void App::go(Screen s) {
  clearDiagInject();
  screen_ = s;
  cursor_ = 0;
  scroll_ = 0;
  confirm_hits_ = 0;
  last_activity_ = millis();
  last_turn_ms_ = 0;
  ignore_[0] = 0;
  if (s == Screen::Status) {
    owner_ = MotorOwner::Som;
  } else if (s == Screen::DiagJog) {
    owner_ = MotorOwner::Jog;
  } else if (s == Screen::DiagObstTest) {
    owner_ = MotorOwner::ObstTest;
  } else if (s >= Screen::CalPathClear && s <= Screen::CalLearnLoaded) {
    owner_ = MotorOwner::Cal;
  } else if (s == Screen::CalMenu || s == Screen::DiagMenu ||
             s == Screen::SetMenu || s == Screen::ModeMenu ||
             s == Screen::Docs) {
    if (!door.running()) {
      owner_ = MotorOwner::Som;
    }
  }
}

void App::goStatus() {
  owner_ = MotorOwner::Som;
  go(Screen::Status);
}

bool App::somBlocked() const {
  return !store.somReady() || !door.positionTrusted() ||
         door.phase() == DoorPhase::Fault;
}

void App::handle(Cmd cmd, int32_t arg, const char* text) {
  wake();
  if (cmd == Cmd::JogBeat) {
    if (screen_ == Screen::DiagJog) {
      door.jogBeat();
      if (arg < 0) {
        door.startJog(Travel::Open);
      } else if (arg > 0) {
        door.startJog(Travel::Close);
      }
    }
    return;
  }
  if (cmd == Cmd::WebText && text) {
    if (strcmp(text, "ssid") == 0 && arg == 0) {
      return;
    }
    // arg unused; text is "ssid=..." or handled by web.cpp via pending
    (void)arg;
  }
  if (cmd == Cmd::Turn) {
    onTurn(arg);
  } else if (cmd == Cmd::Press) {
    onPress();
  } else if (cmd == Cmd::Hold) {
    onHold();
  }
}

void App::cancelProc() {
  door.cancel();
  owner_ = MotorOwner::Som;
  if (screen_ >= Screen::CalMenu && screen_ <= Screen::CalReview) {
    go(Screen::CalMenu);
  } else if (screen_ == Screen::DiagJog || screen_ == Screen::DiagObstTest) {
    go(Screen::DiagMenu);
  }
  copy(ignore_, sizeof(ignore_), "cancelled");
}

void App::onHold() {
  if (door.fault() != FaultId::None && door.fault() != FaultId::PositionUnknown &&
      screen_ == Screen::Status) {
    return;
  }
  switch (screen_) {
    case Screen::Status:
      go(Screen::ModeMenu);
      break;
    case Screen::ModeMenu:
      goStatus();
      break;
    case Screen::DiagMenu:
    case Screen::CalMenu:
    case Screen::SetMenu:
      goStatus();
      break;
    case Screen::Docs:
      go(Screen::ModeMenu);
      break;
    case Screen::DiagJog:
    case Screen::DiagObstTest:
      cancelProc();
      break;
    case Screen::SetEdit:
    case Screen::SetConfirm:
      go(Screen::SetMenu);
      break;
    case Screen::SetReview:
      store.copyPendingFromLive();
      setting_dirty_ = false;
      go(Screen::SetMenu);
      break;
    case Screen::CalPathClear:
    case Screen::CalCurrentZero:
    case Screen::CalJitter:
    case Screen::CalBottleKey:
    case Screen::CalDirection:
    case Screen::CalOpenMarker:
    case Screen::CalCloseMarker:
    case Screen::CalSeated:
    case Screen::CalStroke:
    case Screen::CalLearnEmpty:
    case Screen::CalLearnLoaded:
      cancelProc();
      break;
    case Screen::CalReview:
      go(Screen::CalMenu);
      break;
    default:
      if (screen_ >= Screen::DiagBottleKey && screen_ <= Screen::DiagCalStatus) {
        go(Screen::DiagMenu);
      } else if (screen_ >= Screen::SetMotion &&
                 screen_ <= Screen::SetWifiCleared) {
        go(Screen::SetMenu);
      } else {
        goStatus();
      }
      break;
  }
}

void App::startStage(Screen s) {
  path_ok_[0] = path_ok_[1] = path_ok_[2] = false;
  probe_n_ = 0;
  probe_spread_ = 0;
  key_capture_ = 0;
  learn_run_ = 0;
  stage_t0_ = millis();
  jitter_min_ = door.position();
  jitter_max_ = door.position();
  cal_target_ = s;
  const bool moves = s == Screen::CalDirection || s == Screen::CalOpenMarker ||
                     s == Screen::CalCloseMarker || s == Screen::CalSeated ||
                     s == Screen::CalLearnEmpty || s == Screen::CalLearnLoaded;
  if (moves) {
    go(Screen::CalPathClear);
    return;
  }
  go(s);
}

void App::onTurn(int32_t n) {
  if (n == 0) {
    return;
  }
  const Screen s0 = screen_;
  const uint8_t c0 = cursor_;
  const int dir = n > 0 ? 1 : -1;

  if (screen_ == Screen::DiagJog) {
    door.startJog(dir > 0 ? Travel::Close : Travel::Open);
    return;
  }
  if (screen_ == Screen::SetEdit) {
    applyEdit(dir);
    return;
  }
  if (screen_ == Screen::CalStroke) {
    store.pending.stroke_inches =
        clampf(store.pending.stroke_inches + dir * 0.1f, 10.0f, 48.0f);
    return;
  }
  if (screen_ == Screen::CalPathClear) {
    int i = cursor_ + dir;
    if (i < 0) {
      i = 0;
    }
    if (i > 2) {
      i = 2;
    }
    cursor_ = static_cast<uint8_t>(i);
    noteMenuTurn(s0, c0);
    return;
  }
  if (screen_ == Screen::DiagTableClose && dir) {
    go(Screen::DiagTableOpen);
    noteMenuTurn(s0, c0);
    return;
  }
  if (screen_ == Screen::DiagTableOpen && dir) {
    go(Screen::DiagTableClose);
    noteMenuTurn(s0, c0);
    return;
  }
  if (screen_ == Screen::DiagLimits || screen_ == Screen::DiagRockers ||
      screen_ == Screen::DiagBottleKey) {
    if (last_turn_ms_ != 0 &&
        millis() - last_turn_ms_ < kEc11TwoChoiceSettleMs) {
      return;
    }
    int i = static_cast<int>(cursor_) + dir;
    if (i < 0) {
      i = 0;
    }
    if (i > 1) {
      i = 1;
    }
    cursor_ = static_cast<uint8_t>(i);
    if (cursor_ != c0) {
      confirm_hits_ = 0;
    }
    noteMenuTurn(s0, c0);
    last_turn_ms_ = millis();
    prev_screen_ = s0;
    prev_cursor_ = c0;
    return;
  }

  auto wrap = [&](uint8_t count) {
    int i = static_cast<int>(cursor_) + dir;
    if (i < 0) {
      i = count - 1;
    }
    if (i >= count) {
      i = 0;
    }
    cursor_ = static_cast<uint8_t>(i);
  };

  switch (screen_) {
    case Screen::ModeMenu:
      wrap(5);
      break;
    case Screen::DiagMenu:
      wrap(12);
      break;
    case Screen::CalMenu:
      wrap(9);
      break;
    case Screen::SetMenu:
      wrap(7);
      break;
    case Screen::SetMotion:
      wrap(7);
      break;
    case Screen::SetTravel:
      wrap(6);
      break;
    case Screen::SetObstruction:
      wrap(5);
      break;
    case Screen::SetTriggers:
      wrap(3);
      break;
    case Screen::SetDisplay:
      wrap(3);
      break;
    case Screen::SetService:
      wrap(5);
      break;
    case Screen::SetReview:
      wrap(4);
      break;
    default:
      break;
  }
  if (cursor_ != c0) {
    confirm_hits_ = 0;
  }
  noteMenuTurn(s0, c0);
}

void App::applyEdit(int32_t dir) {
  auto stepI = [&](int32_t lo, int32_t hi, int32_t st) {
    pending_int_ += dir * st;
    if (pending_int_ < lo) {
      pending_int_ = lo;
    }
    if (pending_int_ > hi) {
      pending_int_ = hi;
    }
  };
  switch (edit_field_) {
    case EditField::CruiseDuty:
      stepI(128, 768, 8);
      break;
    case EditField::MinDuty:
      stepI(32, 256, 8);
      break;
    case EditField::CreepDuty:
    case EditField::JogDuty:
      stepI(48, 256, 8);
      break;
    case EditField::AccelMs:
    case EditField::CruiseMs:
    case EditField::DecelMs:
      stepI(500, 8000, 100);
      break;
    case EditField::DisplayTimeoutMin:
      stepI(1, 60, 1);
      break;
    case EditField::HoldMinMs:
      stepI(200, 1200, 50);
      break;
    case EditField::HoldMaxMs:
      stepI(600, 4000, 100);
      break;
    case EditField::SwitchDebounce:
      stepI(5, 80, 5);
      break;
    case EditField::JoinTimeoutMs:
      stepI(3000, 30000, 1000);
      break;
    case EditField::InrushSkip:
      stepI(100, 1500, 50);
      break;
    case EditField::SnugAmpsX100:
      stepI(50, 250, 5);
      break;
    case EditField::TravelCap:
      stepI(40, 800, 10);
      break;
    case EditField::ProbeTol:
      stepI(8, 200, 2);
      break;
    case EditField::ResyncDrift:
      stepI(10, 400, 5);
      break;
    case EditField::Polarity:
      pending_int_ = pending_int_ ? 0 : 1;
      break;
    default:
      break;
  }
}

void App::acceptEdit() {
  Settings& p = store.pending;
  bool invalidates = false;
  switch (edit_field_) {
    case EditField::CruiseDuty:
      if (p.pwm_max_duty != static_cast<uint16_t>(pending_int_)) {
        invalidates = true;
      }
      p.pwm_max_duty = static_cast<uint16_t>(pending_int_);
      go(invalidates ? Screen::SetConfirm : Screen::SetMotion);
      break;
    case EditField::MinDuty:
      p.pwm_min_duty = static_cast<uint16_t>(pending_int_);
      go(Screen::SetMotion);
      break;
    case EditField::CreepDuty:
      p.pwm_creep_duty = static_cast<uint16_t>(pending_int_);
      go(Screen::SetMotion);
      break;
    case EditField::JogDuty:
      p.pwm_jog_duty = static_cast<uint16_t>(pending_int_);
      go(Screen::SetMotion);
      break;
    case EditField::AccelMs:
      if (p.move_accel_ms != static_cast<uint32_t>(pending_int_)) {
        invalidates = true;
      }
      p.move_accel_ms = static_cast<uint32_t>(pending_int_);
      go(invalidates ? Screen::SetConfirm : Screen::SetMotion);
      break;
    case EditField::CruiseMs:
      if (p.move_cruise_ms != static_cast<uint32_t>(pending_int_)) {
        invalidates = true;
      }
      p.move_cruise_ms = static_cast<uint32_t>(pending_int_);
      go(invalidates ? Screen::SetConfirm : Screen::SetMotion);
      break;
    case EditField::DecelMs:
      if (p.move_decel_ms != static_cast<uint32_t>(pending_int_)) {
        invalidates = true;
      }
      p.move_decel_ms = static_cast<uint32_t>(pending_int_);
      go(invalidates ? Screen::SetConfirm : Screen::SetMotion);
      break;
    case EditField::DisplayTimeoutMin:
      p.display_timeout_ms = static_cast<uint32_t>(pending_int_) * 60000u;
      go(Screen::SetDisplay);
      break;
    case EditField::HoldMinMs:
      p.ec11_hold_min_ms = static_cast<uint32_t>(pending_int_);
      if (p.ec11_hold_max_ms < p.ec11_hold_min_ms) {
        p.ec11_hold_max_ms = p.ec11_hold_min_ms;
      }
      go(Screen::SetDisplay);
      break;
    case EditField::HoldMaxMs:
      p.ec11_hold_max_ms = static_cast<uint32_t>(pending_int_);
      if (p.ec11_hold_max_ms < p.ec11_hold_min_ms) {
        p.ec11_hold_max_ms = p.ec11_hold_min_ms;
      }
      go(Screen::SetDisplay);
      break;
    case EditField::SwitchDebounce:
      p.switch_debounce_ms = static_cast<uint32_t>(pending_int_);
      go(Screen::SetTriggers);
      break;
    case EditField::JoinTimeoutMs:
      p.wifi_sta_timeout_ms = static_cast<uint32_t>(pending_int_);
      go(Screen::SetNetwork);
      break;
    case EditField::InrushSkip:
      p.obstruction_inrush_skip_ms = static_cast<uint32_t>(pending_int_);
      go(Screen::SetObstruction);
      break;
    case EditField::SnugAmpsX100:
      p.snug_amps = pending_int_ / 100.0f;
      go(Screen::SetObstruction);
      break;
    case EditField::TravelCap:
      p.travel_cap_counts = pending_int_;
      go(Screen::SetTravel);
      break;
    case EditField::ProbeTol:
      p.marker_probe_tolerance_counts = pending_int_;
      go(Screen::SetTravel);
      break;
    case EditField::ResyncDrift:
      p.as5600_resync_drift_counts = pending_int_;
      go(Screen::SetTravel);
      break;
    case EditField::Polarity:
      p.trigger_absent_opens = pending_int_ != 0;
      last_key_ = key_->state();
      go(Screen::SetTriggers);
      break;
    default:
      go(Screen::SetMenu);
      break;
  }
  setting_dirty_ = true;
}

void App::saveSettings() {
  if (door.running()) {
    go(Screen::SetBlocked);
    return;
  }
  const bool profile_changed =
      store.pending.pwm_max_duty != store.settings.pwm_max_duty ||
      store.pending.move_accel_ms != store.settings.move_accel_ms ||
      store.pending.move_cruise_ms != store.settings.move_cruise_ms ||
      store.pending.move_decel_ms != store.settings.move_decel_ms;
  store.saveSettings(store.pending);
  if (profile_changed) {
    store.markTablesStale();
    store.saveCal(store.cal);
  }
  setting_dirty_ = false;
  go(Screen::SetMenu);
}

void App::onPress() {
  if (screen_ == Screen::Status) {
    if (door.phase() == DoorPhase::Fault &&
        door.fault() != FaultId::PositionUnknown) {
      door.acknowledgeFault();
      return;
    }
    go(Screen::ModeMenu);
    return;
  }

  auto openEdit = [&](EditField f, int32_t v) {
    edit_field_ = f;
    pending_int_ = v;
    go(Screen::SetEdit);
  };

  switch (screen_) {
    case Screen::ModeMenu:
      if (cursor_ == 0) {
        goStatus();
      } else if (cursor_ == 1) {
        go(Screen::DiagMenu);
      } else if (cursor_ == 2) {
        go(Screen::CalMenu);
      } else if (cursor_ == 3) {
        if (door.running()) {
          go(Screen::SetBlocked);
        } else {
          go(Screen::SetMenu);
        }
      } else {
        go(Screen::Docs);
      }
      break;
    case Screen::DiagMenu: {
      static const Screen kDiag[] = {
          Screen::DiagBottleKey, Screen::DiagShaft,     Screen::DiagLimits,
          Screen::DiagRockers,   Screen::DiagCurrent,   Screen::DiagPwm,
          Screen::DiagJog,       Screen::DiagObstTest,  Screen::DiagTableClose,
          Screen::DiagEc11,      Screen::DiagNetwork,   Screen::DiagCalStatus};
      go(kDiag[cursor_]);
      if (kDiag[cursor_] == Screen::DiagJog) {
        owner_ = MotorOwner::Jog;
      }
      if (kDiag[cursor_] == Screen::DiagObstTest) {
        owner_ = MotorOwner::ObstTest;
      }
      break;
    }
    case Screen::DiagBottleKey:
      if (cursor_ == 0) {
        pulseKey();
      } else if (!door.running()) {
        recaptureKey();
      }
      break;
    case Screen::DiagLimits:
      if (cursor_ == 0) {
        door.setOpenMarkForce(!door.openMarkForced());
      } else {
        door.setCloseMarkForce(!door.closeMarkForced());
      }
      break;
    case Screen::DiagRockers:
      pulseRocker(cursor_ == 0);
      break;
    case Screen::DiagCurrent:
      if (door.idle()) {
        door.captureZero();
        store.cal.current_zero_v = door.zeroVolts();
        store.cal.current_zero_ok = true;
      }
      break;
    case Screen::DiagObstTest:
      if (door.idle()) {
        owner_ = MotorOwner::ObstTest;
        door.requestClose();
      } else {
        door.requestOpen();
      }
      break;
    case Screen::DiagJog:
      door.cancel();
      break;
    case Screen::DiagEc11:
      detents_ = 0;
      rotary_raw_ = 0;
      break;
    case Screen::CalMenu:
      if (cursor_ == 8) {
        go(Screen::CalReview);
      } else {
        static const Screen kCal[] = {
            Screen::CalCurrentZero, Screen::CalDirection, Screen::CalOpenMarker,
            Screen::CalCloseMarker, Screen::CalSeated,    Screen::CalStroke,
            Screen::CalLearnEmpty,  Screen::CalLearnLoaded};
        startStage(kCal[cursor_]);
      }
      break;
    case Screen::CalPathClear:
      path_ok_[cursor_] = !path_ok_[cursor_];
      if (path_ok_[0] && path_ok_[1] && path_ok_[2]) {
        go(cal_target_);
        stage_t0_ = millis();
        if (cal_target_ == Screen::CalDirection ||
            cal_target_ == Screen::CalOpenMarker) {
          door.startCreep(Travel::Open);
          door.holdCreep();
        } else if (cal_target_ == Screen::CalCloseMarker ||
                   cal_target_ == Screen::CalSeated) {
          door.startCreep(Travel::Close);
          door.holdCreep();
        } else if (cal_target_ == Screen::CalLearnEmpty ||
                   cal_target_ == Screen::CalLearnLoaded) {
          memset(bin_n_, 0, sizeof(bin_n_));
          memset(bin_sum_, 0, sizeof(bin_sum_));
          memset(bin_sum2_, 0, sizeof(bin_sum2_));
          memset(bin_peak_, 0, sizeof(bin_peak_));
          learn_run_ = 0;
          learn_dir_ = Travel::Close;
          learn_acc_ = LoadTable{};
          door.requestClose();
        }
      }
      break;
    case Screen::CalCurrentZero:
      door.captureZero();
      store.cal.current_zero_v = door.zeroVolts();
      store.cal.current_zero_ok = true;
      go(Screen::CalJitter);
      stage_t0_ = millis();
      jitter_min_ = door.position();
      jitter_max_ = door.position();
      break;
    case Screen::CalJitter: {
      const int32_t pp = jitter_max_ - jitter_min_;
      store.cal.jitter_pp = pp < 1 ? 1 : pp;
      store.settings.as5600_jitter_counts = store.cal.jitter_pp;
      store.settings.as5600_min_progress_counts =
          store.cal.jitter_pp * 2 + 2;
      store.cal.jitter_ok = true;
      go(Screen::CalBottleKey);
      key_capture_ = 0;
      break;
    }
    case Screen::CalBottleKey:
      recaptureKey();
      if (key_capture_ == 0) {
        go(Screen::CalMenu);
      }
      break;
    case Screen::CalDirection:
      store.cal.direction_ok = true;
      door.cancel();
      go(Screen::CalMenu);
      break;
    case Screen::CalStroke: {
      store.settings.stroke_inches = store.pending.stroke_inches;
      store.cal.stroke_ok = store.settings.stroke_inches > 0.1f;
      store.cal.steps_per_inch = store.derivedStepsPerInch();
      go(Screen::CalMenu);
      break;
    }
    case Screen::CalReview:
      if (door.running()) {
        copy(ignore_, sizeof(ignore_), "motor running");
        break;
      }
      store.saveSettings(store.settings);
      store.saveCal(store.cal);
      go(Screen::CalMenu);
      break;
    case Screen::SetMenu: {
      static const Screen kSet[] = {
          Screen::SetMotion,       Screen::SetTravel, Screen::SetObstruction,
          Screen::SetTriggers,     Screen::SetDisplay, Screen::SetNetwork,
          Screen::SetService};
      if (cursor_ == 6) {
        go(Screen::SetService);
      } else {
        go(kSet[cursor_]);
      }
      break;
    }
    case Screen::SetMotion:
      if (cursor_ == 0) {
        openEdit(EditField::CruiseDuty, store.pending.pwm_max_duty);
      } else if (cursor_ == 1) {
        openEdit(EditField::MinDuty, store.pending.pwm_min_duty);
      } else if (cursor_ == 2) {
        openEdit(EditField::CreepDuty, store.pending.pwm_creep_duty);
      } else if (cursor_ == 3) {
        openEdit(EditField::JogDuty, store.pending.pwm_jog_duty);
      } else if (cursor_ == 4) {
        openEdit(EditField::AccelMs, store.pending.move_accel_ms);
      } else if (cursor_ == 5) {
        openEdit(EditField::CruiseMs, store.pending.move_cruise_ms);
      } else {
        openEdit(EditField::DecelMs, store.pending.move_decel_ms);
      }
      break;
    case Screen::SetTravel:
      if (cursor_ == 0) {
        openEdit(EditField::TravelCap, store.settings.travel_cap_counts);
      } else if (cursor_ == 1) {
        openEdit(EditField::ProbeTol,
                 store.settings.marker_probe_tolerance_counts);
      } else if (cursor_ == 2) {
        openEdit(EditField::ResyncDrift,
                 store.settings.as5600_resync_drift_counts);
      } else {
        go(Screen::SetDerived);
      }
      break;
    case Screen::SetObstruction:
      if (cursor_ == 0) {
        openEdit(EditField::InrushSkip,
                 store.pending.obstruction_inrush_skip_ms);
      } else if (cursor_ == 1) {
        openEdit(EditField::SnugAmpsX100,
                 static_cast<int>(store.pending.snug_amps * 100 + 0.5f));
      } else {
        go(Screen::SetDerived);
      }
      break;
    case Screen::SetTriggers:
      if (cursor_ == 0) {
        openEdit(EditField::Polarity, store.pending.trigger_absent_opens ? 1 : 0);
      } else if (cursor_ == 1) {
        openEdit(EditField::SwitchDebounce, store.pending.switch_debounce_ms);
      }
      break;
    case Screen::SetDisplay:
      if (cursor_ == 0) {
        openEdit(EditField::DisplayTimeoutMin,
                 store.pending.display_timeout_ms / 60000);
      } else if (cursor_ == 1) {
        openEdit(EditField::HoldMinMs,
                 static_cast<int32_t>(store.pending.ec11_hold_min_ms));
      } else {
        openEdit(EditField::HoldMaxMs,
                 static_cast<int32_t>(store.pending.ec11_hold_max_ms));
      }
      break;
    case Screen::SetNetwork:
      openEdit(EditField::JoinTimeoutMs, store.pending.wifi_sta_timeout_ms);
      break;
    case Screen::SetEdit:
      acceptEdit();
      break;
    case Screen::SetConfirm:
      go(Screen::SetReview);
      break;
    case Screen::SetReview:
      saveSettings();
      break;
    case Screen::SetService:
      if (++confirm_hits_ < 2) {
        break;
      }
      confirm_hits_ = 0;
      if (door.running()) {
        go(Screen::SetBlocked);
        break;
      }
      if (cursor_ == 0) {
        store.eraseTables();
      } else if (cursor_ == 1) {
        Settings keep = store.settings;
        const CalRecord c = store.cal;
        store.settings = Settings{};
        store.settings.wifi_sta_ssid[0] = 0;
        strncpy(store.settings.wifi_ap_ssid, keep.wifi_ap_ssid,
                sizeof(store.settings.wifi_ap_ssid) - 1);
        strncpy(store.settings.wifi_sta_ssid, keep.wifi_sta_ssid,
                sizeof(store.settings.wifi_sta_ssid) - 1);
        strncpy(store.settings.wifi_sta_password, keep.wifi_sta_password,
                sizeof(store.settings.wifi_sta_password) - 1);
        strncpy(store.settings.wifi_ap_password, keep.wifi_ap_password,
                sizeof(store.settings.wifi_ap_password) - 1);
        store.saveSettings(store.settings);
        store.cal = c;
      } else if (cursor_ == 2) {
        store.eraseCal();
      } else if (cursor_ == 3) {
        store.clearStaWifi();
        setting_dirty_ = false;
        go(Screen::SetWifiCleared);
        break;
      } else {
        door.cancel();
        ESP.restart();
      }
      go(Screen::SetMenu);
      break;
    case Screen::Docs:
      break;
    case Screen::SetWifiCleared:
      if (door.running()) {
        go(Screen::SetBlocked);
        break;
      }
      door.cancel();
      ESP.restart();
      break;
    default:
      break;
  }
}

void App::finishProbe(bool open_end) {
  if (probe_n_ < 2) {
    return;
  }
  int32_t mn = probe_edge_[0], mx = probe_edge_[0];
  int64_t sum = 0;
  for (uint8_t i = 0; i < probe_n_; ++i) {
    if (probe_edge_[i] < mn) {
      mn = probe_edge_[i];
    }
    if (probe_edge_[i] > mx) {
      mx = probe_edge_[i];
    }
    sum += probe_edge_[i];
  }
  probe_spread_ = mx - mn;
  if (probe_spread_ > store.settings.marker_probe_tolerance_counts) {
    copy(calNote, sizeof(calNote), "spread too wide - repeat");
    probe_n_ = 0;
    return;
  }
  const int32_t mean = static_cast<int32_t>(sum / probe_n_);
  if (open_end) {
    door.setOriginHere();
    store.cal.open_marker_ok = true;
    store.cal.open_spread = probe_spread_;
    (void)mean;
  } else {
    store.cal.close_edge_counts = door.position();
    store.cal.close_marker_ok = true;
    store.cal.close_spread = probe_spread_;
  }
  door.cancel();
  go(Screen::CalMenu);
}

void App::updateLearn() {
  if (screen_ != Screen::CalLearnEmpty && screen_ != Screen::CalLearnLoaded) {
    return;
  }
  if (door.fault() != FaultId::None &&
      door.fault() != FaultId::PositionUnknown) {
    copy(calNote, sizeof(calNote), "trip - run discarded");
    door.cancel();
    go(Screen::CalMenu);
    return;
  }
  if (!door.running() && learn_run_ < 6 && door.idle()) {
    // collect finished one way
    if (learn_run_ > 0 || door.lastEvent() == FaultId::None) {
      // merge this direction into learn_acc_
      LoadBin* dst = learn_dir_ == Travel::Close ? learn_acc_.close
                                                 : learn_acc_.open;
      for (int b = 0; b < kBinCount; ++b) {
        if (bin_n_[b] < 3) {
          continue;
        }
        const float n = static_cast<float>(bin_n_[b]);
        const float mean = bin_sum_[b] / n;
        const float var = bin_sum2_[b] / n - mean * mean;
        const float sig = var > 0 ? sqrtf(var) : 0.02f;
        dst[b].mean = mean;
        dst[b].sigma = sig;
        dst[b].peak = bin_peak_[b];
        float trip = dst[b].peak * 1.20f;
        if (mean + 6 * sig > trip) {
          trip = mean + 6 * sig;
        }
        if (mean + 0.15f > trip) {
          trip = mean + 0.15f;
        }
        dst[b].trip = trip;
        dst[b].valid = true;
      }
    }
    memset(bin_n_, 0, sizeof(bin_n_));
    memset(bin_sum_, 0, sizeof(bin_sum_));
    memset(bin_sum2_, 0, sizeof(bin_sum2_));
    memset(bin_peak_, 0, sizeof(bin_peak_));
    if (learn_run_ >= 6) {
      learn_acc_.runs = 3;
      const bool loaded = screen_ == Screen::CalLearnLoaded;
      store.saveTable(loaded, learn_acc_);
      if (loaded) {
        store.cal.table_loaded_ok = true;
        store.cal.table_loaded_stale = false;
      } else {
        store.cal.table_empty_ok = true;
        store.cal.table_empty_stale = false;
      }
      store.cal.profile_cruise = store.settings.pwm_max_duty;
      store.cal.profile_accel = store.settings.move_accel_ms;
      store.cal.profile_cruise_ms = store.settings.move_cruise_ms;
      store.cal.profile_decel = store.settings.move_decel_ms;
      door.cancel();
      go(Screen::CalMenu);
      return;
    }
    learn_dir_ = (learn_run_ % 2 == 0) ? Travel::Close : Travel::Open;
    learn_run_++;
    if (learn_dir_ == Travel::Close) {
      door.requestClose();
    } else {
      door.requestOpen();
    }
  }
  if (door.running()) {
    const int b = door.binIndex();
    const float a = fabsf(door.amps());
    bin_n_[b]++;
    bin_sum_[b] += a;
    bin_sum2_[b] += a * a;
    if (a > bin_peak_[b]) {
      bin_peak_[b] = a;
    }
  }
}

void App::updateCal() {
  if (screen_ == Screen::CalJitter && door.idle()) {
    door.sampleJitter(jitter_min_, jitter_max_);
  }
  if (screen_ == Screen::CalOpenMarker && door.openMark() && door.running()) {
    if (probe_n_ < 6) {
      probe_edge_[probe_n_++] = door.position();
    }
    door.cancel();
    if (probe_n_ >= 3) {
      finishProbe(true);
    } else {
      door.startCreep(Travel::Open);
    }
  }
  if (screen_ == Screen::CalCloseMarker && door.closeMark() && door.running()) {
    if (probe_n_ < 6) {
      probe_edge_[probe_n_++] = door.position();
    }
    door.cancel();
    if (probe_n_ >= 3) {
      finishProbe(false);
    } else {
      door.startCreep(Travel::Close);
    }
  }
  if (screen_ == Screen::CalSeated && door.running()) {
    if (door.closeMark() && fabsf(door.amps()) >= store.settings.snug_amps) {
      store.cal.seated_counts = door.position();
      copy(store.cal.seated_method, sizeof(store.cal.seated_method), "current");
      store.cal.seated_ok = true;
      door.cancel();
      go(Screen::CalMenu);
    } else if (door.phase() == DoorPhase::Fault &&
               door.fault() == FaultId::MotionStall) {
      store.cal.seated_counts = door.position();
      copy(store.cal.seated_method, sizeof(store.cal.seated_method), "stall");
      store.cal.seated_ok = true;
      door.acknowledgeFault();
      go(Screen::CalMenu);
    }
  }
  if (screen_ == Screen::CalDirection && door.running()) {
    door.holdCreep();
    if (millis() - stage_t0_ > 1200) {
      door.cancel();
    }
  }
  updateLearn();
}

void App::clearDiagInject() {
  sim_open_until_ = 0;
  sim_close_until_ = 0;
  sim_key_on_ = false;
  door.clearMarkForce();
  if (key_) {
    last_key_ = key_->state();
  }
  last_open_rocker_ = open_rocker;
  last_close_rocker_ = close_rocker;
  key_capture_ = 0;
}

void App::pulseRocker(bool open) {
  const uint32_t now = millis();
  uint32_t hold = store.settings.switch_debounce_ms + 50;
  if (hold < 80) {
    hold = 80;
  }
  if (open) {
    sim_open_until_ = now + hold;
  } else {
    sim_close_until_ = now + hold;
  }
}

void App::pulseKey() {
  const BottleKeySensor::State cur = effectiveKey();
  sim_key_on_ = true;
  sim_key_ = cur == BottleKeySensor::State::Present
                 ? BottleKeySensor::State::Absent
                 : BottleKeySensor::State::Present;
}

void App::recaptureKey() {
  if (key_capture_ == 0) {
    key_->captureAbsent();
    key_capture_ = 1;
  } else {
    key_->capturePresent();
    store.settings.bottle_key_absent_adc = key_->absentLevel();
    store.settings.bottle_key_present_adc = key_->presentLevel();
    store.cal.bottle_key_ok = true;
    key_capture_ = 0;
  }
}

BottleKeySensor::State App::effectiveKey() const {
  if (sim_key_on_) {
    return sim_key_;
  }
  return key_->state();
}

void App::pollTriggers() {
  if (owner_ != MotorOwner::Som) {
    return;
  }
  if (somBlocked() && door.fault() != FaultId::None &&
      door.phase() == DoorPhase::Fault) {
    return;
  }
  if (somBlocked()) {
    return;
  }

  const bool open_edge = open_rocker && !last_open_rocker_;
  const bool close_edge = close_rocker && !last_close_rocker_;
  last_open_rocker_ = open_rocker;
  last_close_rocker_ = close_rocker;

  const BottleKeySensor::State ks = effectiveKey();
  bool key_open = false;
  bool key_close = false;
  if (ks != last_key_ && ks != BottleKeySensor::State::Unknown) {
    last_key_edge_ms = millis();
    const bool present = ks == BottleKeySensor::State::Present;
    const bool absent_opens = store.settings.trigger_absent_opens;
    if (absent_opens) {
      key_open = !present;
      key_close = present;
    } else {
      key_open = present;
      key_close = !present;
    }
  }
  last_key_ = ks;

  bool want_open = false;
  bool want_close = false;
  if (open_edge && close_edge) {
    want_open = true;
  } else if (open_edge) {
    want_open = true;
  } else if (close_edge) {
    want_close = true;
  } else if (key_open) {
    want_open = true;
  } else if (key_close) {
    want_close = true;
  }

  if (want_open) {
    if (door.direction() == Travel::Open && door.running()) {
      copy(ignore_, sizeof(ignore_), "already opening");
      return;
    }
    if (door.openMark() && door.idle()) {
      copy(ignore_, sizeof(ignore_), "at limit");
      return;
    }
    door.requestOpen();
  } else if (want_close) {
    if (door.direction() == Travel::Close && door.running()) {
      copy(ignore_, sizeof(ignore_), "already closing");
      return;
    }
    if (door.closeMark() && door.idle() && door.positionTrusted() &&
        door.position() >= store.cal.seated_counts - 30) {
      copy(ignore_, sizeof(ignore_), "at limit");
      return;
    }
    door.requestClose();
  }
}

void App::pollMomentary(bool down, bool& last, uint32_t& edge_ms,
                        uint32_t& down_ms, bool& hold_sent, bool revert_turn) {
  const uint32_t now = millis();
  const uint32_t db = store.settings.switch_debounce_ms;
  uint32_t hold_min = store.settings.ec11_hold_min_ms;
  uint32_t hold_max = store.settings.ec11_hold_max_ms;
  if (hold_min < 150) {
    hold_min = kEc11HoldMinMs;
  }
  if (hold_max < hold_min) {
    hold_max = kEc11HoldMaxMs;
  }

  if (down != last) {
    if (now - edge_ms < db) {
      return;
    }
    last = down;
    edge_ms = now;
    if (down) {
      down_ms = now;
      hold_sent = false;
      if (revert_turn) {
        revertRecentTurn();
      }
    } else if (!hold_sent) {
      const uint32_t held = now - down_ms;
      last_press_ms = held;
      if (revert_turn) {
        revertRecentTurn();
      }
      if (held >= hold_min) {
        handle(Cmd::Hold, 0);
      } else {
        handle(Cmd::Press, 0);
      }
    }
    return;
  }

  edge_ms = now;
  if (down && !hold_sent) {
    const uint32_t held = now - down_ms;
    if (held >= hold_min && held <= hold_max) {
      last_press_ms = held;
      if (revert_turn) {
        revertRecentTurn();
      }
      handle(Cmd::Hold, 0);
      hold_sent = true;
    }
  }
}

void App::pollInputs() {
  const uint32_t now = millis();
  const uint32_t db = store.settings.switch_debounce_ms;
  const bool sw = digitalRead(pins::kRotarySw) ==
                  (kRotarySwitchActiveLow ? LOW : HIGH);
  pollMomentary(sw, last_sw_, sw_change_ms_, sw_down_ms_, sw_hold_sent_, true);
  const bool user = digitalRead(pins::kUserButton) == LOW;
  pollMomentary(user, last_user_, user_change_ms_, user_down_ms_,
                user_hold_sent_, false);

  const bool oraw =
      digitalRead(pins::kOpenButton) == (kTriggerActiveLow ? LOW : HIGH);
  const bool craw =
      digitalRead(pins::kCloseButton) == (kTriggerActiveLow ? LOW : HIGH);
  if (oraw != open_raw_ || craw != close_raw_) {
    if (now - rocker_change_ms_ >= db) {
      if (oraw && !open_rocker) {
        open_edges++;
        last_open_edge_ms = now;
      }
      if (craw && !close_rocker) {
        close_edges++;
        last_close_edge_ms = now;
      }
      open_rocker = oraw;
      close_rocker = craw;
      open_raw_ = oraw;
      close_raw_ = craw;
      rocker_change_ms_ = now;
    }
  } else {
    rocker_change_ms_ = now;
  }
  if (now >= sim_open_until_) {
    open_rocker = open_raw_;
  } else {
    if (!open_rocker) {
      open_edges++;
      last_open_edge_ms = now;
    }
    open_rocker = true;
  }
  if (now >= sim_close_until_) {
    close_rocker = close_raw_;
  } else {
    if (!close_rocker) {
      close_edges++;
      last_close_edge_ms = now;
    }
    close_rocker = true;
  }

  if (effectiveKey() == BottleKeySensor::State::Present) {
    if (key_present_ms == 0) {
      key_present_ms = now;
    }
  } else {
    key_present_ms = 0;
  }
}

void App::pushHist() {
  hall_hist_[hist_head_] = key_->rawAdc();
  const float a = fabsf(door.amps());
  uint16_t ca = static_cast<uint16_t>(a * 100.0f + 0.5f);
  if (ca > 400) {
    ca = 400;
  }
  amp_hist_[hist_head_] = ca;
  hist_head_ = (hist_head_ + 1) % kHistLen;
  if (hist_count_ < kHistLen) {
    hist_count_++;
  }
}

void App::update() {
  const uint32_t now = millis();
  if (now - last_ctrl_ms_ >= kControlLoopMs) {
    last_ctrl_ms_ = now;
    pollInputs();
    pollTriggers();
    updateCal();
    maybeBlank(now);
  }
  if (now - last_hist_ms_ >= 200) {
    last_hist_ms_ = now;
    pushHist();
  }
  sta_up_ = WiFi.status() == WL_CONNECTED;
  ap_up_ = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
  if (sta_up_) {
    snprintf(net_label_, sizeof(net_label_), "STA %s",
             WiFi.localIP().toString().c_str());
  } else if (ap_up_) {
    snprintf(net_label_, sizeof(net_label_), "AP %s",
             store.settings.wifi_ap_ssid);
  } else {
    copy(net_label_, sizeof(net_label_), "net down");
  }
  compose();
}

void App::listItem(uint8_t i, const char* label, const char* value, uint8_t pip,
                   bool sel) {
  if (i >= 12) {
    return;
  }
  copy(view_.items[i].label, sizeof(view_.items[i].label), label);
  copy(view_.items[i].value, sizeof(view_.items[i].value), value);
  view_.items[i].pip = pip;
  view_.items[i].sel = sel;
}

uint8_t App::compactMenu(uint8_t total) {
  uint8_t vis = kMenuVisible;
  if (vis > 12) {
    vis = 12;
  }
  if (total <= vis) {
    view_.nitems = total;
    return 0;
  }
  uint8_t start = 0;
  if (cursor_ >= vis) {
    start = cursor_ - vis + 1;
  }
  if (start + vis > total) {
    start = total - vis;
  }
  for (uint8_t i = 0; i < vis; ++i) {
    view_.items[i] = view_.items[start + i];
  }
  view_.nitems = vis;
  return start;
}

void App::row(uint8_t i, const char* k, const char* v, bool dim) {
  if (i >= 6) {
    return;
  }
  copy(view_.rows[i].k, sizeof(view_.rows[i].k), k);
  copy(view_.rows[i].v, sizeof(view_.rows[i].v), v);
  view_.rows[i].dim = dim ? 1 : 0;
}

void App::ftr2(const char* a, const char* b) {
  copy(view_.ftr[0], sizeof(view_.ftr[0]), a);
  copy(view_.ftr[1], sizeof(view_.ftr[1]), b);
  view_.nftr = 2;
}

void App::ftr3(const char* a, const char* b, const char* c) {
  copy(view_.ftr[0], sizeof(view_.ftr[0]), a);
  copy(view_.ftr[1], sizeof(view_.ftr[1]), b);
  copy(view_.ftr[2], sizeof(view_.ftr[2]), c);
  view_.nftr = 3;
}

void App::pin(const char* parts, const char* lv) {
  copy(view_.pin_parts, sizeof(view_.pin_parts), parts);
  copy(view_.pin_lv, sizeof(view_.pin_lv), lv);
  view_.show_pin = true;
}

const char* App::actionWord() const {
  if (door.phase() == DoorPhase::Fault) {
    return faultName(door.fault());
  }
  if (door.fault() == FaultId::PositionUnknown && door.idle()) {
    return "POSITION UNKNOWN";
  }
  switch (door.phase()) {
    case DoorPhase::Idle:
      if (door.closeMark()) {
        return "CLOSED";
      }
      if (door.openMark()) {
        return "OPEN";
      }
      return "IDLE";
    case DoorPhase::RampUp:
    case DoorPhase::Cruise:
    case DoorPhase::RampDown:
    case DoorPhase::CreepOpen:
      return door.direction() == Travel::Open ? "OPENING" : "CLOSING";
    case DoorPhase::CreepClose:
    case DoorPhase::Snug:
      return "SNUG";
    case DoorPhase::Jog:
      return "JOG";
    case DoorPhase::Calibrating:
      return "CAL";
    default:
      return phaseName(door.phase());
  }
}

char App::actionClass() const {
  if (door.phase() == DoorPhase::Fault ||
      (door.fault() == FaultId::PositionUnknown && door.idle())) {
    return 'f';
  }
  if (door.running()) {
    return 'm';
  }
  return ' ';
}

void App::compose() {
  view_ = View{};
  view_.screen = screen_;
  view_.amps = fabsf(door.amps());
  view_.trip = door.binTrip();
  view_.pos_known = door.positionTrusted();
  view_.counts = door.position();
  view_.open_mark = door.openMark();
  view_.close_mark = door.closeMark();
  view_.moving = door.running();
  view_.fault_knob = door.phase() == DoorPhase::Fault;
  if (view_.pos_known && store.cal.close_edge_counts > 0) {
    int pct = static_cast<int>(
        (100L * door.position()) / store.cal.close_edge_counts);
    if (pct < 0) {
      pct = 0;
    }
    if (pct > 100) {
      pct = 100;
    }
    view_.pos_pct = pct;
  }
  view_.net_ap = !sta_up_ && ap_up_;
  copy(view_.hdr_net, sizeof(view_.hdr_net), net_label_);

  if (screen_ == Screen::Status || screen_ == Screen::ModeMenu) {
    composeStatus();
  } else if (screen_ == Screen::Docs) {
    composeDocs();
  } else if (screen_ >= Screen::DiagMenu && screen_ <= Screen::DiagCalStatus) {
    composeDiag();
  } else if (screen_ >= Screen::CalMenu && screen_ <= Screen::CalReview) {
    composeCal();
  } else {
    composeSet();
  }
}
