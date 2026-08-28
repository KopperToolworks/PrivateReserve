#include "store.h"

#include <Preferences.h>
#include <cstring>

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#define PR_HAS_WIFI_SECRETS 1
#else
#define PR_HAS_WIFI_SECRETS 0
#endif

Store store;

namespace {
Preferences nvs;
constexpr const char* kNs = "prsv";

void clearTable(LoadTable& t) {
  t = LoadTable{};
}
}  // namespace

void Store::copyPendingFromLive() { pending = settings; }

int32_t Store::derivedStepsPerInch() const {
  if (cal.stroke_ok && settings.stroke_inches > 0.1f && cal.seated_counts > 0) {
    return static_cast<int32_t>(
        static_cast<float>(cal.seated_counts) / settings.stroke_inches + 0.5f);
  }
  return cal.steps_per_inch;
}

bool Store::tablesArmed() const {
  return cal.table_empty_ok && !cal.table_empty_stale;
}

bool Store::somReady() const {
  return cal.open_marker_ok && cal.close_marker_ok && cal.direction_ok &&
         tablesArmed();
}

const LoadTable* Store::armedTable() const {
  if (!tablesArmed()) {
    return nullptr;
  }
  return &empty_table;
}

void Store::markTablesStale() {
  if (cal.table_empty_ok) {
    cal.table_empty_stale = true;
  }
  if (cal.table_loaded_ok) {
    cal.table_loaded_stale = true;
  }
}

void Store::begin() {
  nvs.begin(kNs, false);
  load();
}

void Store::load() {
  settings = Settings{};
#if PR_HAS_WIFI_SECRETS
  if (kWifiSsid[0] != '\0' && strcmp(kWifiSsid, "YOUR_SSID") != 0) {
    strncpy(settings.wifi_sta_ssid, kWifiSsid, sizeof(settings.wifi_sta_ssid) - 1);
    strncpy(settings.wifi_sta_password, kWifiPassword,
            sizeof(settings.wifi_sta_password) - 1);
  }
#endif
  nvs.getBytes("set", &settings, sizeof(settings));
  if (strcmp(settings.wifi_ap_password, "reservebench1") == 0) {
    strncpy(settings.wifi_ap_password, "reserved",
            sizeof(settings.wifi_ap_password) - 1);
    settings.wifi_ap_password[sizeof(settings.wifi_ap_password) - 1] = 0;
    persistSettings();
  }
  nvs.getBytes("cal", &cal, sizeof(cal));
  nvs.getBytes("tempty", &empty_table, sizeof(empty_table));
  nvs.getBytes("tload", &loaded_table, sizeof(loaded_table));
  if (settings.ec11_hold_min_ms < 150 || settings.ec11_hold_min_ms > 3000) {
    settings.ec11_hold_min_ms = kEc11HoldMinMs;
  }
  if (settings.ec11_hold_max_ms < settings.ec11_hold_min_ms ||
      settings.ec11_hold_max_ms > 8000) {
    settings.ec11_hold_max_ms = kEc11HoldMaxMs;
  }
  if (settings.jog_step_ms < 200 || settings.jog_step_ms > 1000) {
    settings.jog_step_ms = 400;
  }
  if (settings.jog_step_counts < 16 || settings.jog_step_counts > 512) {
    settings.jog_step_counts = 64;
  }
  if (cal.format != 3) {
    cal = CalRecord{};
    clearTable(empty_table);
    clearTable(loaded_table);
  }
  pending = settings;
}

bool Store::saveSettings(const Settings& s) {
  settings = s;
  pending = s;
  return persistSettings();
}

bool Store::persistSettings() {
  return nvs.putBytes("set", &settings, sizeof(settings)) == sizeof(settings);
}

bool Store::saveCal(const CalRecord& c) {
  cal = c;
  return nvs.putBytes("cal", &cal, sizeof(cal)) == sizeof(cal);
}

bool Store::saveTable(bool loaded, const LoadTable& t) {
  if (loaded) {
    loaded_table = t;
    return nvs.putBytes("tload", &loaded_table, sizeof(loaded_table)) ==
           sizeof(loaded_table);
  }
  empty_table = t;
  return nvs.putBytes("tempty", &empty_table, sizeof(empty_table)) ==
         sizeof(empty_table);
}

bool Store::eraseTables() {
  clearTable(empty_table);
  clearTable(loaded_table);
  cal.table_empty_ok = false;
  cal.table_loaded_ok = false;
  cal.table_empty_stale = false;
  cal.table_loaded_stale = false;
  nvs.remove("tempty");
  nvs.remove("tload");
  return saveCal(cal);
}

bool Store::eraseCal() {
  cal = CalRecord{};
  clearTable(empty_table);
  clearTable(loaded_table);
  nvs.remove("cal");
  nvs.remove("tempty");
  nvs.remove("tload");
  return true;
}

bool Store::clearStaWifi() {
  memset(settings.wifi_sta_ssid, 0, sizeof(settings.wifi_sta_ssid));
  memset(settings.wifi_sta_password, 0, sizeof(settings.wifi_sta_password));
  return saveSettings(settings);
}

bool Store::factoryErase() {
  nvs.clear();
  settings = Settings{};
  pending = settings;
  eraseCal();
  return true;
}
