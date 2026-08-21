#pragma once

#include "types.h"

class Store {
 public:
  void begin();
  void load();
  bool saveSettings(const Settings& s);
  bool persistSettings();
  bool saveCal(const CalRecord& c);
  bool saveTable(bool loaded, const LoadTable& t);
  bool eraseTables();
  bool eraseCal();
  bool factoryErase();
  bool clearStaWifi();

  Settings settings;
  Settings pending;
  CalRecord cal;
  LoadTable empty_table;
  LoadTable loaded_table;
  uint8_t dirty_settings = 0;
  char reject[48] = "";

  bool somReady() const;
  bool tablesArmed() const;
  const LoadTable* armedTable() const;
  void markTablesStale();
  void copyPendingFromLive();
  int32_t derivedStepsPerInch() const;
};

extern Store store;
