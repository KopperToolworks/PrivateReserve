#include "app.h"

#include "config.h"
#include "ec11_calibration.h"

#include <WiFi.h>
#include <cmath>
#include <cstring>

namespace {
void copy(char* dst, size_t n, const char* s) {
  strncpy(dst, s ? s : "", n - 1);
  dst[n - 1] = 0;
}

// Fallback AP PSK. Shown in clear: you need it before the web page exists.
const char* apJoinPassword() {
  return store.settings.wifi_ap_password[0] ? store.settings.wifi_ap_password
                                            : "(open)";
}

uint16_t adcMv(uint16_t adc) {
  return static_cast<uint16_t>((static_cast<uint32_t>(adc) * 3300u) / 4095u);
}

const char* stageWord(bool ok, bool stale, const char* good) {
  if (stale) {
    return "stale";
  }
  return ok ? good : "needed";
}

uint8_t stagePip(bool ok, bool stale) {
  if (stale) {
    return 2;
  }
  return ok ? 1 : 0;
}

const char* gpioLvl(uint8_t pin) {
  return digitalRead(pin) == HIGH ? "HIGH" : "LOW";
}
}  // namespace

void App::composeStatus() {
  const bool setup = door.fault() == FaultId::PositionUnknown || !store.somReady();
  copy(view_.hdr_mode, sizeof(view_.hdr_mode),
       setup ? "NEEDS SETUP" : "MAIN");
  view_.show_travel = true;
  view_.show_amps = door.phase() != DoorPhase::Fault ||
                    door.fault() == FaultId::PositionUnknown;

  if (door.phase() == DoorPhase::Fault ||
      (setup && door.idle() && screen_ == Screen::Status)) {
    view_.hero = View::Hero::Bad;
    copy(view_.hero_big, sizeof(view_.hero_big), faultName(door.fault()));
    if (door.fault() == FaultId::PositionUnknown) {
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           "Automatic motion blocked - run calibration");
    } else {
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           "Press knob to acknowledge");
    }
  } else if (door.lastEvent() == FaultId::LearnedObstructionClose &&
             door.running()) {
    view_.hero = View::Hero::Action;
    copy(view_.hero_big, sizeof(view_.hero_big), "REVERSING");
    view_.hero_cls = 'm';
    copy(view_.hero_sub, sizeof(view_.hero_sub),
         "obstruction while closing - opening");
  } else {
    view_.hero = View::Hero::Action;
    copy(view_.hero_big, sizeof(view_.hero_big), actionWord());
    view_.hero_cls = actionClass();
    snprintf(view_.hero_sub, sizeof(view_.hero_sub), "%s%s",
             phaseName(door.phase()),
             ignore_[0] ? " - " : "");
    if (ignore_[0]) {
      strncat(view_.hero_sub, ignore_, sizeof(view_.hero_sub) - strlen(view_.hero_sub) - 1);
    }
  }

  view_.ntiles = 4;
  view_.tile_cols = 4;
  copy(view_.tiles[0].k, sizeof(view_.tiles[0].k), "Bottle key");
  const bool present = key_->state() == BottleKeySensor::State::Present;
  copy(view_.tiles[0].v, sizeof(view_.tiles[0].v),
       key_->calibrated() ? (present ? "PRES" : "ABS") : "UNCAL");
  view_.tiles[0].pip = present ? 1 : 0;
  snprintf(view_.tiles[0].g, sizeof(view_.tiles[0].g), "adc %u", key_->rawAdc());

  copy(view_.tiles[1].k, sizeof(view_.tiles[1].k), "Open marker");
  copy(view_.tiles[1].v, sizeof(view_.tiles[1].v),
       door.openMark() ? "AT END" : "CLEAR");
  view_.tiles[1].pip = door.openMark() ? 1 : 0;
  copy(view_.tiles[1].g, sizeof(view_.tiles[1].g),
       door.openMark() ? "active" : "travel");

  copy(view_.tiles[2].k, sizeof(view_.tiles[2].k), "Close marker");
  copy(view_.tiles[2].v, sizeof(view_.tiles[2].v),
       door.closeMark() ? "AT END" : "CLEAR");
  view_.tiles[2].pip = door.closeMark() ? 1 : 0;
  copy(view_.tiles[2].g, sizeof(view_.tiles[2].g),
       door.closeMark() ? "active" : "travel");

  copy(view_.tiles[3].k, sizeof(view_.tiles[3].k), "Shaft");
  snprintf(view_.tiles[3].v, sizeof(view_.tiles[3].v), "%.1f",
           fabsf(door.revPerSec()));
  copy(view_.tiles[3].g, sizeof(view_.tiles[3].g),
       fabsf(door.revPerSec()) < 0.05f
           ? "-"
           : (door.revPerSec() < 0 ? "CW" : "CCW"));

  if (screen_ == Screen::ModeMenu) {
    copy(view_.hdr_mode, sizeof(view_.hdr_mode), "MODE");
    view_.hero = View::Hero::None;
    view_.show_travel = false;
    view_.show_amps = false;
    view_.ntiles = 0;
    view_.nitems = 5;
    listItem(0, "Main", "door", 1, cursor_ == 0);
    listItem(1, "Diagnostics", "I/O", 1, cursor_ == 1);
    listItem(2, "Calibration", store.somReady() ? "armed" : "needed",
             store.somReady() ? 1 : 2, cursor_ == 2);
    listItem(3, "Settings", setting_dirty_ ? "unsaved" : "NVS",
             setting_dirty_ ? 2 : 1, cursor_ == 3);
    listItem(4, "Documentation", "GitHub", 1, cursor_ == 4);
    ftr3("TURN scroll", "PRESS open", "HOLD back");
  } else {
    ftr2("TURN menu", "PRESS menu");
  }
}

void App::composeDocs() {
  copy(view_.hdr_mode, sizeof(view_.hdr_mode), "DOC");
  view_.show_qr = true;
  ftr2("HOLD back", "SCAN code");
}

