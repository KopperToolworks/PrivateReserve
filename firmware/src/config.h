#pragma once

#include <Arduino.h>

#include "ec11_calibration.h"

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif

// Pin map — LilyGo T-Display-S3 (production). See docs/T-Display-S3_Pinout.md
// Motor rotation: AS5600 I²C. Genie optical encoder is not used.
// Firmware is not fully defined (NVS, snug, frozen env matrix still open).
namespace pins {

constexpr uint8_t kUserButton = 14;       // On-board button (cycle views)
constexpr uint8_t kPwm = 16;
constexpr uint8_t kCurrentSense = 2;      // ACS712-05B divider -> GPIO02
constexpr uint8_t kHallAnalog = 1;        // SS49E bottle door key -> GPIO01
constexpr uint8_t kLimitUpper = 17;
constexpr uint8_t kLimitLower = 3;
constexpr uint8_t kDirectionRelay = 21;
constexpr uint8_t kOpenButton = 10;       // Dual paralleled rockers, active-low
constexpr uint8_t kCloseButton = 18;      // Dual paralleled rockers, active-low
constexpr uint8_t kAs5600Sda = 43;
constexpr uint8_t kAs5600Scl = 44;
// EC11 rotary encoder — LEFT header GPIO11–13
constexpr uint8_t kRotaryClk = 11;
constexpr uint8_t kRotaryDt = 12;
constexpr uint8_t kRotarySw = 13;

}  // namespace pins

// --- AS5600 (motor shaft angle) ---
constexpr uint8_t kAs5600I2cAddr = 0x36;
constexpr uint8_t kAs5600RegRawAngle = 0x0C;  // 12-bit angle, MSB then LSB

// --- Bottle door key (SS49E, GPIO1) ---
constexpr uint16_t kHallAdcMin = 800;
constexpr uint16_t kHallAdcMax = 3200;
constexpr uint8_t kHallAdcSamples = 8;
constexpr float kHallDeviationFraction = 0.10f;

// --- Limits (near-end markers; docs/Limit_Marker_And_Snug.md) ---
// NC-to-GND, INPUT_PULLUP. HIGH = marker or open wire. LOW is travel.
constexpr bool kLimitActiveLow = false;
constexpr uint8_t kLimitDebounceCount = 3;

// --- Trigger / relay ---
constexpr bool kTriggerActiveLow = true;
constexpr bool kDirectionRelayOpenHigh = true;
constexpr bool kRotarySwitchActiveLow = true;
constexpr uint32_t kRelaySettleMs = 25;
constexpr uint32_t kPwmBlankBeforeDirMs = 100;

// --- PWM ---
// ESP32-S3 Arduino 2.x LEDC uses the 40 MHz XTAL clock. At 12-bit that
// caps frequency at ~9.8 kHz, so ledcSetup(15000, 12) fails and the pin
// stays LOW. 10-bit allows 15 kHz (max ~39 kHz).
constexpr uint32_t kPwmFrequencyHz = 15000;
constexpr uint8_t kPwmResolutionBits = 10;
constexpr uint16_t kPwmMaxDuty = (1u << kPwmResolutionBits) - 1u;
constexpr bool kPwmInvertDuty = false;

// --- Move profile (~12 s total) ---
constexpr uint32_t kMoveRampUpMs = 3000;
constexpr uint32_t kMoveCruiseMs = 6000;
constexpr uint32_t kMoveRampDownMs = 3000;
constexpr uint16_t kMoveCruiseDuty = 512;  // ~50% of 10-bit max (1023)

// --- Current sensor (Hall module OUT -> divider -> ADC GPIO2) ---
// ACS712-05B: 185 mV/A, 10k/20k divider. Bench and production use this module.
constexpr float kAcs712ModuleZeroVolts = 2.50f;
constexpr float kAcs712MvPerAmp = 185.0f;
constexpr float kAcs712DividerRatio = 20.0f / 30.0f;
constexpr uint8_t kAcs712AdcSamples = 16;
constexpr uint32_t kAcs712CalDelayMs = 200;
// Preferred ACS712 path is fused +70 V high rail (true motor current) → false.
// Set true only for a return-path / chopped-current build.
constexpr bool kOvercurrentScaleByDuty = false;
constexpr float kOvercurrentMinDutyFraction = 0.15f;
constexpr float kOvercurrentTripAmps = 2.5f;
constexpr uint32_t kOvercurrentHoldMs = 150;

// --- Safety ---
constexpr uint32_t kMotionWatchdogMs = 500;
constexpr uint16_t kMotionMinDuty = 64;  // ~6% of 10-bit max
constexpr int32_t kMinPositionProgressCounts = 5;

// --- Loop ---
constexpr uint32_t kControlLoopMs = 10;
constexpr uint32_t kStatusIntervalMs = 1000;
constexpr uint32_t kWatchdogTimeoutSec = 3;
constexpr uint32_t kSerialBaud = 115200;

// --- WiFi web dashboard (STA when joined; SoftAP if STA is down) ---
constexpr bool kEnableWebPortal = true;
#ifndef PR_HAS_AP_DEFAULTS
constexpr char kApSsid[] = "PrivateReserve";
constexpr char kApPassword[] = "reserved";  // WPA2; min 8 chars; "" = open network
#endif
constexpr uint16_t kWebPort = 80;
constexpr uint32_t kWebSampleMs = 200;

// --- OTA (T-Display STA + ArduinoOTA; secrets in wifi_secrets.h) ---
#if defined(TDISPLAY_S3_DEBUG) || defined(TDISPLAY_S3_OTA) || \
    defined(TDISPLAY_S3_EC11) || defined(TDISPLAY_S3_GPIO) || \
    defined(TDISPLAY_S3_PWM) || defined(TDISPLAY_S3_MAGSCAN) || \
    defined(TDISPLAY_S3_PROD)
constexpr bool kEnableOta = true;
#else
constexpr bool kEnableOta = false;
#endif
constexpr char kOtaHostname[] = "private-reserve";
