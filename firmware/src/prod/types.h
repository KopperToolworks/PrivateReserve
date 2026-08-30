#pragma once

#include <Arduino.h>

#include "ec11_calibration.h"

constexpr int kBinCount = 81;
constexpr int kHistLen = 40;
constexpr uint8_t kMenuVisible = 6;
constexpr int kMenuRowPx = 16;
constexpr uint8_t kPanelPowerPin = 15;
constexpr uint8_t kBacklightPin = 38;
// Board A mount: USB-C left, EC11 right. Landscape 3 is operator-upright.
constexpr uint8_t kDisplayRotation = 3;

enum class Screen : uint8_t {
  Status,
  ModeMenu,
  DiagMenu,
  DiagBottleKey,
  DiagShaft,
  DiagLimits,
  DiagRockers,
  DiagCurrent,
  DiagPwm,
  DiagJog,
  DiagJogCounts,
  DiagRunLimit,
  DiagObstTest,
  DiagTableClose,
  DiagTableOpen,
  DiagEc11,
  DiagNetwork,
  DiagCalStatus,
  CalMenu,
  CalPathClear,
  CalCurrentZero,
  CalJitter,
  CalBottleKey,
  CalDirection,
  CalOpenMarker,
  CalCloseMarker,
  CalSeated,
  CalStroke,
  CalLearnEmpty,
  CalLearnLoaded,
  CalReview,
  SetMenu,
  SetMotion,
  SetTravel,
  SetObstruction,
  SetTriggers,
  SetDisplay,
  SetNetwork,
  SetDerived,
  SetService,
  SetEdit,
  SetConfirm,
  SetReview,
  SetBlocked,
  SetWifiCleared,
  Docs,
};

enum class DoorPhase : uint8_t {
  Idle,
  DecayWait,
  PwmBlank,
  RelaySettle,
  RampUp,
  Cruise,
  RampDown,
  CreepClose,
  Snug,
  CreepOpen,
  Jog,
  RunLimit,
  Calibrating,
  Fault,
};

enum class FaultId : uint8_t {
  None,
  HardOvercurrent,
  LearnedObstructionClose,
  ObstructionWhileOpening,
  RepeatedObstructionClose,
  MotionStall,
  LimitHit,
  TravelLimitExceeded,
  PositionUnknown,
  SensorFault,
  ReSyncDrift,
  CurrentDecayTimeout,
  MoveTimeout,
};

enum class Travel : uint8_t { Open, Close };

enum class MotorOwner : uint8_t { Som, Cal, Jog, ObstTest };

enum class Cmd : uint8_t {
  Turn,
  Press,
  Hold,
  JogBeat,
  WebText,
};

enum class EditField : uint8_t {
  None,
  CruiseDuty,
  MinDuty,
  CreepDuty,
  JogDuty,
  AccelMs,
  CruiseMs,
  DecelMs,
  DisplayTimeoutMin,
  HoldMinMs,
  HoldMaxMs,
  SwitchDebounce,
  JoinTimeoutMs,
  InrushSkip,
  SnugAmpsX100,
  TravelCap,
  ProbeTol,
  ResyncDrift,
  Polarity,
};

struct LoadBin {
  float mean = 0;
  float sigma = 0;
  float peak = 0;
  float trip = 0;
  bool valid = false;
};

struct LoadTable {
  LoadBin close[kBinCount];
  LoadBin open[kBinCount];
  uint8_t runs = 0;
};

struct Settings {
  uint16_t pwm_min_duty = 64;
  uint16_t pwm_max_duty = 512;
  uint16_t pwm_creep_duty = 96;
  uint16_t pwm_jog_duty = 96;
  uint32_t move_accel_ms = 3000;
  uint32_t move_decel_ms = 3000;
  uint32_t move_cruise_ms = 6000;
  uint32_t pwm_blank_before_dir_ms = 100;
  uint32_t pwm_blank_after_dir_ms = 100;
  uint32_t relay_settle_ms = 25;
  uint32_t display_timeout_ms = 300000;
  uint32_t switch_debounce_ms = 25;
  uint32_t wifi_sta_timeout_ms = 15000;
  uint32_t jog_step_ms = 400;
  uint32_t obstruction_inrush_skip_ms = 400;
  uint32_t current_decay_timeout_ms = 2000;
  uint32_t move_timeout_ms = 30000;
  int32_t travel_cap_counts = 220;
  int32_t as5600_resync_drift_counts = 80;
  int32_t as5600_min_progress_counts = 8;
  int32_t as5600_jitter_counts = 3;
  int32_t marker_probe_tolerance_counts = 40;
  int32_t close_limit_to_seated = 160;
  int32_t open_limit_to_open = 160;
  float stroke_inches = 0;
  float snug_amps = 1.80f;
  float hall_deviation_fraction = 0.10f;
  bool direction_invert = false;
  bool trigger_absent_opens = true;
  uint16_t bottle_key_absent_adc = 0;
  uint16_t bottle_key_present_adc = 0;
  char wifi_sta_ssid[33] = "";
  char wifi_sta_password[65] = "";
  // Same public defaults as kApSsid / kApPassword in wifi_secrets.h.
  char wifi_ap_ssid[33] = "PrivateReserve";
  char wifi_ap_password[65] = "reserved";
  uint32_t ec11_hold_min_ms = kEc11HoldMinMs;
  uint32_t ec11_hold_max_ms = kEc11HoldMaxMs;
  // Append-only: a mid-struct field shifts NVS and corrupts later keys
  // (hold time, Wi-Fi).
  int32_t jog_step_counts = 64;
};