void App::composeDiag() {
  char nbuf[12];
  snprintf(nbuf, sizeof(nbuf), "%u / 13",
           static_cast<unsigned>(screen_) - static_cast<unsigned>(Screen::DiagMenu));

  auto hdr = [&](const char* m) {
    snprintf(view_.hdr_mode, sizeof(view_.hdr_mode), "DIAG - %s", m);
    copy(view_.hdr_net, sizeof(view_.hdr_net), nbuf);
  };

  if (screen_ == Screen::DiagMenu) {
    copy(view_.hdr_mode, sizeof(view_.hdr_mode), "DIAG - menu");
    const char* labels[12] = {
        "Bottle door key - 49E",
        "Motor rotary encoder - AS5600",
        "Limit switches",
        "Override rockers",
        "Motor current load - ACS712",
        "Motor PWM and direction",
        "Jog to position",
        "Obstruction test",
        "Obstruction table",
        "EC11 knob and button",
        "Network and OTA",
        "Stored calibration"};
    char vals[12][16];
    copy(vals[0], 16, key_->present() ? "present" : "absent");
    copy(vals[1], 16, door.magnetOk() ? "ok" : "fault");
    copy(vals[2], 16, door.bothMarks() ? "BOTH" : "clear");
    copy(vals[3], 16, close_rocker ? "close held" : (open_rocker ? "open held" : "off"));
    snprintf(vals[4], 16, "%.2f A", fabsf(door.amps()));
    copy(vals[5], 16, door.idle() ? "idle" : phaseName(door.phase()));
    copy(vals[6], 16, "moves motor");
    copy(vals[7], 16, "moves motor");
    copy(vals[8], 16, store.tablesArmed() ? "armed" : "stale");
    copy(vals[9], 16, "ok");
    copy(vals[10], 16, sta_up_ ? "STA" : "AP");
    copy(vals[11], 16, store.somReady() ? "ok" : "setup");
    uint8_t pips[12] = {1, 0, 0, 1, 1, 1, 2, 2, 1, 1, 1, 1};
    pips[1] = door.magnetOk() ? 1 : 3;
    pips[2] = door.bothMarks() ? 3 : 1;
    pips[8] = store.tablesArmed() ? 1 : 2;
    pips[11] = store.somReady() ? 1 : 2;
    for (uint8_t i = 0; i < 12; ++i) {
      listItem(i, labels[i], vals[i], pips[i], i == cursor_);
    }
    const uint8_t start = compactMenu(12);
    snprintf(view_.hdr_net, sizeof(view_.hdr_net), "%u-%u of 12",
             start + 1, start + view_.nitems);
    ftr3("TURN scroll", "PRESS open", "HOLD exit");
    return;
  }

  char lv[44];
  char tmp[48];
  char tmp2[48];

  switch (screen_) {
    case Screen::DiagBottleKey: {
      hdr("bottle key");
      view_.hero = View::Hero::Action;
      const BottleKeySensor::State ks = effectiveKey();
      const bool present = ks == BottleKeySensor::State::Present;
      if (sim_key_on_) {
        copy(view_.hero_big, sizeof(view_.hero_big),
             present ? "PRESENT" : "ABSENT");
        view_.hero_cls = 'm';
      } else if (!key_->calibrated()) {
        copy(view_.hero_big, sizeof(view_.hero_big), "UNCAL");
        view_.hero_cls = 'm';
      } else {
        copy(view_.hero_big, sizeof(view_.hero_big),
             present ? "PRESENT" : "ABSENT");
        view_.hero_cls = 'k';
      }
      view_.spark = 1;
      if (cursor_ == 1 && key_capture_ == 1) {
        copy(view_.hero_sub, sizeof(view_.hero_sub),
             "recapture 2 of 2 - bottle IN");
      } else {
        snprintf(view_.hero_sub, sizeof(view_.hero_sub),
                 "%s%sadc %u - last edge %lu ms",
                 sim_key_on_ ? "SIM - " : "",
                 cursor_ == 0 ? "pulse - " : "recapture - ",
                 key_->rawAdc(),
                 static_cast<unsigned long>(millis() - last_key_edge_ms));
      }
      snprintf(tmp, sizeof(tmp), "%u", key_->absentLevel());
      snprintf(tmp2, sizeof(tmp2), "%u", key_->presentLevel());
      row(0, "absent level (calibrated)", tmp);
      row(1, "present level (calibrated)", tmp2);
      snprintf(tmp, sizeof(tmp), "%u / %u", key_->enterThreshold(),
               key_->exitThreshold());
      row(2, "enter / exit threshold", tmp);
      row(3, "trigger polarity",
          store.settings.trigger_absent_opens ? "absent -> OPEN"
                                              : "present -> OPEN");
      view_.nrows = 4;
      snprintf(lv, sizeof(lv), "%u / 4095 - %.2f V", key_->rawAdc(),
               adcMv(key_->rawAdc()) / 1000.0f);
      pin("GPIO01 - T23 - ADC1_CH0 - 49E OUT", lv);
      if (cursor_ == 0) {
        ftr3("TURN action", "PRESS pulse", "HOLD back");
      } else if (key_capture_ == 1) {
        ftr3("TURN action", "PRESS capture IN", "HOLD back");
      } else {
        ftr3("TURN action", "PRESS recapture", "HOLD back");
      }
      break;
    }

    case Screen::DiagShaft: {
      hdr("shaft encoder");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%ld",
               static_cast<long>(door.position()));
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "%d%% of span - raw %u",
               view_.pos_pct, door.rawAngle());
      snprintf(view_.hero_right, sizeof(view_.hero_right), "%.1f deg",
               door.rawAngle() * (360.0f / 4096.0f));
      snprintf(view_.hero_right2, sizeof(view_.hero_right2), "%.1f rev/s",
               door.revPerSec());
      snprintf(tmp, sizeof(tmp), "%u - %s", door.rawAngle(),
               door.magnetOk() ? "OK" : "BAD");
      row(0, "raw angle - magnet", tmp);
      snprintf(tmp, sizeof(tmp), "%u - %s", 0, door.magnetOk() ? "detected" : "lost");
      row(1, "status", door.magnetOk() ? "detected" : "fault", !door.magnetOk());
      snprintf(tmp, sizeof(tmp), "+/- %ld cnt",
               static_cast<long>(store.settings.as5600_jitter_counts));
      row(2, "jitter band at rest", tmp);
      snprintf(tmp, sizeof(tmp), "%ld / %ld cnt",
               static_cast<long>(store.cal.drift_open),
               static_cast<long>(store.cal.drift_close));
      row(3, "drift last re-sync (open / close)", tmp);
      view_.nrows = 4;
      pin("GPIO43/44 - T10/T9 - I2C 0x36 - 12-bit", "4096 cnt/rev - 100 kHz");
      ftr2("PRESS zero log", "HOLD back");
      break;
    }

    case Screen::DiagLimits:
      hdr("limit markers");
      view_.ntiles = 2;
      view_.tile_cols = 2;
      copy(view_.tiles[0].k, sizeof(view_.tiles[0].k), "Open marker - GPIO17 - T7");
      copy(view_.tiles[0].v, sizeof(view_.tiles[0].v),
           door.openMark() ? "ACTIVE" : "CLEAR");
      view_.tiles[0].pip =
          door.openMarkForced() ? 2 : (door.openMark() ? 1 : 0);
      view_.tiles[0].sel = cursor_ == 0;
      if (door.openMarkForced()) {
        snprintf(view_.tiles[0].g, sizeof(view_.tiles[0].g), "SIM - pin %s",
                 gpioLvl(pins::kLimitUpper));
      } else {
        copy(view_.tiles[0].g, sizeof(view_.tiles[0].g),
             gpioLvl(pins::kLimitUpper));
      }
      copy(view_.tiles[1].k, sizeof(view_.tiles[1].k), "Close marker - GPIO03 - T21");
      copy(view_.tiles[1].v, sizeof(view_.tiles[1].v),
           door.closeMark() ? "ACTIVE" : "CLEAR");
      view_.tiles[1].pip =
          door.closeMarkForced() ? 2 : (door.closeMark() ? 1 : 0);
      view_.tiles[1].sel = cursor_ == 1;
      if (door.closeMarkForced()) {
        snprintf(view_.tiles[1].g, sizeof(view_.tiles[1].g), "SIM - pin %s",
                 gpioLvl(pins::kLimitLower));
      } else {
        copy(view_.tiles[1].g, sizeof(view_.tiles[1].g),
             gpioLvl(pins::kLimitLower));
      }
      row(0, "polarity", "HIGH = marker or cut wire");
      row(1, "both active check", door.bothMarks() ? "FAIL" : "pass",
          door.bothMarks());
      snprintf(tmp, sizeof(tmp), "%ld cnt", static_cast<long>(door.position()));
      row(2, "position now", tmp);
      snprintf(tmp, sizeof(tmp), "%ld cnt",
               static_cast<long>(store.settings.travel_cap_counts));
      row(3, "overtravel cap past marker", tmp);
      view_.nrows = 4;
      snprintf(lv, sizeof(lv), "17: %s - 03: %s", gpioLvl(pins::kLimitUpper),
               gpioLvl(pins::kLimitLower));
      pin("NC -> GND - INPUT_PULLUP - HIGH=marker", lv);
      ftr3("TURN select", "PRESS force", "HOLD back");
      break;

    case Screen::DiagRockers: {
      hdr("override rockers");
      const bool sim_o = millis() < sim_open_until_;
      const bool sim_c = millis() < sim_close_until_;
      view_.ntiles = 2;
      view_.tile_cols = 2;
      copy(view_.tiles[0].k, sizeof(view_.tiles[0].k), "Open throw - GPIO10 - T20");
      copy(view_.tiles[0].v, sizeof(view_.tiles[0].v), open_rocker ? "MADE" : "OFF");
      view_.tiles[0].pip = sim_o ? 2 : (open_rocker ? 1 : 0);
      view_.tiles[0].sel = cursor_ == 0;
      if (sim_o) {
        snprintf(view_.tiles[0].g, sizeof(view_.tiles[0].g), "SIM - edges %u",
                 open_edges);
      } else {
        snprintf(view_.tiles[0].g, sizeof(view_.tiles[0].g), "edges %u",
                 open_edges);
      }
      copy(view_.tiles[1].k, sizeof(view_.tiles[1].k), "Close throw - GPIO18 - T8");
      copy(view_.tiles[1].v, sizeof(view_.tiles[1].v), close_rocker ? "MADE" : "OFF");
      view_.tiles[1].pip = sim_c ? 2 : (close_rocker ? 1 : 0);
      view_.tiles[1].sel = cursor_ == 1;
      if (sim_c) {
        snprintf(view_.tiles[1].g, sizeof(view_.tiles[1].g), "SIM - edges %u",
                 close_edges);
      } else {
        snprintf(view_.tiles[1].g, sizeof(view_.tiles[1].g), "edges %u",
                 close_edges);
      }
      row(0, "command model", "edge trigger - centre does nothing");
      row(1, "both throws made", "tie-break: OPEN wins");
      snprintf(tmp, sizeof(tmp), "%lu ms",
               static_cast<unsigned long>(store.settings.switch_debounce_ms));
      row(2, "debounce", tmp);
      row(3, "last ignored", ignore_[0] ? ignore_ : "-");
      view_.nrows = 4;
      snprintf(lv, sizeof(lv), "10: %s - 18: %s", gpioLvl(pins::kOpenButton),
               gpioLvl(pins::kCloseButton));
      pin("active-low -> GND - INPUT_PULLUP - 2 stations", lv);
      ftr3("TURN select", "PRESS pulse", "HOLD back");
      break;
    }

    case Screen::DiagCurrent:
      hdr("current load");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%.2f", fabsf(door.amps()));
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "%s - bin %d of 81",
               phaseName(door.phase()), door.binIndex());
      view_.spark = 2;
      {
        const LoadBin* b = door.binNow();
        if (b && b->valid) {
          snprintf(tmp, sizeof(tmp), "%.2f / %.2f A", b->mean, b->peak);
          snprintf(tmp2, sizeof(tmp2), "%.2f A - 2.50 A", b->trip);
        } else {
          copy(tmp, sizeof(tmp), "- / -");
          copy(tmp2, sizeof(tmp2), "ceiling 2.50 A");
        }
      }
      row(0, "bin envelope mean / peak", tmp);
      row(1, "bin trip - hard ceiling", tmp2);
      snprintf(tmp, sizeof(tmp), "%lu - %lu ms",
               static_cast<unsigned long>(kOvercurrentHoldMs),
               static_cast<unsigned long>(store.settings.obstruction_inrush_skip_ms));
      row(2, "trip hold - inrush skip", tmp);
      snprintf(tmp, sizeof(tmp), "%.3f V - 185 mV/A", door.zeroVolts());
      row(3, "zero - scale", tmp);
      view_.nrows = 4;
      snprintf(lv, sizeof(lv), "%u / 4095 - %.2f V", door.currentAdc(),
               door.currentAdc() * 3.3f / 4095.0f);
      pin("GPIO02 - T22 - ADC1_CH1 - 10k/20k divider", lv);
      ftr2("PRESS recapture zero", "HOLD back");
      break;

    case Screen::DiagPwm:
      hdr("pwm - direction");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%u", door.duty());
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "/ 1023 - 15 kHz - 10-bit");
      snprintf(view_.hero_right, sizeof(view_.hero_right), "K1 %s",
               door.k1Open() ? "OPEN" : "CLOSE");
      snprintf(tmp, sizeof(tmp), "%s - %lu ms", phaseName(door.phase()),
               static_cast<unsigned long>(door.phaseElapsedMs()));
      row(0, "phase", tmp);
      snprintf(tmp, sizeof(tmp), "%lu / %lu ms",
               static_cast<unsigned long>(store.settings.pwm_blank_before_dir_ms),
               static_cast<unsigned long>(store.settings.pwm_blank_after_dir_ms));
      row(1, "blank before / after direction", tmp);
      snprintf(tmp, sizeof(tmp), "%lu ms",
               static_cast<unsigned long>(store.settings.relay_settle_ms));
      row(2, "relay settle", tmp);
      snprintf(tmp, sizeof(tmp), "%.1f rev/s - %ld cnt", door.revPerSec(),
               static_cast<long>(door.position()));
      row(3, "shaft (supporting)", tmp);
      view_.nrows = 4;
      snprintf(lv, sizeof(lv), "16: PWM - 21: %s", door.k1Open() ? "HIGH" : "LOW");
      pin("GPIO16 -> PWM -> 6N137 - GPIO21 -> DIR", lv);
      view_.nftr = 1;
      copy(view_.ftr[0], sizeof(view_.ftr[0]), "HOLD back");
      break;

    case Screen::DiagJog:
      hdr("jog");
      view_.hero = View::Hero::Warn;
      copy(view_.hero_big, sizeof(view_.hero_big), "MOTOR LIVE - JOG");
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           "EC11 only - opposite blanks before reverse");
      view_.show_travel = true;
      {
        const int pct = static_cast<int>(
            (static_cast<uint32_t>(store.settings.pwm_jog_duty) * 100u +
             (kPwmMaxDuty / 2u)) /
            kPwmMaxDuty);
        snprintf(tmp, sizeof(tmp), "%d %% - %s", pct,
                 door.k1Open() ? "OPEN" : "CLOSE");
        row(0, "jog duty - direction", tmp, false, cursor_ == 1);
      }
      snprintf(tmp, sizeof(tmp), "%lu ms - %lu ms",
               static_cast<unsigned long>(store.settings.jog_step_ms),
               static_cast<unsigned long>(door.jogRemainingMs()));
      row(1, "step - remaining", tmp, false, cursor_ == 2);
      view_.nrows = 2;
      view_.spark = 2;
      if (door.magnetOk()) {
        snprintf(lv, sizeof(lv), "%ld cnt", static_cast<long>(door.position()));
      } else {
        copy(lv, sizeof(lv), "enc --");
      }
      pin("GPIO16 - GPIO21 - T5 / T6", lv);
      if (cursor_ == 1) {
        ftr3("TURN duty 5-25%", "PRESS next", "HOLD exit");
      } else if (cursor_ == 2) {
        ftr3("TURN step 200-1000", "PRESS next", "HOLD exit");
      } else if (door.idle()) {
        ftr3("TURN jog", "PRESS edit", "HOLD exit");
      } else {
        ftr3("TURN jog", "PRESS stop", "HOLD exit");
      }
      break;

    case Screen::DiagObstTest:
      hdr("obstruction test");
      if (door.lastEvent() == FaultId::LearnedObstructionClose ||
          door.lastEvent() == FaultId::ObstructionWhileOpening ||
          door.lastEvent() == FaultId::RepeatedObstructionClose) {
        view_.hero = View::Hero::Warn;
        copy(view_.hero_big, sizeof(view_.hero_big), "TRIPPED");
        snprintf(view_.hero_sub, sizeof(view_.hero_sub), "%s - %.2f A vs trip %.2f",
                 faultName(door.lastEvent()), fabsf(door.amps()), door.binTrip());
      } else if (door.running()) {
        view_.hero = View::Hero::Warn;
        copy(view_.hero_big, sizeof(view_.hero_big), "MOTOR LIVE - TEST");
        copy(view_.hero_sub, sizeof(view_.hero_sub),
             "plastic bottle between doors");
      } else {
        view_.hero = View::Hero::Action;
        copy(view_.hero_big, sizeof(view_.hero_big), "READY");
        copy(view_.hero_sub, sizeof(view_.hero_sub),
             "close first - table writes disabled");
      }
      row(0, "test load", "plastic bottle between doors");
      row(1, "expected close / open", "reverse once / fault first trip");
      row(2, "table writes - hard ceiling", "disabled - 2.50 A armed");
      row(3, "result", faultName(door.lastEvent()));
      view_.nrows = 4;
      snprintf(lv, sizeof(lv), "%.2f A - %s", fabsf(door.amps()),
               phaseName(door.phase()));
      pin("GPIO02 current - GPIO16 pwm - GPIO21 dir", lv);
      ftr2("PRESS run", "HOLD exit");
      break;

    case Screen::DiagTableClose:
    case Screen::DiagTableOpen:
      hdr(screen_ == Screen::DiagTableClose ? "table - close" : "table - open");
      view_.graph = screen_ == Screen::DiagTableClose ? 1 : 2;
      view_.live_bin = static_cast<uint8_t>(door.binIndex());
      {
        const LoadTable* t = store.armedTable();
        const LoadBin* bins =
            t ? (view_.graph == 1 ? t->close : t->open) : nullptr;
        int valid = 0;
        float worst_trip = 0;
        int worst_b = 0;
        if (bins) {
          for (int b = 0; b < kBinCount; ++b) {
            if (bins[b].valid) {
              valid++;
              if (bins[b].trip > worst_trip) {
                worst_trip = bins[b].trip;
                worst_b = b;
              }
            }
          }
        }
        snprintf(tmp, sizeof(tmp), "%d of 81 - %u", valid,
                 t ? t->runs : 0);
        row(0, "valid bins - runs merged", tmp);
        snprintf(tmp, sizeof(tmp), "%.2f A at bin %d - %.2f A", worst_trip,
                 worst_b, 2.50f - worst_trip);
        row(1, "worst trip - margin to 2.50 A", tmp);
        view_.nrows = 2;
      }
      snprintf(lv, sizeof(lv), "%.2f A - bin %d", fabsf(door.amps()),
               door.binIndex());
      pin("GPIO02 live - empty-rack table - 81 bins", lv);
      ftr3("TURN direction", "PRESS bin detail", "HOLD back");
      break;

    case Screen::DiagEc11:
      hdr("ec11");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%+ld",
               static_cast<long>(detents_));
      copy(view_.hero_sub, sizeof(view_.hero_sub), "detents since reset");
      copy(view_.hero_right, sizeof(view_.hero_right),
           digitalRead(pins::kRotarySw) ? "SW released" : "SW down");
      snprintf(tmp, sizeof(tmp), "%ld counts", static_cast<long>(rotary_raw_));
      row(0, "raw quadrature", tmp);
      snprintf(tmp, sizeof(tmp), "%ld / %ld",
               static_cast<long>(kEc11DetentScaleNumerator),
               static_cast<long>(kEc11DetentScaleDenominator));
      row(1, "detent scale", tmp);
      snprintf(tmp, sizeof(tmp), "%lu ms",
               static_cast<unsigned long>(store.settings.switch_debounce_ms));
      row(2, "debounce", tmp);
      snprintf(tmp, sizeof(tmp), "%lu-%lu ms - last %lu ms",
               static_cast<unsigned long>(store.settings.ec11_hold_min_ms),
               static_cast<unsigned long>(store.settings.ec11_hold_max_ms),
               static_cast<unsigned long>(last_press_ms));
      row(3, "hold window - last down", tmp);
      view_.nrows = 4;
      pin("GPIO11 CLK - GPIO12 DT - GPIO13 SW - on-board", "on-board header");
      ftr2("PRESS zero count", "HOLD back");
      break;

    case Screen::DiagNetwork:
      hdr("network");
      view_.hero = View::Hero::Action;
      copy(view_.hero_big, sizeof(view_.hero_big),
           sta_up_ ? WiFi.localIP().toString().c_str()
                   : store.settings.wifi_ap_ssid);
      view_.hero_cls = sta_up_ ? 'k' : 'm';
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "%s - %s",
               sta_up_ ? "STA" : "SoftAP",
               sta_up_ ? store.settings.wifi_sta_ssid : "192.168.4.1");
      row(0, "mDNS", "private-reserve.local");
      if (sta_up_) {
        row(1, "fallback AP - web clients", store.settings.wifi_ap_ssid);
      } else {
        row(1, "join password", apJoinPassword());
      }
      row(2, "OTA", door.idle() ? "idle only - armed" : "blocked - motor busy");
      row(3, "antenna", "onboard - lid must be open");
      view_.nrows = 4;
      pin("onboard radio - no external SMA", sta_up_ ? "STA joined" : "AP up");
      ftr2("PRESS rejoin", "HOLD back");
      break;

    case Screen::DiagCalStatus:
      hdr("calibration");
      view_.nitems = 7;
      listItem(0, "Direction - origin - span",
               store.cal.open_marker_ok && store.cal.close_marker_ok ? "ok"
                                                                     : "needed",
               stagePip(store.cal.open_marker_ok && store.cal.close_marker_ok,
                        false),
               cursor_ == 0);
      snprintf(tmp, sizeof(tmp), "+/- %ld cnt",
               static_cast<long>(store.cal.open_spread));
      listItem(1, "Marker edges (both)", tmp,
               stagePip(store.cal.open_marker_ok, false), false);
      listItem(2, "Closed seated",
               store.cal.seated_ok ? store.cal.seated_method : "needed",
               stagePip(store.cal.seated_ok, false), false);
      snprintf(tmp, sizeof(tmp), "%ld cnt/in",
               static_cast<long>(store.derivedStepsPerInch()));
      listItem(3, "Stroke", store.cal.stroke_ok ? tmp : "needed",
               stagePip(store.cal.stroke_ok, false), false);
      listItem(4, "Obstruction table - empty racks",
               stageWord(store.cal.table_empty_ok, store.cal.table_empty_stale,
                         "armed"),
               stagePip(store.cal.table_empty_ok, store.cal.table_empty_stale),
               false);
      listItem(5, "Obstruction table - loaded racks",
               stageWord(store.cal.table_loaded_ok, store.cal.table_loaded_stale,
                         "armed"),
               stagePip(store.cal.table_loaded_ok, store.cal.table_loaded_stale),
               false);
      listItem(6, "NVS format v3", "blob", 1, false);
      compactMenu(7);
      pin("NVS - format v3", store.somReady() ? "SOM ready" : "needs setup");
      ftr2("PRESS open", "HOLD back");
      break;

    default:
      break;
  }
}

