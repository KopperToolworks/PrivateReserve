#!/usr/bin/env python3
"""Generate Board_A.svg — earth-side T-Display-S3 service schematic."""

from pathlib import Path

OUT = Path(__file__).with_name("Board_A.svg")

# Columns: T1/T13 … T12/T24, left → right, USB-C nearest T1/T13
X0 = 102
PITCH = 108
XS = [X0 + i * PITCH for i in range(12)]

ON_X, ON_Y, ON_W = 40, 162, 1620
OFF_X, OFF_W = 1684, 556
BOX_GAP = 18

# Terminal strips
TOP_Y, TOP_H = 230, 86
BOT_Y, BOT_H = 1188, 92

# T-Display body (pins sit on the long edges)
DISP_X = XS[0] - 30
DISP_W = XS[-1] - XS[0] + 60
DISP_Y = 496
DISP_H = 260
Y_TOP_PIN = DISP_Y
Y_BOT_PIN = DISP_Y + DISP_H

Y33 = 340
YGND = 1096

LOWER_Y = Y_BOT_PIN + 12
LOWER_H = 612
ON_H = LOWER_Y + LOWER_H + BOX_GAP - ON_Y
W, H = 2280, ON_Y + ON_H + 80

C_V5 = "#b42318"
C_V33 = "#dc6803"
C_GND = "#101828"
C_IO = "#175cd3"
C_ENC = "#7f56d9"
C_NC = "#98a2b3"
C_INK = "#222"

KIND = {
    "v5": (C_V5, "v5"),
    "v33": (C_V33, "v33"),
    "gnd": (C_GND, "gnd"),
    "io": (C_IO, "earth"),
    "enc": (C_ENC, "enc"),
    "nc": (C_NC, "nc"),
}


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Svg:
    def __init__(self):
        self.b = []

    def add(self, s):
        self.b.append(s)

    def line(self, x1, y1, x2, y2, cls):
        self.add(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{cls}"/>'
        )

    def path(self, d, cls):
        self.add(f'<path d="{d}" class="{cls}"/>')

    def circle(self, x, y, r, fill):
        self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}"/>')

    def text(self, x, y, cls, t, extra=""):
        self.add(
            f'<text x="{x}" y="{y}" class="{cls}"{extra}>{esc(t)}</text>'
        )

    def mid(self, x, y, cls, t):
        self.text(x, y, cls, t, ' text-anchor="middle"')

    def rect(self, x, y, w, h, cls, rx=5):
        self.add(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
            f'rx="{rx}" class="{cls}"/>'
        )

    def stroke(self, x1, y1, x2, y2, color, sw=3):
        self.add(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
            f'stroke="{color}" stroke-width="{sw}" '
            f'stroke-linecap="round"/>'
        )

    def res_v(self, x, y, w, h, value):
        self.rect(x - w / 2, y, w, h, "part", rx=2)
        self.mid(x, y + h / 2 + 4, "pin", value)

    def hop_h(self, y, x0, x1, hop_xs, cls, color, r=8):
        hops = sorted(h for h in hop_xs if min(x0, x1) < h < max(x0, x1))
        step = 1 if x1 >= x0 else -1
        cur = x0
        for hx in hops if x1 >= x0 else reversed(hops):
            self.line(cur, y, hx - step * r, y, cls)
            x_a, x_b = hx - r, hx + r
            sweep = 1 if x1 >= x0 else 0
            self.add(
                f'<path d="M{x_a} {y} A{r} {r} 0 0 {sweep} {x_b} {y}" '
                f'fill="none" stroke="{color}" stroke-width="3"/>'
            )
            cur = hx + step * r
        self.line(cur, y, x1, y, cls)

    def hop_v(self, x, y0, y1, hop_ys, cls, color, r=8):
        hops = sorted(h for h in hop_ys if min(y0, y1) < h < max(y0, y1))
        step = 1 if y1 >= y0 else -1
        cur = y0
        for hy in hops if y1 >= y0 else reversed(hops):
            self.line(x, cur, x, hy - step * r, cls)
            y_a, y_b = hy - r, hy + r
            sweep = 1 if y1 >= y0 else 0
            self.add(
                f'<path d="M{x} {y_a} A{r} {r} 0 0 {sweep} {x} {y_b}" '
                f'fill="none" stroke="{color}" stroke-width="3"/>'
            )
            cur = hy + step * r
        self.line(x, cur, x, y1, cls)

    def out(self):
        return "\n".join(self.b)