struct CalRecord {
  uint8_t format = 3;
  bool current_zero_ok = false;
  bool jitter_ok = false;
  bool bottle_key_ok = false;
  bool direction_ok = false;
  bool open_marker_ok = false;
  bool close_marker_ok = false;
  bool seated_ok = false;
  bool stroke_ok = false;
  bool table_empty_ok = false;
  bool table_loaded_ok = false;
  bool table_empty_stale = false;
  bool table_loaded_stale = false;
  float current_zero_v = 1.65f;
  int32_t jitter_pp = 3;
  int32_t close_edge_counts = 0;
  int32_t seated_counts = 0;
  int32_t open_spread = 0;
  int32_t close_spread = 0;
  char seated_method[12] = "";
  int32_t steps_per_inch = 0;
  int32_t drift_open = 0;
  int32_t drift_close = 0;
  uint16_t profile_cruise = 512;
  uint32_t profile_accel = 3000;
  uint32_t profile_cruise_ms = 6000;
  uint32_t profile_decel = 3000;
};

struct View {
  Screen screen = Screen::Status;
  char hdr_mode[28] = "";
  char hdr_net[32] = "";
  bool net_ap = false;

  enum class Hero : uint8_t { None, Action, Bad, Warn, Ok } hero = Hero::None;
  char hero_big[40] = "";
  char hero_sub[72] = "";
  char hero_cls = ' ';
  char hero_right[36] = "";
  char hero_right2[36] = "";

  bool show_travel = false;
  bool show_amps = false;
  float amps = 0;
  float trip = 0;
  int pos_pct = 0;
  int32_t counts = 0;
  bool pos_known = false;
  bool moving = false;
  bool fault_knob = false;
  bool open_mark = false;
  bool close_mark = false;

  struct Tile {
    char k[22];
    char v[16];
    char g[24];
    uint8_t pip;
    bool sel;
  } tiles[4];
  uint8_t ntiles = 0;
  uint8_t tile_cols = 4;

  struct Row {
    char k[42];
    char v[42];
    uint8_t dim;
    uint8_t sel;
  } rows[6];
  uint8_t nrows = 0;

  struct Li {
    char label[48];
    char value[28];
    uint8_t pip;
    bool sel;
  } items[14];
  uint8_t nitems = 0;

  char pin_parts[80] = "";
  char pin_lv[44] = "";
  bool show_pin = false;

  char ftr[3][28];
  uint8_t nftr = 0;

  bool show_entry = false;
  char entry_val[16] = "";
  char entry_unit[36] = "";
  char entry_note[56] = "";
  int32_t range_val = 0;
  int32_t range_min = 0;
  int32_t range_max = 0;
  int32_t range_def = 0;
  bool show_range = false;

  uint8_t prog[4] = {0, 0, 0, 0};
  char prog_l[16] = "";
  char prog_r[16] = "";
  bool show_prog = false;

  uint8_t graph = 0;
  uint8_t live_bin = 0;
  uint8_t spark = 0;
  bool show_qr = false;
};

inline const char* faultName(FaultId id) {
  switch (id) {
    case FaultId::None:
      return "none";
    case FaultId::HardOvercurrent:
      return "HARD OVERCURRENT";
    case FaultId::LearnedObstructionClose:
      return "OBSTRUCTION";
    case FaultId::ObstructionWhileOpening:
      return "OBSTRUCTION OPEN";
    case FaultId::RepeatedObstructionClose:
      return "OBSTRUCTION x2";
    case FaultId::MotionStall:
      return "MOTION STALL";
    case FaultId::LimitHit:
      return "LIMIT / CUT WIRE";
    case FaultId::TravelLimitExceeded:
      return "TRAVEL LIMIT";
    case FaultId::PositionUnknown:
      return "POSITION UNKNOWN";
    case FaultId::SensorFault:
      return "SENSOR FAULT";
    case FaultId::ReSyncDrift:
      return "RE-SYNC DRIFT";
    case FaultId::CurrentDecayTimeout:
      return "CURRENT DECAY";
    case FaultId::MoveTimeout:
      return "MOVE TIMEOUT";
  }
  return "?";
}

inline const char* phaseName(DoorPhase p) {
  switch (p) {
    case DoorPhase::Idle:
      return "idle";
    case DoorPhase::DecayWait:
      return "current decay";
    case DoorPhase::PwmBlank:
      return "pwm blank";
    case DoorPhase::RelaySettle:
      return "relay settle";
    case DoorPhase::RampUp:
      return "accel";
    case DoorPhase::Cruise:
      return "cruise";
    case DoorPhase::RampDown:
      return "decel";
    case DoorPhase::CreepClose:
      return "creep close";
    case DoorPhase::Snug:
      return "snug";
    case DoorPhase::CreepOpen:
      return "creep open";
    case DoorPhase::Jog:
      return "jog";
    case DoorPhase::RunLimit:
      return "run to limit";
    case DoorPhase::Calibrating:
      return "calibrating";
    case DoorPhase::Fault:
      return "fault";
  }
  return "?";
}
