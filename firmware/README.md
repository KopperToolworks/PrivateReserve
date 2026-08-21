# Production firmware

Operator image for the LilyGo T-Display-S3 on Board A. PlatformIO env
`tdisplay_s3_prod`.

A prebuilt binary is in [`images/tdisplay_s3_prod.bin`](images/tdisplay_s3_prod.bin).
Rebuild from source if you need a fresh image.

## SoftAP (no site Wi-Fi yet)

| Item | Value |
|------|-------|
| Name | `PrivateReserve` |
| Password | `reserved` |
| HTTP | `http://192.168.4.1/` |
| Set site Wi-Fi | `http://192.168.4.1/wifi` |

The join password is on the LCD (Settings → Network).

## Build and flash

```bash
cd firmware
pio run -e tdisplay_s3_prod
pio run -e tdisplay_s3_prod -t upload --upload-port /dev/ttyACM0
```

After the board has joined site Wi-Fi:

```bash
pio run -e tdisplay_s3_prod_ota -t upload
```

OTA host is `private-reserve.local`. Auth is `reserve-ota` in
[`src/wifi_secrets.h`](src/wifi_secrets.h) unless you change it.

That file is committed here. Station SSID and password are empty. SoftAP
name and password are the public defaults (`PrivateReserve` / `reserved`).
Do not put a site network in this copy.