void App::composeCal() {
  auto hdr = [&](const char* m, const char* r) {
    snprintf(view_.hdr_mode, sizeof(view_.hdr_mode), "CAL - %s", m);
    copy(view_.hdr_net, sizeof(view_.hdr_net), r);
  };

  if (screen_ == Screen::CalMenu) {
    hdr("stages", "9 items");
    listItem(0, "1 - Baselines (current, shaft, key)",
             stageWord(store.cal.current_zero_ok && store.cal.jitter_ok &&
                           store.cal.bottle_key_ok,
                       false, "done"),
             stagePip(store.cal.current_zero_ok && store.cal.jitter_ok &&
                          store.cal.bottle_key_ok,
                      false),
             cursor_ == 0);
    listItem(1, "2 - Direction verify",
             stageWord(store.cal.direction_ok, false, "match"),
             stagePip(store.cal.direction_ok, false), cursor_ == 1);
    snprintf(view_.items[2].value, sizeof(view_.items[2].value), "+/- %ld cnt",
             static_cast<long>(store.cal.open_spread));
    listItem(2, "3 - Open marker -> origin",
             store.cal.open_marker_ok ? view_.items[2].value : "needed",
             stagePip(store.cal.open_marker_ok, false), cursor_ == 2);
    snprintf(view_.items[3].value, sizeof(view_.items[3].value), "+/- %ld cnt",
             static_cast<long>(store.cal.close_spread));
    listItem(3, "4 - Close marker -> span",
             store.cal.close_marker_ok ? view_.items[3].value : "needed",
             stagePip(store.cal.close_marker_ok, false), cursor_ == 3);
    listItem(4, "5 - Closed seated",
             store.cal.seated_ok ? store.cal.seated_method : "needed",
             stagePip(store.cal.seated_ok, false), cursor_ == 4);
    listItem(5, "6 - Stroke entry",
             stageWord(store.cal.stroke_ok, false, "ok"),
             stagePip(store.cal.stroke_ok, false), cursor_ == 5);
    listItem(6, "7 - Obstruction table - empty",
             stageWord(store.cal.table_empty_ok, store.cal.table_empty_stale,
                       "armed"),
             stagePip(store.cal.table_empty_ok, store.cal.table_empty_stale),
             cursor_ == 6);
    listItem(7, "8 - Obstruction table - loaded",
             stageWord(store.cal.table_loaded_ok, store.cal.table_loaded_stale,
                       "optional"),
             stagePip(store.cal.table_loaded_ok, store.cal.table_loaded_stale),
             cursor_ == 7);
    listItem(8, "Review and save", door.idle() ? "ready" : "motor busy", 1,
             cursor_ == 8);
    const uint8_t start = compactMenu(9);
    snprintf(view_.hdr_net, sizeof(view_.hdr_net), "%u-%u of 9", start + 1,
             start + view_.nitems);
    ftr3("TURN scroll", "PRESS open", "HOLD exit");
    return;
  }

  char tmp[48];
  char tmp2[48];

  if (screen_ == Screen::CalPathClear) {
    hdr("confirm", "path clear");
    view_.hero = View::Hero::Warn;
    copy(view_.hero_big, sizeof(view_.hero_big), "THE DOOR WILL MOVE");
    copy(view_.hero_sub, sizeof(view_.hero_sub),
         "140 V motor - no physical E-stop installed");
    view_.nitems = 3;
    listItem(0, "Travel path clear of tools and people", "",
             path_ok_[0] ? 1 : 0, cursor_ == 0);
    listItem(1, "Both marker switches wired and reading LOW", "",
             path_ok_[1] ? 1 : 0, cursor_ == 1);
    listItem(2, "Racks empty for this stage", "", path_ok_[2] ? 1 : 0,
             cursor_ == 2);
    ftr3("TURN tick", "PRESS start", "HOLD cancel");
    return;
  }

  switch (screen_) {
    case Screen::CalCurrentZero:
      hdr("baseline - current", "1a of 8");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%.3f", door.zeroVolts());
      view_.hero_cls = 'k';
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           "16 samples at PWM 0");
      snprintf(tmp, sizeof(tmp), "%u / 4095 - 0.00 A", door.currentAdc());
      row(0, "raw adc mean - implies", tmp);
      row(1, "scale (fixed)", "185 mV/A - 10k/20k");
      snprintf(tmp, sizeof(tmp), "%.3f V", store.cal.current_zero_v);
      row(2, "previous capture", tmp, true);
      view_.nrows = 3;
      view_.show_prog = true;
      view_.prog[0] = 2;
      copy(view_.prog_l, sizeof(view_.prog_l), "baselines");
      copy(view_.prog_r, sizeof(view_.prog_r), "1 of 3");
      ftr2("PRESS accept", "HOLD cancel");
      break;

    case Screen::CalJitter:
      hdr("baseline - shaft", "1b of 8");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "+/- %ld",
               static_cast<long>(jitter_max_ - jitter_min_));
      copy(view_.hero_sub, sizeof(view_.hero_sub), "peak-to-peak at rest");
      view_.hero_cls = 'k';
      row(0, "samples - magnet", door.magnetOk() ? "OK" : "BAD");
      snprintf(tmp, sizeof(tmp), "%ld cnt / 500 ms",
               static_cast<long>(store.settings.as5600_min_progress_counts));
      row(1, "sets stall minimum progress", tmp);
      snprintf(tmp, sizeof(tmp), "%ld cnt",
               static_cast<long>(store.settings.as5600_jitter_counts));
      row(2, "sets accumulation deadband", tmp);
      view_.nrows = 3;
      view_.show_prog = true;
      view_.prog[0] = 1;
      view_.prog[1] = 2;
      copy(view_.prog_l, sizeof(view_.prog_l), "baselines");
      copy(view_.prog_r, sizeof(view_.prog_r), "2 of 3");
      ftr2("PRESS accept", "HOLD cancel");
      break;

    case Screen::CalBottleKey:
      hdr("baseline - key", "1c of 8");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%u", key_->rawAdc());
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           key_capture_ == 0 ? "capture 1 of 2 - bottle OUT"
                             : "capture 2 of 2 - bottle IN");
      snprintf(view_.hero_right, sizeof(view_.hero_right), "absent %u",
               key_->absentLevel());
      snprintf(tmp, sizeof(tmp), "%d counts",
               static_cast<int>(key_->presentLevel()) -
                   static_cast<int>(key_->absentLevel()));
      row(0, "gap between levels", tmp);
      snprintf(tmp, sizeof(tmp), "%u / %u", key_->enterThreshold(),
               key_->exitThreshold());
      row(1, "derived enter / exit", tmp);
      row(2, "polarity for this build",
          store.settings.trigger_absent_opens ? "absent -> OPEN"
                                              : "present -> OPEN");
      view_.nrows = 3;
      view_.show_prog = true;
      view_.prog[0] = 1;
      view_.prog[1] = 1;
      view_.prog[2] = 2;
      copy(view_.prog_l, sizeof(view_.prog_l), "baselines");
      copy(view_.prog_r, sizeof(view_.prog_r), "3 of 3");
      ftr2("PRESS accept", "HOLD cancel");
      break;

    case Screen::CalDirection:
      hdr("direction verify", "2 of 8");
      view_.hero = View::Hero::Action;
      copy(view_.hero_big, sizeof(view_.hero_big),
           door.running() ? "CREEP OPEN" : "STOPPED");
      view_.hero_cls = 'm';
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "AS5600 delta %ld cnt",
               static_cast<long>(door.unwrapDelta()));
      row(0, "commanded", "OPEN");
      row(1, "invert flag", store.settings.direction_invert ? "ON" : "OFF");
      row(2, "K1", door.k1Open() ? "OPEN" : "CLOSE");
      view_.nrows = 3;
      ftr2("PRESS match", "HOLD cancel");
      break;

    case Screen::CalOpenMarker:
      hdr("open marker", "3 of 8");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "probe %u/3", probe_n_);
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "spread %ld cnt - origin 0",
               static_cast<long>(probe_spread_));
      row(0, "accepted edge becomes", "count 0");
      row(1, "mark the door here", "stroke reference");
      row(2, "note", calNote);
      view_.nrows = 3;
      ftr2("auto probe", "HOLD cancel");
      break;

    case Screen::CalCloseMarker:
      hdr("close marker", "4 of 8");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "probe %u/3", probe_n_);
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "spread %ld - pos %ld",
               static_cast<long>(probe_spread_),
               static_cast<long>(door.position()));
      row(0, "span will be", "close edge - origin");
      row(1, "bins divide this span", "81 bins");
      row(2, "note", calNote);
      view_.nrows = 3;
      ftr2("auto probe", "HOLD cancel");
      break;

    case Screen::CalSeated:
      hdr("closed seated", "5 of 8");
      view_.hero = View::Hero::Action;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "%ld",
               static_cast<long>(door.position()));
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           "past close marker - watching current");
      snprintf(tmp, sizeof(tmp), "%.2f A  snug %.2f", fabsf(door.amps()),
               store.settings.snug_amps);
      row(0, "current", tmp);
      row(1, "method if accepted", "current rise or stall");
      row(2, "not a home", "seal can move");
      view_.nrows = 3;
      ftr2("auto capture", "HOLD cancel");
      break;

    case Screen::CalStroke: {
      hdr("stroke entry", "6 of 8");
      view_.show_entry = true;
      view_.show_range = true;
      snprintf(view_.entry_val, sizeof(view_.entry_val), "%.1f",
               store.pending.stroke_inches > 0.1f ? store.pending.stroke_inches
                                                  : 32.0f);
      if (store.pending.stroke_inches < 0.1f) {
        store.pending.stroke_inches = 32.0f;
      }
      copy(view_.entry_unit, sizeof(view_.entry_unit), "inches");
      copy(view_.entry_note, sizeof(view_.entry_note),
           "open marker to closed seated");
      view_.range_val = static_cast<int32_t>(store.pending.stroke_inches * 10);
      view_.range_min = 100;
      view_.range_max = 480;
      view_.range_def = 320;
      const int32_t spi = store.cal.seated_counts > 0
                              ? static_cast<int32_t>(
                                    store.cal.seated_counts /
                                        store.pending.stroke_inches +
                                    0.5f)
                              : 0;
      snprintf(tmp, sizeof(tmp), "%ld  (geom ~640)", static_cast<long>(spi));
      row(0, "derived counts/in", tmp);
      row(1, "plausibility", spi > 0 && (spi < 400 || spi > 900)
                                 ? "WARN - check mount"
                                 : "ok");
      row(2, "never widens a limit", "display only", true);
      view_.nrows = 3;
      ftr3("TURN adjust", "PRESS accept", "HOLD cancel");
      break;
    }

    case Screen::CalLearnEmpty:
    case Screen::CalLearnLoaded:
      hdr(screen_ == Screen::CalLearnEmpty ? "learn - empty"
                                           : "learn - loaded",
          screen_ == Screen::CalLearnEmpty ? "7 of 8" : "8 of 8");
      view_.hero = View::Hero::Warn;
      snprintf(view_.hero_big, sizeof(view_.hero_big), "RUN %u / 6", learn_run_);
      snprintf(view_.hero_sub, sizeof(view_.hero_sub), "%s - reverse OFF",
               learn_dir_ == Travel::Close ? "closing" : "opening");
      view_.show_travel = true;
      row(0, "table writes", "after accepted set only");
      row(1, "obstruction", "discards this run");
      row(2, "note", calNote);
      view_.nrows = 3;
      ftr2("auto", "HOLD cancel");
      break;

    case Screen::CalReview:
      hdr("review", "save");
      view_.hero = View::Hero::Ok;
      copy(view_.hero_big, sizeof(view_.hero_big),
           door.idle() ? "READY TO SAVE" : "MOTOR BUSY");
      copy(view_.hero_sub, sizeof(view_.hero_sub),
           door.idle() ? "motor idle - current decayed" : "wait for idle");
      view_.nitems = 6;
      listItem(0, "Baselines",
               store.cal.current_zero_ok ? "ok" : "missing",
               stagePip(store.cal.current_zero_ok, false), false);
      listItem(1, "Direction - markers - seated",
               store.cal.open_marker_ok && store.cal.close_marker_ok ? "ok"
                                                                     : "missing",
               stagePip(store.cal.open_marker_ok, false), false);
      listItem(2, "Stroke", store.cal.stroke_ok ? "ok" : "missing",
               stagePip(store.cal.stroke_ok, false), false);
      listItem(3, "Empty table",
               stageWord(store.cal.table_empty_ok, store.cal.table_empty_stale,
                         "armed"),
               stagePip(store.cal.table_empty_ok, store.cal.table_empty_stale),
               false);
      listItem(4, "Loaded table",
               stageWord(store.cal.table_loaded_ok, store.cal.table_loaded_stale,
                         "ok"),
               stagePip(store.cal.table_loaded_ok, store.cal.table_loaded_stale),
               false);
      listItem(5, "Single NVS write", "format v3", 1, true);
      ftr2("PRESS save", "HOLD back");
      break;

    default:
      break;
  }
  (void)tmp2;
}