# RIGHT header, pin 12 nearest USB (left). Display-up top edge.
TOP = [
    dict(t="T1", lab="3V", gpio="+3V3", kind="v33", dest="sensor VCC",
         pin=12, hdr="3V"),
    dict(t="T2", lab="GND", gpio="MCU_GND", kind="gnd", dest="returns",
         pin=11, hdr="GND"),
    dict(t="T3", lab="GND", gpio="MCU_GND", kind="gnd", dest="returns",
         pin=10, hdr="GND"),
    dict(t="T4", lab="NC", gpio="—", kind="nc", dest="unassigned",
         pin=9, hdr="NC", to_term=False),
    dict(t="T5", lab="PWM", gpio="GPIO16", kind="io", dest="→ B T4",
         pin=8, hdr="16"),
    dict(t="T6", lab="DIR", gpio="GPIO21", kind="io", dest="→ B T8",
         pin=7, hdr="21"),
    dict(t="T7", lab="LIM-U", gpio="GPIO17", kind="io", dest="upper NC",
         pin=6, hdr="17"),
    dict(t="T8", lab="CLOSE", gpio="GPIO18", kind="io", dest="rocker",
         pin=5, hdr="18"),
    dict(t="T9", lab="SCL", gpio="GPIO44", kind="io", dest="AS5600",
         pin=4, hdr="44"),
    dict(t="T10", lab="SDA", gpio="GPIO43", kind="io", dest="AS5600",
         pin=3, hdr="43"),
    dict(t="T11", lab="GND", gpio="MCU_GND", kind="gnd", dest="returns",
         pin=2, hdr="GND"),
    dict(t="T12", lab="GND", gpio="MCU_GND", kind="gnd", dest="→ B T5",
         pin=1, hdr="GND"),
]

# LEFT header, pin 12 nearest USB (left). Display-up bottom edge.
BOT = [
    dict(t="T13", lab="5V", gpio="+5V_EARTH", kind="v5", dest="HDR #1 +",
         pin=12, hdr="5V", to_term=True),
    dict(t="T14", lab="GND", gpio="MCU_GND", kind="gnd", dest="HDR #1 −",
         pin=11, hdr="GND", to_term=True),
    dict(t="T15", lab="5V", gpio="+5V_EARTH", kind="v5", dest="ACS712 VCC",
         pin=10, hdr="NC", hdr_kind="nc", to_term=False),
    dict(t="T16", lab="GND", gpio="MCU_GND", kind="gnd", dest="returns",
         pin=9, hdr="NC", hdr_kind="nc", to_term=False),
    dict(t="T17", lab="GND", gpio="MCU_GND", kind="gnd", dest="returns",
         pin=8, hdr="13", hdr_kind="enc", to_term=False),
    dict(t="T18", lab="GND", gpio="MCU_GND", kind="gnd", dest="returns",
         pin=7, hdr="12", hdr_kind="enc", to_term=False),
    dict(t="T19", lab="NC", gpio="GPIO11", kind="nc", dest="EC11 CLK on-bd",
         pin=6, hdr="11", hdr_kind="enc", to_term=False),
    dict(t="T20", lab="OPEN", gpio="GPIO10", kind="io", dest="rocker",
         pin=5, hdr="10", to_term=True),
    dict(t="T21", lab="LIM-L", gpio="GPIO3", kind="io", dest="lower NC",
         pin=4, hdr="3", to_term=True),
    dict(t="T22", lab="ISENSE", gpio="GPIO2", kind="io", dest="ACS712 OUT",
         pin=3, hdr="2", to_term=True),
    dict(t="T23", lab="HALL", gpio="GPIO1", kind="io", dest="SS49E OUT",
         pin=2, hdr="1", to_term=True),
    dict(t="T24", lab="3V", gpio="+3V3", kind="v33", dest="sensor VCC",
         pin=1, hdr="3V", to_term=True),
]


