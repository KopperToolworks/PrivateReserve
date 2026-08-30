#include "net.h"

#include "config.h"
#include "door.h"
#include "store.h"
#include "types.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#define PR_OTA_PASS kOtaPassword
#else
#define PR_OTA_PASS "reserve-ota"
#endif

namespace {
bool ota_started = false;

void stopPwm() { ledcWrite(0, 0); }
}  // namespace

void netBegin() {
  const Settings& s = store.settings;
  bool sta_ok = false;
  if (s.wifi_sta_ssid[0] != '\0') {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(WIFI_PS_NONE);
    WiFi.begin(s.wifi_sta_ssid, s.wifi_sta_password);
    Serial.print(F("[net] STA "));
    Serial.println(s.wifi_sta_ssid);
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - t0 < s.wifi_sta_timeout_ms) {
      delay(200);
      esp_task_wdt_reset();
    }
    sta_ok = WiFi.status() == WL_CONNECTED;
  }

  if (!sta_ok) {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(WIFI_PS_NONE);
    char ap[33];
    char pw[65];
    strncpy(ap, s.wifi_ap_ssid, sizeof(ap) - 1);
    ap[sizeof(ap) - 1] = 0;
    strncpy(pw, s.wifi_ap_password, sizeof(pw) - 1);
    pw[sizeof(pw) - 1] = 0;
    if (ap[0] == '\0') {
      strncpy(ap, "PrivateReserve", sizeof(ap) - 1);
      ap[sizeof(ap) - 1] = 0;
    }
    if (pw[0] != '\0') {
      WiFi.softAP(ap, pw);
    } else {
      WiFi.softAP(ap);
    }
    Serial.print(F("[net] SoftAP "));
    Serial.print(ap);
    if (pw[0] != '\0') {
      Serial.print(F("  pw "));
      Serial.print(pw);
    } else {
      Serial.print(F("  open"));
    }
    Serial.print(F("  "));
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print(F("[net] STA IP "));
    Serial.println(WiFi.localIP());
  }

  if (MDNS.begin(kOtaHostname)) {
    MDNS.addService("http", "tcp", 80);
  }

  ArduinoOTA.setHostname(kOtaHostname);
  ArduinoOTA.setPassword(PR_OTA_PASS);
  ArduinoOTA.onStart([]() {
    if (!door.idle()) {
      Serial.println(F("[ota] refused — motor not idle"));
    }
    stopPwm();
    esp_task_wdt_reset();
  });
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { esp_task_wdt_reset(); });
  ArduinoOTA.begin();
  ota_started = true;
}

void netLoop() {
  if (ota_started && door.idle()) {
    ArduinoOTA.handle();
  }
}
