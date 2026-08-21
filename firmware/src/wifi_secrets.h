#pragma once

// Station (site Wi-Fi). Leave empty in the published tree.
// Shop: copy this file to wifi_secrets.h (gitignored) and fill SSID/password
// to seed NVS on first boot.
constexpr char kWifiSsid[] = "";
constexpr char kWifiPassword[] = "";

// Fallback SoftAP. Public defaults. Same values as the operator docs and LCD.
#define PR_HAS_AP_DEFAULTS 1
constexpr char kApSsid[] = "PrivateReserve";
constexpr char kApPassword[] = "reserved";  // WPA2; min 8 chars; "" = open

// ArduinoOTA / PlatformIO espota.
constexpr char kOtaPassword[] = "reserve-ota";