void App::composeSet() {
  auto hdr = [&](const char* m, const char* r) {
    snprintf(view_.hdr_mode, sizeof(view_.hdr_mode), "SET - %s", m);
    copy(view_.hdr_net, sizeof(view_.hdr_net), r);
  };
  char tmp[48];
  char tmp2[48];
  Settings& p = store.pending;

  if (screen_ == Screen::SetMenu) {
    hdr("groups", setting_dirty_ ? "unsaved" : "NVS");
    view_.nitems = 7;
    listItem(0, "Motion profile", "7 values", setting_dirty_ ? 2 : 1,
             cursor_ == 0);
    listItem(1, "Travel and markers", "6 values", 1, cursor_ == 1);
    listItem(2, "Obstruction detection", "5 values", 1, cursor_ == 2);
    listItem(3, "Triggers and debounce", "3 values", 1, cursor_ == 3);
    listItem(4, "Display and knob", "3 values", 1, cursor_ == 4);
    listItem(5, "Network", "5 values", 1, cursor_ == 5);
    listItem(6, "Service and defaults", "5 actions", 0, cursor_ == 6);
    compactMenu(7);
    ftr3("TURN scroll", "PRESS open", "HOLD exit");
    return;
  }

  if (screen_ == Screen::SetBlocked) {
    hdr("blocked", "no write");
    view_.hero = View::Hero::Bad;
    copy(view_.hero_big, sizeof(view_.hero_big), "SETTINGS LOCKED");
    copy(view_.hero_sub, sizeof(view_.hero_sub),
         "configuration writes need an idle motor");
    row(0, "holder", phaseName(door.phase()));
    row(1, "pending changes", setting_dirty_ ? "kept in memory" : "none");
    row(2, "retry", "when motion completes");
    view_.nrows = 3;
    view_.nftr = 1;
    copy(view_.ftr[0], sizeof(view_.ftr[0]), "HOLD back");
    return;
  }

  if (screen_ == Screen::SetDerived) {
    hdr("derived - locked", "read only");
    snprintf(tmp, sizeof(tmp), "%ld cnt",
             static_cast<long>(store.cal.close_edge_counts));
    row(0, "Measured span", tmp, true);
    snprintf(tmp, sizeof(tmp), "%ld - derived from stroke",
             static_cast<long>(store.derivedStepsPerInch()));
    row(1, "Counts per inch", tmp, true);
    snprintf(tmp, sizeof(tmp), "%ld cnt - stage 5",
             static_cast<long>(store.cal.seated_counts));
    row(2, "Seated position", tmp, true);
    row(3, "Hard current ceiling", "2.50 A - compile-time", true);
    row(4, "PWM carrier", "15 kHz - 10-bit", true);
    view_.nrows = 5;
    view_.hero = View::Hero::Ok;
    copy(view_.hero_big, sizeof(view_.hero_big), "");
    view_.hero = View::Hero::None;
    // banner via hero Ok without big? use Warn small - skip
    view_.nftr = 1;
    copy(view_.ftr[0], sizeof(view_.ftr[0]), "HOLD back");
    return;
  }

  if (screen_ == Screen::SetMotion) {
    hdr("motion", "7 values");
    snprintf(tmp, sizeof(tmp), "%u", p.pwm_max_duty);
    listItem(0, "Cruise duty", tmp, 0, cursor_ == 0);
    snprintf(tmp, sizeof(tmp), "%u", p.pwm_min_duty);
    listItem(1, "Minimum moving duty", tmp, 0, cursor_ == 1);
    snprintf(tmp, sizeof(tmp), "%u", p.pwm_creep_duty);
    listItem(2, "Creep duty", tmp, 0, cursor_ == 2);
    snprintf(tmp, sizeof(tmp), "%u", p.pwm_jog_duty);
    listItem(3, "Jog duty", tmp, 0, cursor_ == 3);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.move_accel_ms));
    listItem(4, "Accelerate", tmp, 0, cursor_ == 4);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.move_cruise_ms));
    listItem(5, "Cruise time", tmp, 0, cursor_ == 5);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.move_decel_ms));
    listItem(6, "Decelerate", tmp, 0, cursor_ == 6);
    compactMenu(7);
    ftr3("TURN scroll", "PRESS edit", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetTravel) {
    hdr("travel", "6 values");
    snprintf(tmp, sizeof(tmp), "%ld cnt", static_cast<long>(p.travel_cap_counts));
    listItem(0, "Upper travel cap", tmp, 0, cursor_ == 0);
    snprintf(tmp, sizeof(tmp), "%ld cnt",
             static_cast<long>(p.marker_probe_tolerance_counts));
    listItem(1, "Probe tolerance", tmp, 0, cursor_ == 1);
    snprintf(tmp, sizeof(tmp), "%ld cnt",
             static_cast<long>(p.as5600_resync_drift_counts));
    listItem(2, "Re-sync drift max", tmp, 0, cursor_ == 2);
    snprintf(tmp, sizeof(tmp), "%ld cnt", static_cast<long>(p.close_limit_to_seated));
    listItem(3, "Close overtravel (locked)", tmp, 0, cursor_ == 3);
    snprintf(tmp, sizeof(tmp), "%ld cnt", static_cast<long>(p.open_limit_to_open));
    listItem(4, "Open overtravel (locked)", tmp, 0, cursor_ == 4);
    listItem(5, "Derived and locked values", "read only", 0, cursor_ == 5);
    view_.nitems = 6;
    ftr3("TURN scroll", "PRESS edit", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetObstruction) {
    hdr("obstruction", "5 values");
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.obstruction_inrush_skip_ms));
    listItem(0, "Inrush skip", tmp, 0, cursor_ == 0);
    snprintf(tmp, sizeof(tmp), "%.2f A", p.snug_amps);
    listItem(1, "Snug current", tmp, 0, cursor_ == 1);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.move_timeout_ms));
    listItem(2, "Move timeout", tmp, 0, cursor_ == 2);
    listItem(3, "Hard ceiling", "2.50 A locked", 0, cursor_ == 3);
    listItem(4, "Armed table", store.tablesArmed() ? "empty-rack" : "none",
             store.tablesArmed() ? 1 : 2, cursor_ == 4);
    view_.nitems = 5;
    ftr3("TURN scroll", "PRESS edit", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetTriggers) {
    hdr("triggers", "3 values");
    listItem(0, "Key polarity",
             p.trigger_absent_opens ? "absent opens" : "present opens", 1,
             cursor_ == 0);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.switch_debounce_ms));
    listItem(1, "Switch debounce", tmp, 0, cursor_ == 1);
    listItem(2, "Open wins if both throws", "tie-break", 1, cursor_ == 2);
    view_.nitems = 3;
    ftr3("TURN scroll", "PRESS edit", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetDisplay) {
    hdr("display", "3 values");
    snprintf(tmp, sizeof(tmp), "%lu min",
             static_cast<unsigned long>(p.display_timeout_ms / 60000));
    listItem(0, "Display timeout", tmp, 0, cursor_ == 0);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.ec11_hold_min_ms));
    listItem(1, "Hold back starts at", tmp, 0, cursor_ == 1);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.ec11_hold_max_ms));
    listItem(2, "Hold window ends", tmp, 0, cursor_ == 2);
    view_.nitems = 3;
    ftr3("TURN scroll", "PRESS edit", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetNetwork) {
    hdr("network", "5 values");
    row(0, "Station SSID",
        p.wifi_sta_ssid[0] ? p.wifi_sta_ssid : "(empty - AP only)");
    row(1, "Station password", "******** (web only)", true);
    snprintf(tmp, sizeof(tmp), "%lu ms",
             static_cast<unsigned long>(p.wifi_sta_timeout_ms));
    row(2, "Join timeout", tmp);
    row(3, "Fallback AP name", p.wifi_ap_ssid);
    row(4, "Fallback AP password", apJoinPassword());
    view_.nrows = 5;
    view_.hero = View::Hero::Warn;
    copy(view_.hero_big, sizeof(view_.hero_big), "");
    view_.hero = View::Hero::None;
    copy(view_.hero_sub, sizeof(view_.hero_sub),
         "No login. Anyone on this network can move the door.");
    ftr3("TURN scroll", "PRESS edit numbers", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetService) {
    hdr("service", "5 actions");
    listItem(0, "Erase obstruction tables", "both", 3, cursor_ == 0);
    listItem(1, "Restore setting defaults", "keeps calibration", 2,
             cursor_ == 1);
    listItem(2, "Clear calibration and origin", "forces setup", 3,
             cursor_ == 2);
    listItem(3, "Clear station Wi-Fi", "SoftAP after reboot", 2,
             cursor_ == 3);
    listItem(4, "Reboot controller", "now", 2, cursor_ == 4);
    view_.nitems = 5;
    const char* blast =
        cursor_ == 0
            ? "erases both envelopes - hard ceiling until relearn"
            : cursor_ == 1
                  ? "resets settings - keeps calibration and Wi-Fi"
                  : cursor_ == 2
                        ? "clears origin - automatic motion blocked"
                        : cursor_ == 3
                              ? "clears SSID - reboot required for SoftAP"
                              : "stops PWM - no motion resume";
    row(0, "selected action", blast);
    view_.nrows = 1;
    ftr2("PRESS confirm twice", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetWifiCleared) {
    hdr("wifi", "reboot");
    view_.hero = View::Hero::Warn;
    copy(view_.hero_big, sizeof(view_.hero_big), "WIFI CLEARED");
    copy(view_.hero_sub, sizeof(view_.hero_sub),
         "PRESS reboot - SoftAP on next boot");
    row(0, "Station SSID", "(empty - AP only)");
    row(1, "Fallback AP name", store.settings.wifi_ap_ssid);
    row(2, "Join password", apJoinPassword());
    row(3, "After reboot", "join AP, then set SSID on the web");
    view_.nrows = 4;
    ftr2("PRESS reboot", "HOLD back");
    return;
  }

  if (screen_ == Screen::SetEdit) {
    hdr("edit", "pending");
    view_.show_entry = true;
    view_.show_range = true;
    if (edit_field_ == EditField::Polarity) {
      view_.show_entry = false;
      view_.show_range = false;
      view_.nitems = 2;
      listItem(0, "Bottle absent opens the door", "default",
               pending_int_ ? 1 : 0, pending_int_ != 0);
      listItem(1, "Bottle present opens the door", "", pending_int_ ? 0 : 1,
               pending_int_ == 0);
      snprintf(tmp, sizeof(tmp), "%s - adc %u",
               key_->present() ? "PRESENT" : "ABSENT", key_->rawAdc());
      row(0, "key reads now", tmp);
      row(1, "on save", "re-arm from current level, no move");
      view_.nrows = 2;
    } else {
      snprintf(view_.entry_val, sizeof(view_.entry_val), "%d", pending_int_);
      view_.range_val = pending_int_;
      switch (edit_field_) {
        case EditField::CruiseDuty:
          copy(view_.entry_unit, sizeof(view_.entry_unit), "duty counts, 10-bit");
          view_.range_min = 128;
          view_.range_max = 768;
          view_.range_def = 512;
          row(0, "step per detent", "8 counts");
          row(1, "invalidates", "both learned tables");
          view_.nrows = 2;
          break;
        case EditField::DisplayTimeoutMin:
          copy(view_.entry_unit, sizeof(view_.entry_unit), "minutes of idle");
          view_.range_min = 1;
          view_.range_max = 60;
          view_.range_def = 5;
          row(0, "step per detent", "1 minute");
          row(1, "invalidates", "nothing", true);
          view_.nrows = 2;
          break;
        case EditField::HoldMinMs:
          copy(view_.entry_unit, sizeof(view_.entry_unit), "ms down before HOLD");
          view_.range_min = 200;
          view_.range_max = 1200;
          view_.range_def = static_cast<int32_t>(kEc11HoldMinMs);
          row(0, "step per detent", "50 ms");
          row(1, "fires while the button is still down", "then ignore release");
          view_.nrows = 2;
          break;
        case EditField::HoldMaxMs:
          copy(view_.entry_unit, sizeof(view_.entry_unit), "ms end of HOLD window");
          view_.range_min = 600;
          view_.range_max = 4000;
          view_.range_def = static_cast<int32_t>(kEc11HoldMaxMs);
          row(0, "step per detent", "100 ms");
          row(1, "no second HOLD if the finger stays down", "");
          view_.nrows = 2;
          break;
        default:
          copy(view_.entry_unit, sizeof(view_.entry_unit), "value");
          view_.range_min = 0;
          view_.range_max = pending_int_ + 100;
          view_.range_def = pending_int_;
          break;
      }
    }
    ftr3("TURN adjust", "PRESS accept", "HOLD cancel");
    return;
  }

  if (screen_ == Screen::SetConfirm) {
    hdr("confirm", "relearn required");
    view_.hero = View::Hero::Warn;
    copy(view_.hero_big, sizeof(view_.hero_big), "RELEARN REQUIRED");
    copy(view_.hero_sub, sizeof(view_.hero_sub),
         "Changing the profile invalidates both obstruction tables");
    listItem(0, "Empty-rack table", "armed -> stale", 2, false);
    listItem(1, "Loaded-rack table", "stale -> stale", 2, false);
    listItem(2, "Until relearn, trip is", "2.50 A ceiling only", 3, false);
    view_.nitems = 3;
    ftr2("PRESS accept change", "HOLD keep previous");
    return;
  }

  if (screen_ == Screen::SetReview) {
    hdr("review", "save");
    view_.hero = View::Hero::Ok;
    copy(view_.hero_big, sizeof(view_.hero_big), "READY TO SAVE");
    copy(view_.hero_sub, sizeof(view_.hero_sub), "motor idle - current decayed");
    snprintf(tmp, sizeof(tmp), "%u -> %u", store.settings.pwm_max_duty,
             p.pwm_max_duty);
    listItem(0, "Cruise duty", tmp, 2, false);
    snprintf(tmp, sizeof(tmp), "%lu -> %lu min",
             static_cast<unsigned long>(store.settings.display_timeout_ms / 60000),
             static_cast<unsigned long>(p.display_timeout_ms / 60000));
    listItem(1, "Display timeout", tmp, 1, false);
    snprintf(tmp, sizeof(tmp), "%lu -> %lu ms",
             static_cast<unsigned long>(store.settings.switch_debounce_ms),
             static_cast<unsigned long>(p.switch_debounce_ms));
    listItem(2, "Switch debounce", tmp, 1, false);
    listItem(3, "Side effect: both tables -> stale", "", 2, false);
    view_.nitems = 4;
    ftr3("PRESS save all", "TURN drop one", "HOLD discard");
    return;
  }

  (void)tmp2;
}
