#pragma once

#include "door.h"
#include "types.h"

#include "bottle_key.h"

class App {
 public:
  void begin(BottleKeySensor* key);
  void update();
  void handle(Cmd cmd, int32_t arg, const char* text = nullptr);
  void jump(Screen s) { go(s); }
  void compose();
  const View& view() const { return view_; }
  Screen screen() const { return screen_; }
  bool displayOn() const { return display_on_; }
  void setDisplayOn(bool on);
  const char* netLabel() const { return net_label_; }
  bool staUp() const { return sta_up_; }
  bool apUp() const { return ap_up_; }
  const char* mdnsName() const { return "private-reserve"; }
  int32_t rotaryDetents() const { return detents_; }
  int32_t rotaryRaw() const { return rotary_raw_; }
  void addRotary(int32_t steps);
  const uint16_t* hallHist() const { return hall_hist_; }
  const uint16_t* ampHist() const { return amp_hist_; }
  uint8_t histCount() const { return hist_count_; }
  uint8_t histChronoIndex(uint8_t i) const {
    const uint8_t start =
        static_cast<uint8_t>((hist_head_ + kHistLen - hist_count_) % kHistLen);
    return static_cast<uint8_t>((start + i) % kHistLen);
  }
  BottleKeySensor* key() { return key_; }
  uint32_t displayRemainMs() const;
  const char* lastIgnore() const { return ignore_; }
  EditField editField() const { return edit_field_; }
  MotorOwner owner() const { return owner_; }
  uint8_t pathChecks() const {
    return (path_ok_[0] ? 1 : 0) | (path_ok_[1] ? 2 : 0) | (path_ok_[2] ? 4 : 0);
  }
  int pendingInt() const { return pending_int_; }
  float pendingFloat() const { return pending_float_; }
  uint8_t cursor() const { return cursor_; }
  uint8_t learnRun() const { return learn_run_; }
  uint8_t probeN() const { return probe_n_; }
  int32_t probeSpread() const { return probe_spread_; }
  char calNote[48] = "";
  uint32_t key_present_ms = 0;
  uint32_t last_key_edge_ms = 0;
  uint32_t last_open_edge_ms = 0;
  uint32_t last_close_edge_ms = 0;
  uint16_t open_edges = 0;
  uint16_t close_edges = 0;
  bool open_rocker = false;
  bool close_rocker = false;
  uint32_t last_press_ms = 0;

 private:
  void pollInputs();
  void pollMomentary(bool down, bool& last, uint32_t& edge_ms,
                     uint32_t& down_ms, bool& hold_sent, bool revert_turn);
  void pollTriggers();
  void clearDiagInject();
  void pulseRocker(bool open);
  void pulseKey();
  void recaptureKey();
  BottleKeySensor::State effectiveKey() const;
  void onTurn(int32_t n);
  void noteMenuTurn(Screen s0, uint8_t c0);
  void revertRecentTurn();
  void onPress();
  void onHold();
  void go(Screen s);
  void goStatus();
  void startStage(Screen s);
  void cancelProc();
  void updateCal();
  void updateLearn();
  void finishProbe(bool open_end);
  void applyEdit(int32_t n);
  void acceptEdit();
  void saveSettings();
  void composeStatus();
  void composeDiag();
  void composeCal();
  void composeSet();
  void composeDocs();
  void listItem(uint8_t i, const char* label, const char* value, uint8_t pip,
                bool sel);
  uint8_t compactMenu(uint8_t total);
  void row(uint8_t i, const char* k, const char* v, bool dim = false);
  void ftr2(const char* a, const char* b);
  void ftr3(const char* a, const char* b, const char* c);
  void pin(const char* parts, const char* lv);
  const char* actionWord() const;
  char actionClass() const;
  bool somBlocked() const;
  void pushHist();
  void maybeBlank(uint32_t now);
  void wake();

  BottleKeySensor* key_ = nullptr;
  View view_;
  Screen screen_ = Screen::Status;
  Screen return_to_ = Screen::Status;
  Screen cal_target_ = Screen::CalCurrentZero;
  MotorOwner owner_ = MotorOwner::Som;
  uint8_t cursor_ = 0;
  uint8_t scroll_ = 0;
  uint8_t prev_cursor_ = 0;
  Screen prev_screen_ = Screen::Status;
  uint32_t last_turn_ms_ = 0;
  bool display_on_ = true;
  uint32_t last_activity_ = 0;
  char net_label_[32] = "AP ...";
  bool sta_up_ = false;
  bool ap_up_ = false;
  int32_t rotary_raw_ = 0;
  int32_t detents_ = 0;
  int32_t rotary_acc_ = 0;
  uint16_t hall_hist_[kHistLen] = {};
  uint16_t amp_hist_[kHistLen] = {};
  uint8_t hist_head_ = 0;
  uint8_t hist_count_ = 0;
  char ignore_[48] = "";
  EditField edit_field_ = EditField::None;
  int pending_int_ = 0;
  float pending_float_ = 0;
  uint8_t confirm_hits_ = 0;
  bool path_ok_[3] = {false, false, false};
  uint8_t probe_n_ = 0;
  int32_t probe_edge_[6] = {};
  int32_t probe_spread_ = 0;
  int32_t jitter_min_ = 0;
  int32_t jitter_max_ = 0;
  uint32_t stage_t0_ = 0;
  uint8_t key_capture_ = 0;
  uint8_t learn_run_ = 0;
  Travel learn_dir_ = Travel::Close;
  uint32_t bin_n_[kBinCount] = {};
  float bin_sum_[kBinCount] = {};
  float bin_sum2_[kBinCount] = {};
  float bin_peak_[kBinCount] = {};
  LoadTable learn_acc_;
  bool setting_dirty_ = false;
  bool last_open_rocker_ = false;
  bool last_close_rocker_ = false;
  BottleKeySensor::State last_key_ = BottleKeySensor::State::Unknown;
  bool last_sw_ = false;
  uint32_t sw_change_ms_ = 0;
  uint32_t sw_down_ms_ = 0;
  bool sw_hold_sent_ = false;
  bool last_user_ = false;
  uint32_t user_change_ms_ = 0;
  uint32_t user_down_ms_ = 0;
  bool user_hold_sent_ = false;
  uint32_t rocker_change_ms_ = 0;
  bool open_raw_ = false;
  bool close_raw_ = false;
  uint32_t last_ctrl_ms_ = 0;
  uint32_t last_hist_ms_ = 0;
  uint32_t sim_open_until_ = 0;
  uint32_t sim_close_until_ = 0;
  bool sim_key_on_ = false;
  BottleKeySensor::State sim_key_ = BottleKeySensor::State::Absent;
};

extern App app;