def build():
    s = Svg()
    a = s.add

    a(f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"
     viewBox="0 0 {W} {H}" version="1.1" role="img"
     aria-labelledby="title desc">
  <title id="title">Board A — Earth-side control</title>
  <desc id="desc">Service schematic for the Private Reserve Board A
    dual Perma-Proto 1/2. LilyGo T-Display-S3, on-board EC11, screw
    terminals T1–T24. Earth domain only.
  </desc>
  <defs>
    <style>
      .wire {{ fill:none; stroke:#222; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .earth {{ fill:none; stroke:#175cd3; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .gnd {{ fill:none; stroke:#101828; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .v5 {{ fill:none; stroke:#b42318; stroke-width:4; stroke-linecap:round; stroke-linejoin:round }}
      .v33 {{ fill:none; stroke:#dc6803; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .enc {{ fill:none; stroke:#7f56d9; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .nc {{ fill:none; stroke:#98a2b3; stroke-width:2; stroke-dasharray:6 4; stroke-linecap:round }}
      .box {{ fill:#fff; stroke:#344054; stroke-width:2 }}
      .part {{ fill:#f9fafb; stroke:#344054; stroke-width:2 }}
      .onboard {{ fill:#fff; stroke:#344054; stroke-width:2.4 }}
      .earthbox {{ fill:#eff8ff; stroke:#175cd3; stroke-width:1.6 }}
      .off {{ fill:#f9fafb; stroke:#667085; stroke-width:2; stroke-dasharray:8 5 }}
      .warn {{ fill:#fffaeb; stroke:#dc6803; stroke-width:2 }}
      .termbox {{ fill:#fff; stroke:#344054; stroke-width:2 }}
      .termnc {{ fill:#f9fafb; stroke:#98a2b3; stroke-width:1.6; stroke-dasharray:5 3 }}
      .callout {{ fill:#fffaeb; stroke:#dc6803; stroke-width:1.6; stroke-dasharray:5 3 }}
      .lcd {{ fill:#1d2939; stroke:#344054; stroke-width:1.6 }}
      .title {{ font:700 26px sans-serif; fill:#101828 }}
      .section {{ font:700 16px sans-serif; fill:#101828 }}
      .label {{ font:600 15px sans-serif; fill:#101828 }}
      .text {{ font:14px sans-serif; fill:#344054 }}
      .small {{ font:12px sans-serif; fill:#475467 }}
      .pin {{ font:12px monospace; fill:#344054 }}
      .lcdtitle {{ font:700 16px sans-serif; fill:#f2f4f7 }}
      .lcdsub {{ font:11px sans-serif; fill:#d0d5dd }}
      .v5text {{ font:700 13px sans-serif; fill:#b42318 }}
      .v33text {{ font:700 13px sans-serif; fill:#dc6803 }}
      .gndtext {{ font:700 13px sans-serif; fill:#101828 }}
      .earthtext {{ font:700 13px sans-serif; fill:#175cd3 }}
      .enctext {{ font:700 12px sans-serif; fill:#6941c6 }}
      .nctext {{ font:700 12px sans-serif; fill:#667085 }}
    </style>
  </defs>''')

    s.rect(0, 0, W, H, "box", rx=0)
    a(f'<rect width="{W}" height="{H}" fill="#fff"/>')

    s.text(40, 40, "title", "Board A — Earth-side control")
    s.text(
        40,
        64,
        "text",
        "Private Reserve · as-built dual Adafruit Perma-Proto 1/2 · "
        "LilyGo T-Display-S3 + EC11 · T1–T24 · Rev 2026-08-22",
    )

    s.rect(40, 78, 2200, 68, "warn")
    s.text(56, 102, "label",
           "Earth domain only. Never land floating or HV copper here.")
    s.text(
        56,
        124,
        "text",
        "No +5V_ISO, +12V_GD, −70V, +70V, or SW. MCU_GND is not −70V. "
        "Do not back-feed 3.3 V into the module 3V pin. LED resistors stay "
        "on Board B.",
    )

    # ---- ON BOARD ----
    s.rect(ON_X, ON_Y, ON_W, ON_H, "onboard", rx=8)
    s.text(56, 186, "section", "BOARD A · on-board")
    s.text(
        280,
        186,
        "small",
        "Display facing you · USB-C left · EC11 right · header pin 12 "
        "nearest USB · upper outer rails unused · +5V_EARTH is not a proto + bus",
    )

    # Proto halves (light)
    s.rect(54, 198, 1548, DISP_Y - 198 - 8, "earthbox", rx=6)
    s.text(66, 216, "earthtext", "UPPER PERMA-PROTO 1/2")
    s.text(280, 216, "small",
           "row A takes the T-Display RIGHT header · top edge T1–T12 · pin 12 nearest USB")

    s.rect(54, LOWER_Y, 1548, LOWER_H, "earthbox", rx=6)
    s.text(66, Y_BOT_PIN + 32, "earthtext", "LOWER PERMA-PROTO 1/2")
    s.text(280, Y_BOT_PIN + 32, "small",
           "row J takes the T-Display LEFT header · bottom edge T13–T24 · pin 12 nearest USB")

    # ---- Top terminal strip T1–T12 ----
    strip_x = XS[0] - PITCH / 2
    strip_w = 12 * PITCH
    s.rect(strip_x, TOP_Y, strip_w, TOP_H, "termbox", rx=4)

    for i, p in enumerate(TOP):
        x = XS[i]
        cell_x = x - PITCH / 2
        color, cls = KIND[p["kind"]]
        if i:
            s.stroke(cell_x, TOP_Y, cell_x, TOP_Y + TOP_H, "#d0d5dd", 1.2)
        s.mid(x, TOP_Y + 20, "pin", p["t"])
        s.mid(x, TOP_Y + 38, "label", p["lab"])
        s.mid(x, TOP_Y + 54, "small", p["gpio"])
        s.mid(x, TOP_Y + 70, "small", p["dest"])
        cy = TOP_Y + TOP_H
        s.circle(x, cy, 4.5, color)

    # +3V3 rail (proposed inner)
    s.line(XS[0], Y33, XS[-1], Y33, "v33")
    s.circle(XS[0], Y33, 5, C_V33)
    s.text(XS[0] + 10, Y33 - 10, "v33text",
           "+3V3_TDISPLAY   module 3V · sensors · EC11 +   ·   do not back-feed")

    # ---- T-Display-S3 ----
    s.rect(DISP_X, DISP_Y, DISP_W, DISP_H, "part", rx=8)

    # USB-C on the left
    s.rect(DISP_X - 36, DISP_Y + 96, 36, 56, "part", rx=4)
    s.mid(DISP_X - 18, DISP_Y + 128, "small", "USB")

    # LCD
    lcd_x, lcd_y, lcd_w, lcd_h = DISP_X + 168, DISP_Y + 44, 720, 168
    s.rect(lcd_x, lcd_y, lcd_w, lcd_h, "lcd", rx=4)
    s.text(lcd_x + 20, lcd_y + 36, "lcdtitle", "T-Display-S3")
    s.text(lcd_x + 20, lcd_y + 58, "lcdsub", "LilyGo · ESP32-S3 · ST7789 320×170")
    s.text(lcd_x + 20, lcd_y + 80, "lcdsub",
           "setRotation(3) · origin is operator top-left")
    s.text(lcd_x + 20, lcd_y + 110, "lcdsub",
           "GPIO14  on-board button   ·   GPIO0  BOOT")
    s.text(lcd_x + 20, lcd_y + 128, "lcdsub",
           "LCD / backlight GPIOs reserved — do not reassign")
    s.text(lcd_x + 20, lcd_y + 154, "lcdsub",
           "RIGHT header on top · LEFT header on bottom · pin 12 nearest USB")

    # Buttons near USB
    s.rect(DISP_X + 18, DISP_Y + 88, 52, 28, "part", rx=3)
    s.mid(DISP_X + 44, DISP_Y + 106, "small", "BOOT")
    s.rect(DISP_X + 18, DISP_Y + 126, 52, 28, "part", rx=3)
    s.mid(DISP_X + 44, DISP_Y + 144, "small", "GPIO14")

    s.text(DISP_X + 18, DISP_Y + 196, "small", "LILYGO")

    # Top header pins (on the module edge)
    s.text(DISP_X + 8, DISP_Y - 18, "small", "RIGHT")
    for i, p in enumerate(TOP):
        x = XS[i]
        color, cls = KIND[p["kind"]]
        s.circle(x, Y_TOP_PIN, 5.5, color)
        s.mid(x, Y_TOP_PIN + 22, "pin", p["hdr"])
        s.mid(x - 12, DISP_Y - 8, "nctext" if p["kind"] == "nc" else "small",
              f"p{p['pin']}")

    # Bottom header pins
    s.text(DISP_X + 8, Y_BOT_PIN + 32, "small", "LEFT")
    for i, p in enumerate(BOT):
        x = XS[i]
        hdr_kind = p.get("hdr_kind", p["kind"])
        color, cls = KIND[hdr_kind]
        s.circle(x, Y_BOT_PIN, 5.5, color)
        s.mid(x, Y_BOT_PIN - 12, "pin", p["hdr"])
        s.mid(x - 12, Y_BOT_PIN + 22, "nctext" if hdr_kind == "nc" else "small",
              f"p{p['pin']}")

    # Wires: top strip → top pins. Only T1 joins +3V3; others hop that rail.
    for i, p in enumerate(TOP):
        x = XS[i]
        color, cls = KIND[p["kind"]]
        y0 = TOP_Y + TOP_H
        if not p.get("to_term", True):
            continue
        if p["kind"] == "v33":
            s.line(x, y0, x, Y33, "v33")
            s.circle(x, Y33, 4, C_V33)
            s.line(x, Y33, x, Y_TOP_PIN, "v33")
        elif p["kind"] == "nc":
            s.hop_v(x, y0, Y_TOP_PIN, [Y33], "nc", C_NC)
        else:
            s.hop_v(x, y0, Y_TOP_PIN, [Y33], cls, color)
        if p["kind"] != "nc":
            s.circle(x, Y_TOP_PIN, 4, color)

    # R7 10 kΩ DIR pull-down (on the DIR column)
    x_dir = XS[5]
    x_r7 = XS[2] + 20
    y_tap = 368
    y_r7 = 404
    s.circle(x_dir, y_tap, 4, C_IO)
    s.line(x_dir, y_tap, x_r7, y_tap, "earth")
    s.line(x_r7, y_tap, x_r7, y_r7, "earth")
    s.circle(x_r7, y_tap, 4, C_IO)
    s.res_v(x_r7, y_r7, 28, 52, "10k")
    y_r7_gnd = DISP_Y - 22
    s.line(x_r7, y_r7 + 52, x_r7, y_r7_gnd, "gnd")
    s.line(x_r7, y_r7_gnd, XS[2], y_r7_gnd, "gnd")
    s.circle(XS[2], y_r7_gnd, 4, C_GND)
    s.text(x_r7 + 20, y_r7 + 24, "label", "R7")
    s.text(x_r7 + 20, y_r7 + 40, "small", "DIR pull-down")
    s.text(x_r7 + 20, y_r7 + 56, "small", "holds B LED off")

    # ---- EC11 (right of the module) ----
    ex, ey, ew, eh = 1368, 508, 268, 236
    s.rect(ex, ey, ew, eh, "part", rx=6)
    s.text(ex + 12, ey + 22, "label", "EC11  menu encoder")
    s.text(ex + 12, ey + 40, "small", "on-board · not on T17–T19")
    s.circle(ex + 48, ey + 88, 22, "#101828")
    a(f'<circle cx="{ex + 48}" cy="{ey + 88}" r="22" fill="none" '
      f'stroke="#667085" stroke-width="2"/>')
    s.text(ex + 80, ey + 72, "pin", "CLK   GPIO11")
    s.text(ex + 80, ey + 90, "pin", "DT    GPIO12")
    s.text(ex + 80, ey + 108, "pin", "SW    GPIO13")
    s.text(ex + 80, ey + 132, "pin", "+     +3V3")
    s.text(ex + 80, ey + 150, "pin", "GND   MCU_GND")
    s.text(ex + 12, ey + 180, "small", "3.3 V pull-ups on the breakout.")
    s.text(ex + 12, ey + 196, "small", "Never pull CLK / DT / SW to 5 V.")
    s.text(ex + 12, ey + 220, "enctext", "Short leads to LEFT header")

    # EC11 signal taps just under the LEFT header pins (GPIO13/12/11)
    enc_pins = [
        (4, 788, "SW"),
        (5, 808, "DT"),
        (6, 828, "CLK"),
    ]
    hop_xs = XS[7:]  # hop OPEN / LIM-L / ISENSE / HALL / 3V
    enc_end = ex + 8
    for idx, y, name in enc_pins:
        x = XS[idx]
        s.line(x, Y_BOT_PIN, x, y, "enc")
        s.circle(x, Y_BOT_PIN, 4, C_ENC)
        s.circle(x, y, 4, C_ENC)
        s.hop_h(y, x, enc_end, hop_xs, "enc", C_ENC)
        s.circle(enc_end, y, 4, C_ENC)
        s.text(enc_end + 8, y + 4, "enctext", name)

    # EC11 + from +3V3 (T24 / bottom 3V pin) and GND from rail
    x3 = XS[11]
    s.line(x3, Y_BOT_PIN, x3, 848, "v33")
    s.circle(x3, 848, 4, C_V33)
    s.line(x3, 848, enc_end, 848, "v33")
    s.circle(enc_end, 848, 4, C_V33)
    s.text(enc_end + 8, 852, "v33text", "+")

    s.line(enc_end, 868, enc_end, YGND, "gnd")
    s.circle(enc_end, 868, 4, C_GND)
    s.circle(enc_end, YGND, 5, C_GND)
    s.text(enc_end + 8, 872, "gndtext", "GND")

    # +3V3 riser to the right of the module (T24 / EC11 +, not through the LCD)
    xr33 = 1344
    s.line(XS[-1], Y33, xr33, Y33, "v33")
    s.line(xr33, Y33, xr33, Y_BOT_PIN, "v33")
    s.line(xr33, Y_BOT_PIN, XS[11], Y_BOT_PIN, "v33")
    s.circle(xr33, Y33, 4, C_V33)
    s.circle(xr33, Y_BOT_PIN, 4, C_V33)

    # GND rail — header GNDs are common on the module; proto rail is T14 / R7 / EC11
    s.line(XS[1], YGND, enc_end, YGND, "gnd")
    s.circle(XS[1], YGND, 5, C_GND)
    s.text(XS[1] + 10, YGND - 12, "gndtext",
           "MCU_GND   T2 T3 T11 T12 T14 T16–T18 · sensor / rocker / Board B LED return   ·   not −70V")

    # Bottom header → terminals. Signals hop MCU_GND; T14 joins it.
    for i, p in enumerate(BOT):
        x = XS[i]
        color, cls = KIND[p["kind"]]
        y1 = BOT_Y
        if not p.get("to_term", True):
            continue
        if p["kind"] == "nc":
            s.hop_v(x, Y_BOT_PIN, y1, [YGND], "nc", C_NC)
            continue
        if p["kind"] == "v5":
            s.line(x, Y_BOT_PIN, x, y1, "v5")
            s.circle(x, Y_BOT_PIN, 4, C_V5)
            continue
        if p["kind"] == "v33":
            s.hop_v(x, Y_BOT_PIN, y1, [YGND], "v33", C_V33)
            s.circle(x, Y_BOT_PIN, 4, C_V33)
            continue
        if p["kind"] == "gnd":
            s.line(x, Y_BOT_PIN, x, YGND, "gnd")
            s.circle(x, YGND, 4, C_GND)
            s.line(x, YGND, x, y1, "gnd")
            continue
        if p["t"] == "T22":
            # ACS712 10k / 20k divider on the ISENSE column
            yj = 960
            s.line(x, Y_BOT_PIN, x, yj, "earth")
            s.circle(x, Y_BOT_PIN, 4, C_IO)
            s.circle(x, yj, 4, C_IO)
            s.line(x, yj, x, yj + 8, "earth")
            s.res_v(x, yj + 8, 28, 48, "10k")
            s.hop_v(x, yj + 56, y1, [YGND], "earth", C_IO)
            s.line(x, yj, x - 40, yj, "earth")
            s.line(x - 40, yj, x - 40, yj + 8, "earth")
            s.res_v(x - 40, yj + 8, 28, 52, "20k")
            s.line(x - 40, yj + 60, x - 40, YGND, "gnd")
            s.circle(x - 40, YGND, 4, C_GND)
            s.text(x + 18, yj + 28, "label", "10k/20k")
            s.text(x + 18, yj + 44, "small", "ACS712 ≤ 3.3 V")
            continue
        s.hop_v(x, Y_BOT_PIN, y1, [YGND], cls, color)
        s.circle(x, Y_BOT_PIN, 4, color)

    # T13–T15 +5V bridge. LEFT p10 stays NC on the module; T15 is not that pin.
    y_br = BOT_Y - 22
    s.line(XS[0], BOT_Y, XS[0], y_br, "v5")
    s.hop_h(y_br, XS[0], XS[2], [XS[1]], "v5", C_V5)
    s.line(XS[2], y_br, XS[2], BOT_Y, "v5")
    s.circle(XS[0], y_br, 4, C_V5)
    s.circle(XS[2], y_br, 4, C_V5)
    s.text(XS[1] + 16, y_br - 16, "v5text", "T13–T15  +5V bridge")

    # T16–T18 sensor GND returns land on the MCU_GND rail (same net as T14).
    for i in (3, 4, 5):
        s.line(XS[i], YGND, XS[i], BOT_Y, "gnd")
        s.circle(XS[i], YGND, 4, C_GND)

    # ---- Bottom terminal strip T13–T24 ----
    s.rect(strip_x, BOT_Y, strip_w, BOT_H, "termbox", rx=4)

    for i, p in enumerate(BOT):
        x = XS[i]
        cell_x = x - PITCH / 2
        color, _ = KIND[p["kind"]]
        if i:
            s.stroke(cell_x, BOT_Y, cell_x, BOT_Y + BOT_H, "#d0d5dd", 1.2)
        if p["kind"] == "nc":
            s.rect(cell_x + 2, BOT_Y + 2, PITCH - 4, BOT_H - 4, "termnc", rx=3)
        s.mid(x, BOT_Y + 20, "pin", p["t"])
        s.mid(x, BOT_Y + 38, "label", p["lab"])
        s.mid(x, BOT_Y + 54, "small", p["gpio"])
        s.mid(x, BOT_Y + 70, "small", p["dest"])
        s.circle(x, BOT_Y, 4.5, color)

    s.text(
        66,
        1304,
        "small",
        "As-built screws, USB-C left → right.  Top:  T1 3V · T2 GND · T3 GND · "
        "T4 NC · T5 PWM · T6 DIR · T7 LIM-U · T8 CLOSE · T9 SCL · T10 SDA · "
        "T11 GND · T12 GND",
    )
    s.text(
        66,
        1322,
        "small",
        "Bottom:  T13 5V · T14 GND · T15 5V (← T13) · T16–T18 GND (← T14) · "
        "T19 NC (EC11 CLK on header) · T20 OPEN · T21 LIM-L · T22 ISENSE · "
        "T23 HALL · T24 3V",
    )
    s.text(
        66,
        1348,
        "small",
        "3-pin earth cable to Board B:  T5 PWM · T6 DIR · T12 MCU_GND.  "
        "No floating supply on that cable.  R7 is on this board; 220 Ω / "
        "270 Ω LED resistors are on Board B.",
    )
    s.text(
        66,
        1366,
        "small",
        "Upper outer +/− unused.  Lower outer + unused.  Lower outer − is "
        "MCU_GND.  Inner +3V3 / GND buses are proposed.",
    )

    # ---- OFF BOARD ----
    s.rect(OFF_X, ON_Y, OFF_W, ON_H, "off", rx=8)
    tx = OFF_X + 16
    s.text(tx, 186, "section", "OFF BOARD")
    s.text(tx, 204, "small", "Dashed outline. Not on Board A copper.")

    s.rect(tx, 220, 524, 108, "box")
    s.text(tx + 12, 244, "label", "HDR-15-5 #1  ·  +5V_EARTH / MCU_GND")
    s.text(tx + 12, 268, "pin", "+  →  T13  5V      −  →  T14  GND")
    s.text(tx + 12, 288, "small", "T13→T15 +5V.  T14→T16–T18 GND.")
    s.text(tx + 12, 306, "small", "USB-C remains for bench programming.")

    s.rect(tx, 344, 524, 148, "box")
    s.text(tx + 12, 368, "label", "Board B · floating gate driver")
    s.text(tx + 12, 392, "pin", "T5   PWM     GPIO16  →  Board B T4")
    s.text(tx + 12, 410, "pin", "T6   DIR  GPIO21  →  Board B T8")
    s.text(tx + 12, 428, "pin", "T12  MCU_GND          →  Board B T5")
    s.text(tx + 12, 450, "small", "Keyed 3-pin earth cable. No +5V_ISO / −70V.")
    s.text(tx + 12, 468, "small", "R7 keeps DIR low if A is dark or resetting.")
    s.text(tx + 12, 486, "small", "See schematics/Board_B.svg")

    s.rect(tx, 508, 524, 292, "box")
    s.text(tx + 12, 532, "label", "Field I/O  (earth only)")
    s.text(tx + 12, 556, "pin", "T7   LIM-U   GPIO17   upper NC → GND")
    s.text(tx + 12, 574, "pin", "T21  LIM-L   GPIO3    lower NC → GND")
    s.text(tx + 12, 592, "small", "INPUT_PULLUP. HIGH = marker or open wire.")
    s.text(tx + 12, 616, "pin", "T20  OPEN    GPIO10   rocker (active-low)")
    s.text(tx + 12, 634, "pin", "T8   CLOSE   GPIO18   rocker (active-low)")
    s.text(tx + 12, 652, "small", "Dual paralleled (ON)-OFF-(ON) stations.")
    s.text(tx + 12, 676, "pin", "T9 / T10    GPIO44 / 43    AS5600 SCL / SDA")
    s.text(tx + 12, 694, "pin", "T23  HALL    GPIO1     SS49E OUT")
    s.text(tx + 12, 712, "pin", "T1 / T24    +3V3       sensor VCC")
    s.text(tx + 12, 730, "small", "AS5600 at 0x36. SS49E is the bottle key,")
    s.text(tx + 12, 746, "small", "not travel. Returns on T2 / T3 / T11 / T14 / T16–T18.")
    s.text(tx + 12, 770, "pin", "T22  ISENSE  GPIO2     ACS712-05B OUT")
    s.text(tx + 12, 788, "small", "Module is DIN-mounted. 10k/20k lives here.")

    s.rect(tx, 816, 524, 100, "box")
    s.text(tx + 12, 840, "label", "ACS712-05B  ·  DIN HV island")
    s.text(tx + 12, 864, "small", "Current path is fused +70V, not on Board A.")
    s.text(tx + 12, 882, "small", "VCC ← T15  (T13–T15 +5V bridge).  GND ← T14.")
    s.text(tx + 12, 900, "small", "OUT → T22 → 10k / 20k → GPIO2  (185 mV/A).")

    s.rect(tx, 932, 524, 88, "callout")
    s.text(tx + 12, 956, "label", "Not on Board A")
    s.text(tx + 12, 976, "small", "U1 6N137 · U3 TC4426 · U4 4N35 · Q1–Q3")
    s.text(tx + 12, 994, "small", "+70V · −70V · SW · +5V_ISO · +12V_GD · K1")
    s.text(tx + 12, 1012, "small", "See schematics/Board_B.svg and Board_C.svg")

    s.rect(tx, 1036, 524, 320, "part")
    s.text(tx + 12, 1064, "label", "Private Reserve")
    s.text(tx + 12, 1088, "text", "DWG  schematics/Board_A.svg")
    s.text(tx + 12, 1110, "text", "REV  2026-08-22 · earth control")
    s.text(tx + 12, 1142, "small", "Datasheets:")
    s.text(tx + 12, 1162, "small", "T-Display-S3 · ESP32-S3")
    s.text(tx + 12, 1180, "small", "EC11 · ACS712-05B")
    s.text(tx + 12, 1198, "small", "AS5600 · SS49E")
    s.text(tx + 12, 1216, "small", "HDR-15-5 · SZL-WL")
    s.text(tx + 260, 1162, "small", "GPIO14 is the module")
    s.text(tx + 260, 1180, "small", "button, not a header pin.")
    s.text(tx + 260, 1198, "small", "T19 stays NC (EC11 CLK).")
    s.text(tx + 260, 1216, "small", "No 555 / 74LS157.")

    s.text(
        40,
        ON_Y + ON_H + 28,
        "small",
        "GPIO16 and GPIO21 must be LOW at boot. PWM-off, wait for current "
        "decay, then change K1, wait ≥25 ms, then resume PWM. Loss of "
        "+5V_EARTH extinguishes the Board B LEDs → gate off.",
    )

    a("</svg>")
    return s.out()


def main():
    svg = build()
    OUT.write_text(svg)
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
