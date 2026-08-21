#pragma once

#include "as5600.h"
#include "store.h"
#include "types.h"

class Door {
 public:
  void begin(As5600Sensor* enc);
  void update();

  bool idle() const { return phase_ == DoorPhase::Idle; }
  bool running() const {
    return phase_ != DoorPhase::Idle && phase_ != DoorPhase::Fault;
  }
  DoorPhase phase() const { return phase_; }
  FaultId fault() const { return fault_; }
  FaultId lastEvent() const { return last_event_; }
  Travel direction() const { return dir_; }
  uint16_t duty() const { return duty_; }
  float amps() const { return amps_; }
  uint16_t currentAdc() const { return current_adc_; }
  float zeroVolts() const { return zero_v_; }
  int32_t position() const { return trusted_ ? counts_ - origin_ : counts_; }
  bool positionTrusted() const { return trusted_; }
  bool magnetOk() const;
  uint16_t rawAngle() const;
  float revPerSec() const { return rps_; }
  bool k1Open() const { return dir_ == Travel::Open; }
  bool openMark() const { return open_mark_ || force_open_; }
  bool closeMark() const { return close_mark_ || force_close_; }
  bool bothMarks() const { return openMark() && closeMark(); }
  bool openMarkForced() const { return force_open_; }
  bool closeMarkForced() const { return force_close_; }
  void setOpenMarkForce(bool on) { force_open_ = on; }
  void setCloseMarkForce(bool on) { force_close_ = on; }
  void clearMarkForce() { force_open_ = false; force_close_ = false; }
  int binIndex() const;
  float binTrip() const;
  const LoadBin* binNow() const;
  uint32_t phaseElapsedMs() const;
  uint32_t pwmOnMs() const { return pwm_on_ms_; }
  int32_t unwrapDelta() const { return last_delta_; }
  const char* lastHint() const { return hint_; }

  void requestOpen();
  void requestClose();
  void cancel();
  void acknowledgeFault();
  void startJog(Travel dir);
  void jogBeat();
  void startCreep(Travel dir);
  void holdCreep();
  void stopMotionKeepK1();
  void captureZero();
  void setOriginHere();
  void setTrusted(bool v) { trusted_ = v; }
  void sampleJitter(int32_t& min_c, int32_t& max_c);

 private:
  void applyDuty(uint16_t d);
  void setRelay(Travel dir);
  void setPhase(DoorPhase p);
  void latch(FaultId id);
  void startMove(Travel dir, bool reverse);
  void enterBlank(Travel next);
  float readAmps();
  void unwrap();
  void debounceMarks();
  void checkHardCurrent(uint32_t now);
  void checkEnvelope(uint32_t now);
  void checkStall(uint32_t now);
  void checkTravelCap();
  void advance(uint32_t now);
  uint16_t smoothDuty(uint32_t elapsed, uint32_t dur, uint16_t from,
                      uint16_t to) const;
  bool inCreepOrSnug() const;
  const LoadTable* table() const;

  As5600Sensor* enc_ = nullptr;
  DoorPhase phase_ = DoorPhase::Idle;
  FaultId fault_ = FaultId::None;
  FaultId last_event_ = FaultId::None;
  Travel dir_ = Travel::Close;
  Travel pending_dir_ = Travel::Close;
  uint16_t duty_ = 0;
  float amps_ = 0;
  uint16_t current_adc_ = 0;
  float zero_v_ = 1.65f;
  int32_t counts_ = 0;
  int32_t origin_ = 0;
  bool trusted_ = false;
  uint16_t last_raw_ = 0;
  int32_t last_delta_ = 0;
  float rps_ = 0;
  int32_t rps_acc_ = 0;
  uint32_t rps_t_ = 0;
  bool open_mark_ = false;
  bool close_mark_ = false;
  bool force_open_ = false;
  bool force_close_ = false;
  bool open_pending_ = false;
  bool close_pending_ = false;
  uint8_t open_n_ = 0;
  uint8_t close_n_ = 0;
  uint32_t phase_t_ = 0;
  uint32_t pwm_on_ms_ = 0;
  uint32_t move_t0_ = 0;
  uint32_t oc_t_ = 0;
  uint32_t env_t_ = 0;
  uint32_t stall_t_ = 0;
  int32_t stall_pos_ = 0;
  uint32_t jog_beat_ = 0;
  bool oc_hold_ = false;
  bool env_hold_ = false;
  bool reversed_this_chain_ = false;
  bool resync_done_ = false;
  bool current_tick_ = false;
  bool next_is_cal_ = false;
  bool next_is_jog_ = false;
  char hint_[40] = "";
};

extern Door door;
