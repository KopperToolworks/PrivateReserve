# Private Reserve

Private Reserve is a door controller for openings that a stock garage opener is a poor fit for: a concealed or architectural door, a 140 V DC motor without the manufacturer's logic board, and a close that must seat against the frame rather than hit a hard stop.

It ramps the motor with PWM instead of switching it full-on. A magnetic shaft encoder is the position source. Near-end markers only flag the last fraction of travel. Motor current both stops an obstruction mid-stroke and, after the close marker, snugs the door shut. The same firmware takes commands from a magnetic trigger, dual rocker stations, the on-box knob and LCD, and a phone web page that shares that LCD state.

The trigger is a hall sensor. A magnet placed over it opens the door. The magnet can be hidden in an object; the recess in the bottom of a bottle is one example.

This repository holds operator documentation, as-built wiring notes, and the production firmware image.

## Control network


| Item                    | Value                           |
| ----------------------- | ------------------------------- |
| Fallback AP name (SSID) | `PrivateReserve`                |
| Fallback AP password    | `reserved` (WPA2)               |
| SoftAP HTTP             | `http://192.168.4.1/`           |
| Set site Wi-Fi          | `http://192.168.4.1/wifi`       |
| mDNS (after STA join)   | `http://private-reserve.local/` |
| OTA hostname            | `private-reserve`               |


Join the AP, type the password, then open the HTTP address. The LCD shows the join password on Settings → Network. Station (house) SSID and password do not belong in this repository.

## Layout

| Path | Contents |
|------|----------|
| [`docs/`](docs/) | Operator docs. Software and wiring HTML in [`docs/html/`](docs/html/), PDF guides in [`docs/pdf/`](docs/pdf/). Rebuild with [`docs/build.mjs`](docs/build.mjs). |
| [`firmware/`](firmware/) | Production operator image (`tdisplay_s3_prod`). Source, PlatformIO env, and the shipped [`.bin`](firmware/images/tdisplay_s3_prod.bin). |
| [`schematics/`](schematics/) | Service drawings. Board A: [`schematics/Board_A.svg`](schematics/Board_A.svg) (`gen_board_a.py`). Board B: [`schematics/Board_B.svg`](schematics/Board_B.svg) (`gen_board_b.py`). Board C: [`schematics/Board_C.svg`](schematics/Board_C.svg) (`gen_board_c.py`). Manufacturer datasheets for Boards A, B, and C are in [`schematics/datasheets/`](schematics/datasheets/). |
| [`models/`](models/) | Printable `.3mf` bodies. Sensor puck, rotary-encoder cover, Board A Perma-Proto standoff, ACS712 mount. See [`models/README.md`](models/README.md). |


## Operator docs

Index: [`docs/README.md`](docs/README.md).

| Guide | HTML | PDF |
|-------|------|-----|
| Software User Manual | [`docs/html/software-manual.html`](docs/html/software-manual.html) | [`docs/pdf/Software_User_Manual.pdf`](docs/pdf/Software_User_Manual.pdf) |
| Wiring Guide | [`docs/html/wiring-guide.html`](docs/html/wiring-guide.html) | [`docs/pdf/Wiring_Guide.pdf`](docs/pdf/Wiring_Guide.pdf) |


Software chapters: [Overview](docs/html/index.html), [Main](docs/html/main.html), [Calibration](docs/html/calibration.html), [Diagnostics](docs/html/diagnostics.html), [Settings](docs/html/settings.html).

Wiring chapters: [Board A](docs/html/board-a/index.html), [Board B](docs/html/board-b/index.html), [Board C](docs/html/board-c/index.html).

Rebuild from [`docs/`](docs/):

```bash
cd docs
npm install
./node_modules/.bin/playwright install chromium
npm run build
```

HTML in [`docs/html/`](docs/html/) is the source. The build reprints the chapter PDFs and the two bound guides.

## Firmware

See [`firmware/README.md`](firmware/README.md). The last published binary is [`firmware/images/tdisplay_s3_prod.bin`](firmware/images/tdisplay_s3_prod.bin).

## Secrets

Station (house) Wi-Fi credentials do not belong in this repository. Fallback SoftAP name and password are public and live in [`firmware/src/wifi_secrets.h`](firmware/src/wifi_secrets.h): `PrivateReserve` / `reserved`. They are also on the LCD and in the operator docs. ArduinoOTA auth is `reserve-ota` in that same file.
