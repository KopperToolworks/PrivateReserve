#pragma once

#include <Arduino.h>

inline bool isAdc2Pin(uint8_t pin) {
#if CONFIG_IDF_TARGET_ESP32S3
  // ESP32-S3 ADC2 is GPIO11–20. GPIO1/GPIO2 are ADC1 (CH0/CH1).
  return pin >= 11 && pin <= 20;
#else
  switch (pin) {
    case 0:
    case 2:
    case 4:
    case 12:
    case 13:
    case 14:
    case 15:
    case 25:
    case 26:
    case 27:
      return true;
    default:
      return false;
  }
#endif
}

// Reads an ESP32 ADC pin. Suspends WiFi briefly for ADC2 pins when the web AP is on.
uint16_t readAnalogPin(uint8_t pin, uint8_t samples);

void restoreWebAp();
