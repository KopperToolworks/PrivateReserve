#include "adc_util.h"

#include "config.h"

#include <WiFi.h>
#include <esp_wifi.h>

namespace {

uint16_t samplePin(uint8_t pin, uint8_t samples) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return static_cast<uint16_t>(sum / samples);
}

}  // namespace

void restoreWebAp() {
  if (!kEnableWebPortal) {
    return;
  }
  // ADC2 sampling stops Wi-Fi. Do not rebuild SoftAP over a live STA session.
  if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    return;
  }

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(WIFI_PS_NONE);
  if (kApPassword[0] != '\0') {
    WiFi.softAP(kApSsid, kApPassword);
  } else {
    WiFi.softAP(kApSsid);
  }
}

uint16_t readAnalogPin(uint8_t pin, uint8_t samples) {
  if (kEnableWebPortal && isAdc2Pin(pin)) {
    esp_wifi_stop();
    delay(2);
    const uint16_t value = samplePin(pin, samples);
    esp_wifi_start();
    delay(5);
    restoreWebAp();
    return value;
  }
  return samplePin(pin, samples);
}
