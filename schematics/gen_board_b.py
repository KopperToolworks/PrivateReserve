#!/usr/bin/env python3
"""Generate Board_B.svg — floating gate-driver service schematic."""

from pathlib import Path

OUT = Path(__file__).with_name("Board_B.svg")

W, H = 2280, 1420

# Rails — below the domain titles, with a gap between +12V and +5V
Y12, Y5, Y70 = 292, 356, 1296
RX0, RX1 = 390, 1510

# Earth GND (same Y as −70V, never joined)
YGND = 1296
GX0, GX1 = 70, 340

ISO_X = 356

# Signal rows
YPWM = 640
YDIR = 1000
Y_MID70 = 860

# Colors
C_EARTH = "#175cd3"
C_GND = "#101828"
C_12 = "#ca8504"
C_5 = "#c11574"
C_70 = "#78716c"
C_ISO = "#7f56d9"
C_INK = "#222"


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Svg:
    def __init__(self):
        self.b = []

    def add(self, s):
        self.b.append(s)

    def raw(self, s):
        self.add(s)

    def line(self, x1, y1, x2, y2, cls):
        self.add(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{cls}"/>'
        )

    def path(self, d, cls):
        self.add(f'<path d="{d}" class="{cls}"/>')

    def circle(self, x, y, r, fill):
        self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}"/>')

    def text(self, x, y, cls, t):
        self.add(f'<text x="{x}" y="{y}" class="{cls}">{esc(t)}</text>')

    def rect(self, x, y, w, h, cls, rx=5):
        self.add(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
            f'rx="{rx}" class="{cls}"/>'
        )

    def poly(self, pts, stroke, fill="none", sw=3):
        p = " ".join(f"{x},{y}" for x, y in pts)
        self.add(
            f'<polygon points="{p}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{sw}"/>'
        )

    def stroke(self, x1, y1, x2, y2, color, sw=3):
        self.add(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
            f'stroke="{color}" stroke-width="{sw}" '
            f'stroke-linecap="round"/>'
        )

    def res_h(self, x, y, w, h, value):
        self.rect(x, y - h / 2, w, h, "part", rx=2)
        self.text(x + 6, y + 4, "pin", value)

    def res_v(self, x, y, w, h, value):
        self.rect(x - w / 2, y, w, h, "part", rx=2)
        self.text(x - w / 2 + 3, y + h / 2 + 4, "pin", value)

    def cap_v(self, x, yt, yb, value):
        """Vertical capacitor. Plates between yt (top lead) and yb (bottom)."""
        mid = (yt + yb) / 2
        g = 5
        self.stroke(x, yt, x, mid - g, C_INK, 3)
        self.stroke(x - 12, mid - g, x + 12, mid - g, C_INK, 3)
        self.stroke(x - 12, mid + g, x + 12, mid + g, C_INK, 3)
        self.stroke(x, mid + g, x, yb, C_INK, 3)
        self.text(x + 14, mid + 4, "pin", value)

    def led_right(self, x, y, color):
        """Anode at left (x), cathode bar at x+36, center y."""
        self.poly(
            [(x, y - 16), (x, y + 16), (x + 28, y)],
            color,
        )
        self.stroke(x + 32, y - 16, x + 32, y + 16, color, 3)
        # light arrows
        self.stroke(x + 10, y - 28, x + 22, y - 40, color, 2)
        self.stroke(x + 22, y - 22, x + 34, y - 34, color, 2)

    def diode_v(self, x, yk, ya, color, cathode_up=True):
        """Vertical diode. Fixed symbol size; leads fill yk↔ya."""
        body = 28
        mid = (yk + ya) / 2
        if cathode_up:
            bar_y = mid - body / 2
            base_y = mid + body / 2
            tip_y = bar_y
            self.stroke(x, yk, x, bar_y, color, 3)
            self.stroke(x - 14, bar_y, x + 14, bar_y, color, 3)
            self.poly([(x - 12, base_y), (x + 12, base_y), (x, tip_y)], color)
            self.stroke(x, base_y, x, ya, color, 3)
        else:
            bar_y = mid + body / 2
            base_y = mid - body / 2
            tip_y = bar_y
            self.stroke(x, ya, x, base_y, color, 3)
            self.poly([(x - 12, base_y), (x + 12, base_y), (x, tip_y)], color)
            self.stroke(x - 14, bar_y, x + 14, bar_y, color, 3)
            self.stroke(x, bar_y, x, yk, color, 3)

    def npn(self, x, y):
        """NPN. (x,y) = device center. C up, E down, B left."""
        self.stroke(x + 10, y - 36, x + 10, y + 36, C_INK, 3)
        self.stroke(x + 10, y - 22, x + 28, y - 22, C_INK, 3)
        self.stroke(x + 10, y + 22, x + 28, y + 22, C_INK, 3)
        self.stroke(x + 28, y - 22, x + 28, y - 48, C_INK, 3)  # C
        self.stroke(x + 28, y + 22, x + 28, y + 48, C_INK, 3)  # E
        self.stroke(x - 28, y, x + 10, y, C_INK, 3)  # B
        # emitter arrow
        self.poly(
            [(x + 28, y + 48), (x + 22, y + 34), (x + 34, y + 34)],
            C_INK,
            fill=C_INK,
            sw=1,
        )
        return {
            "b": (x - 28, y),
            "c": (x + 28, y - 48),
            "e": (x + 28, y + 48),
        }

    def nmos(self, x, y):
        """IEEE-style N-MOSFET. Isolated gate, broken channel, S arrow in."""
        gx, ch = x + 6, x + 26
        self.stroke(x - 20, y, gx, y, C_INK, 3)  # G lead
        self.stroke(gx, y - 36, gx, y + 36, C_INK, 3)  # gate plate
        for yy in (y - 24, y, y + 24):
            self.stroke(gx + 8, yy, ch, yy, C_INK, 3)
        self.stroke(ch, y - 28, ch, y - 20, C_INK, 3)
        self.stroke(ch, y - 4, ch, y + 4, C_INK, 3)
        self.stroke(ch, y + 20, ch, y + 28, C_INK, 3)
        self.stroke(ch, y - 24, ch, y - 52, C_INK, 3)  # D
        self.stroke(ch, y + 24, ch, y + 52, C_INK, 3)  # S
        self.poly(
            [(ch, y + 24), (ch - 6, y + 38), (ch + 6, y + 38)],
            C_INK,
            fill=C_INK,
            sw=1,
        )
        return {
            "g": (x - 20, y),
            "d": (ch, y - 52),
            "s": (ch, y + 52),
        }

    def inverter(self, x, y, w=42):
        """Inverting buffer. Input (x,y), output x+w."""
        self.poly(
            [(x, y - 16), (x, y + 16), (x + w - 10, y)],
            C_INK,
        )
        self.circle(x + w - 5, y, 5, "#fff")
        self.add(
            f'<circle cx="{x + w - 5}" cy="{y}" r="5" fill="none" '
            f'stroke="{C_INK}" stroke-width="2.4"/>'
        )
        return x + w

    def hop_v(self, x, y0, y1, hop_ys, cls, color, r=8):
        """Vertical wire y0→y1 with semicircle hops at each hop_ys."""
        hops = sorted(h for h in hop_ys if min(y0, y1) < h < max(y0, y1))
        step = 1 if y1 >= y0 else -1
        cur = y0
        for hy in (hops if y1 >= y0 else reversed(hops)):
            self.line(x, cur, x, hy - step * r, cls)
            y_a, y_b = hy - r, hy + r
            if y1 >= y0:
                self.add(
                    f'<path d="M{x} {y_a} A{r} {r} 0 0 1 {x} {y_b}" '
                    f'fill="none" stroke="{color}" stroke-width="3"/>'
                )
            else:
                self.add(
                    f'<path d="M{x} {y_b} A{r} {r} 0 0 0 {x} {y_a}" '
                    f'fill="none" stroke="{color}" stroke-width="3"/>'
                )
            cur = hy + step * r
        self.line(x, cur, x, y1, cls)

    def hop_h(self, y, x0, x1, hop_xs, cls, color, r=8):
        """Horizontal wire x0→x1 with semicircle hops at each hop_xs."""
        hops = sorted(h for h in hop_xs if min(x0, x1) < h < max(x0, x1))
        step = 1 if x1 >= x0 else -1
        cur = x0
        for hx in hops if x1 >= x0 else reversed(hops):
            self.line(cur, y, hx - step * r, y, cls)
            x_a, x_b = hx - r, hx + r
            if x1 >= x0:
                # sweep 1 → bulge up (smaller y) in SVG coordinates
                self.add(
                    f'<path d="M{x_a} {y} A{r} {r} 0 0 1 {x_b} {y}" '
                    f'fill="none" stroke="{color}" stroke-width="3"/>'
                )
            else:
                self.add(
                    f'<path d="M{x_b} {y} A{r} {r} 0 0 0 {x_a} {y}" '
                    f'fill="none" stroke="{color}" stroke-width="3"/>'
                )
            cur = hx + step * r
        self.line(cur, y, x1, y, cls)

    def out(self):
        return "\n".join(self.b)


def build():
    s = Svg()
    a = s.add

    a(f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"
     viewBox="0 0 {W} {H}" version="1.1" role="img"
     aria-labelledby="title desc">
  <title id="title">Board B — Floating gate driver</title>
  <desc id="desc">Service schematic for the Private Reserve Board B
    Perma-Proto 1/2. Earth LED island and floating −70V domain. U1 6N137,
    Q2 PN2222A fail-off, U3 TC4426, U4 4N35, Q3 2N7000TA. T1–T11.
  </desc>
  <defs>
    <style>
      .wire {{ fill:none; stroke:#222; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .earth {{ fill:none; stroke:#175cd3; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .gnd {{ fill:none; stroke:#101828; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .v12 {{ fill:none; stroke:#ca8504; stroke-width:4; stroke-linecap:round; stroke-linejoin:round }}
      .iso5 {{ fill:none; stroke:#c11574; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .m70 {{ fill:none; stroke:#78716c; stroke-width:4; stroke-linecap:round; stroke-linejoin:round }}
      .iso {{ fill:none; stroke:#7f56d9; stroke-width:3; stroke-dasharray:8 6; stroke-linecap:round }}
      .box {{ fill:#fff; stroke:#344054; stroke-width:2 }}
      .part {{ fill:#f9fafb; stroke:#344054; stroke-width:2 }}
      .onboard {{ fill:#fff; stroke:#344054; stroke-width:2.4 }}
      .earthbox {{ fill:#eff8ff; stroke:#175cd3; stroke-width:2 }}
      .floatbox {{ fill:#faf8ff; stroke:#7f56d9; stroke-width:1.6 }}
      .off {{ fill:#f9fafb; stroke:#667085; stroke-width:2; stroke-dasharray:8 5 }}
      .warn {{ fill:#fffaeb; stroke:#dc6803; stroke-width:2 }}
      .termbox {{ fill:#fff; stroke:#344054; stroke-width:2 }}
      .callout {{ fill:#fffaeb; stroke:#dc6803; stroke-width:1.6; stroke-dasharray:5 3 }}
      .isocall {{ fill:#f4f3ff; stroke:#7f56d9; stroke-width:1.6; stroke-dasharray:5 3 }}
      .title {{ font:700 26px sans-serif; fill:#101828 }}
      .section {{ font:700 16px sans-serif; fill:#101828 }}
      .label {{ font:600 15px sans-serif; fill:#101828 }}
      .text {{ font:14px sans-serif; fill:#344054 }}
      .small {{ font:12px sans-serif; fill:#475467 }}
      .pin {{ font:12px monospace; fill:#344054 }}
      .earthtext {{ font:700 13px sans-serif; fill:#175cd3 }}
      .gndtext {{ font:700 13px sans-serif; fill:#101828 }}
      .v12text {{ font:700 14px sans-serif; fill:#ca8504 }}
      .iso5text {{ font:700 13px sans-serif; fill:#c11574 }}
      .m70text {{ font:700 14px sans-serif; fill:#78716c }}
      .isotext {{ font:700 12px sans-serif; fill:#6941c6 }}
    </style>
  </defs>''')

    s.rect(0, 0, W, H, "box", rx=0)
    # overwrite fill — class box has stroke; paint page white
    a(f'<rect width="{W}" height="{H}" fill="#fff"/>')

    s.text(40, 40, "title", "Board B — Floating gate driver")
    s.text(
        40,
        64,
        "text",
        "Private Reserve · as-built Adafruit Perma-Proto 1/2 (cols 1–30) · "
        "earth LED island + floating −70V domain · Rev 2026-08-22",
    )

    s.rect(40, 78, 2200, 68, "warn")
    s.text(56, 102, "label",
           "Two galvanic domains. Never join MCU_GND to −70V.")
    s.text(
        56,
        124,
        "text",
        "Cols 1–5 are earth (PWM, DIR, MCU_GND). Cols 6–30 float on −70V "
        "(+12V_GD, +5V_ISO). After the HDR negatives bond to −70V, both "
        "floating supplies are hazardous relative to earth. Isolated probe only.",
    )

    # ---- ON BOARD ----
    s.rect(40, 162, 1500, 1190, "onboard", rx=8)
    s.text(56, 186, "section", "BOARD B · on-board")
    s.text(
        280,
        186,
        "small",
        "U1 left, U4 right on the proto. Isolation barrier at column 5. "
        "No +70V, SW, or motor copper on this board.",
    )

    # Earth island
    s.rect(54, 198, 288, 1136, "earthbox", rx=6)
    s.text(66, 220, "earthtext", "EARTH LED ISLAND")
    s.text(66, 238, "small", "cols 1–5 · MCU_GND only")

    # Floating wash (right of barrier)
    s.rect(372, 198, 1152, 1136, "floatbox", rx=6)
    s.text(388, 220, "isotext", "FLOATING REGION")
    s.text(388, 238, "small", "cols 6–30 · local ref = −70V  ·  not MCU_GND")

    # Isolation barrier
    s.line(ISO_X, 198, ISO_X, 1334, "iso")
    s.text(66, 840, "isotext", "ISOLATION BARRIER")
    s.text(66, 856, "small", "no copper across col 5")

    # Floating rails sit below the domain titles
    s.line(RX0, Y12, RX1, Y12, "v12")
    s.text(RX0 + 8, Y12 - 12, "v12text", "+12V_GD   U3 VDD · Q2 bias · K1 coil+")
    s.circle(RX0, Y12, 5, C_12)

    s.line(RX0, Y5, RX1, Y5, "iso5")
    s.text(980, Y5 - 10, "iso5text", "+5V_ISO   U1 VCC / VE")
    s.circle(RX0, Y5, 5, C_5)

    s.line(RX0, Y70, RX1, Y70, "m70")
    s.text(
        RX0 + 8,
        Y70 + 22,
        "m70text",
        "−70V   U3 GND · Q2 E · Q3 S · U1 pin 5   ·   not MCU_GND",
    )
    s.circle(RX0, Y70, 5, C_70)

    # Earth GND rail
    s.line(GX0, YGND, GX1, YGND, "gnd")
    s.text(GX0, YGND + 22, "gndtext", "MCU_GND   U1 / U4 LED cathodes")
    s.circle(GX0, YGND, 5, C_GND)

    # ========== Power-in strip (floating) ==========
    # −70V and +12V risers at the right margin so returns do not cut the
    # PWM / DIR rows.
    X12R, X70R = 1480, 1510
    Y70BUS = 496
    PWR_Y = 400

    s.rect(400, PWR_Y, 1080, 76, "termbox", rx=5)
    s.text(412, PWR_Y + 22, "label", "Power in")
    s.text(490, PWR_Y + 22, "small", "as-built: T1–T3 upper left · T6–T7 lower left")

    def power_label(x, tid, name, color):
        s.text(x, PWR_Y + 52, "pin", f"{tid}  {name}")
        cx, cy = x + 88, PWR_Y + 48
        s.circle(cx, cy, 4.5, color)
        return cx, cy

    t1x, t1y = power_label(412, "T1", "+12V", C_12)
    s.hop_v(t1x, t1y, Y12, [Y5], "v12", C_12)
    s.circle(t1x, Y12, 5, C_12)

    t2x, t2y = power_label(560, "T2", "12V−", C_70)
    t3x, t3y = power_label(708, "T3", "−70V", C_70)
    s.text(708, PWR_Y + 70, "small", "→ Board C T2")
    t6x, t6y = power_label(980, "T6", "+5V", C_5)
    s.line(t6x, t6y, t6x, Y5, "iso5")
    s.circle(t6x, Y5, 5, C_5)
    t7x, t7y = power_label(1128, "T7", "5V−", C_70)

    for cx, cy in ((t2x, t2y), (t3x, t3y), (t7x, t7y)):
        s.line(cx, cy, cx, Y70BUS, "m70")
        s.circle(cx, Y70BUS, 4, C_70)
    s.line(t2x, Y70BUS, X70R, Y70BUS, "m70")
    s.line(X70R, Y70BUS, X70R, Y70, "m70")
    s.circle(X70R, Y70, 5, C_70)
    s.circle(X70R, Y70BUS, 4, C_70)

    # +12V right riser (T11). U4 collector joins this riser on its own
    # lane at Y12C, above the coil box — not across the Q3 row.
    Y12C = 896
    T11_JOIN = 992
    s.hop_v(X12R, Y12, T11_JOIN, [Y5, Y70BUS], "v12", C_12)
    s.circle(X12R, Y12, 5, C_12)
    s.circle(X12R, Y12C, 4, C_12)

    # T2/T3/T7 drops cross +5V_ISO — hop those three
    # redraw hops: actually the V line already drawn. Leave mid dots as
    # "connected only to −70V" markers; the +5V_ISO rail is a different net.

    # ========== EARTH: T4 T5 T8 ==========
    s.rect(66, 270, 160, 150, "termbox", rx=5)
    s.text(78, 290, "label", "From Board A")
    s.text(78, 306, "small", "earth cable only")

    s.text(78, 338, "pin", "T4  PWM")
    s.circle(210, 334, 4.5, C_EARTH)
    s.text(78, 388, "pin", "T5  GND")
    # T5 sits left of the PWM drop so the nets do not share a node.
    s.circle(178, 384, 4.5, C_GND)

    s.rect(66, 850, 160, 88, "termbox", rx=5)
    s.text(78, 870, "label", "From Board A")
    s.text(78, 886, "small", "shares T5 return")
    s.text(78, 916, "pin", "T8  DIR")
    s.circle(210, 912, 4.5, C_EARTH)

    s.text(78, 438, "small", "T4 ← A T5 GPIO16")
    s.text(78, 454, "small", "T5 ← A T12 MCU_GND")
    s.text(78, 1088, "small", "T8 ← A T6 GPIO21")
    s.text(78, 1104, "small", "No extra GND screw.")

    # GND spine in earth island — left of the LED resistors so the
    # rail does not run through the 220 / 270 bodies.
    GX = 155
    s.path(f"M178 384 H{GX} V{YGND}", "gnd")
    s.circle(GX, 384, 4, C_GND)
    s.circle(GX, YGND, 5, C_GND)

    # ========== U1 6N137 ==========
    # Package only: pin labels + isolation. LED / detector are inside the IC.
    U1_Y = 548
    s.rect(230, U1_Y, 300, 220, "part", rx=6)
    s.text(242, U1_Y + 22, "label", "U1  6N137")
    s.text(242, U1_Y + 40, "small", "PWM opto · DIP-8")
    s.line(ISO_X, U1_Y, ISO_X, U1_Y + 220, "iso")

    # T4 → 220 Ω (discrete, on the earth lead) → pin 2
    s.path("M210 334 H210 V470", "earth")
    s.res_v(210, 470, 26, 48, "220")
    s.path(f"M210 518 V{YPWM} H246", "earth")
    s.text(160, 498, "small", "Rled")
    s.text(246, YPWM + 4, "pin", "2 A")

    s.text(246, YPWM + 36, "pin", "3 K")
    s.path(f"M230 {YPWM + 32} H{GX} V384", "gnd")
    s.circle(GX, YPWM + 32, 4, C_GND)

    # Output pins
    s.text(430, YPWM - 28, "pin", "8 VCC")
    s.text(430, YPWM - 10, "pin", "7 VE")
    s.text(430, YPWM + 18, "pin", "6 OUT")
    s.text(430, YPWM + 88, "pin", "5 GND")

    # Sit right of T1's +12V hop (x=500) so the rail junction is not
    # mistaken for a connection to the orange jumper.
    u1v = 542
    s.path(f"M{u1v} {YPWM - 32} V{Y5}", "iso5")
    s.path(f"M{u1v} {YPWM - 32} H500", "iso5")
    s.path(f"M500 {YPWM - 14} H{u1v}", "iso5")
    s.circle(u1v, Y5, 5, C_5)
    s.circle(u1v, YPWM - 32, 4, C_5)
    s.text(u1v + 8, YPWM - 40, "iso5text", "+5V_ISO")

    u1out = (530, YPWM + 14)
    s.path(f"M500 {YPWM + 14} H530", "v12")
    s.circle(*u1out, 4.5, C_12)

    s.path(f"M500 {YPWM + 84} H522 V{Y_MID70} H{X70R}", "m70")
    s.circle(522, YPWM + 84, 4, C_70)
    s.circle(X70R, Y_MID70, 4, C_70)

    # Local bypass, floating side of U1: pin 8 (+5V_ISO) to pin 5 (−70V)
    cby = 468
    s.path(f"M500 {YPWM - 32} H{cby}", "iso5")
    s.cap_v(cby, YPWM - 32, YPWM + 84, "100n")
    s.path(f"M{cby} {YPWM + 84} H500", "m70")
    s.circle(cby, YPWM - 32, 4, C_5)
    s.circle(cby, YPWM + 84, 4, C_70)

    s.text(242, U1_Y + 236, "small", "VE = +5V_ISO, not −70V.")

    # ========== Q2 fail-off ==========
    q2 = s.npn(660, YPWM + 8)
    s.text(676, YPWM - 44, "pin", "C")
    s.text(620, YPWM + 4, "pin", "B")
    s.text(676, YPWM + 64, "pin", "E")
    s.text(704, YPWM + 20, "label", "Q2 PN2222A")
    s.text(704, YPWM + 36, "small", "TO-92 · verify E-B-C")

    # U1 OUT to base
    s.path(f"M530 {u1out[1]} H{q2['b'][0]} V{q2['b'][1]}", "v12")
    s.circle(q2["b"][0], u1out[1], 4, C_12)

    # Rb2 22k: +12V to base
    rbx = 580
    rb2_top = 530
    s.hop_v(rbx, Y12, rb2_top, [Y5], "v12", C_12)
    s.res_v(rbx, rb2_top, 26, 48, "22k")
    s.path(f"M{rbx} {rb2_top + 48} V{u1out[1]} H{q2['b'][0]}", "v12")
    s.circle(rbx, Y12, 5, C_12)
    s.text(548, rb2_top + 26, "small", "Rb2")

    # Rbe2 100k: base to −70V, then mid corridor to the right riser
    rbe_top = YPWM + 20
    s.path(f"M{rbx} {u1out[1]} V{rbe_top}", "m70")
    s.res_v(rbx, rbe_top, 26, 52, "100k")
    s.path(f"M{rbx} {rbe_top + 52} V{Y_MID70} H{X70R}", "m70")
    s.circle(rbx, Y_MID70, 4, C_70)
    s.text(540, rbe_top + 28, "small", "Rbe2")

    # Emitter drops to the −70V corridor — do not cut the fail-off note.
    s.path(f"M{q2['e'][0]} {q2['e'][1]} V{Y_MID70}", "m70")
    s.circle(q2["e"][0], Y_MID70, 4, C_70)

    # Rc2 10k: +12V to collector
    rcx = 735
    rc2_top = 520
    s.hop_v(rcx, Y12, rc2_top, [Y5, Y70BUS], "v12", C_12)
    s.res_v(rcx, rc2_top, 26, 48, "10k")
    s.path(f"M{rcx} {rc2_top + 48} V{q2['c'][1]} H{q2['c'][0]}", "v12")
    s.circle(rcx, Y12, 5, C_12)
    s.circle(q2["c"][0], q2["c"][1], 4, C_12)
    s.text(748, rc2_top + 24, "small", "Rc2")

    # Collector to U3 IN A
    ina_y = YPWM - 20
    U3_X = 868
    s.path(f"M{q2['c'][0]} {q2['c'][1]} V{ina_y} H{U3_X}", "v12")

    s.rect(548, 780, 220, 72, "callout", rx=4)
    s.text(560, 800, "label", "Fail-off")
    s.text(560, 818, "small", "LED off / U1 release: Q2 on → gate off.")
    s.text(560, 834, "small", "LED on: Q2 off, Rc2 pulls IN A HIGH.")

    # ========== U3 TC4426 ==========
    # Shifted right so VDD / GND / IN A sit in the corridor, not on pin text.
    U3_Y = 548
    U3_W = 268
    s.rect(U3_X, U3_Y, U3_W, 260, "part", rx=6)
    s.text(U3_X + 12, U3_Y + 22, "label", "U3  TC4426CPA")
    s.text(U3_X + 12, U3_Y + 40, "small", "dual inverting · both channels")

    inv_x = U3_X + 72
    # IN A → OUT A
    s.text(U3_X + 12, ina_y + 4, "pin", "2 IN A")
    xoa = s.inverter(inv_x, ina_y)
    s.line(U3_X, ina_y, inv_x, ina_y, "v12")
    s.circle(U3_X, ina_y, 4, C_12)
    s.text(xoa + 4, ina_y - 8, "pin", "7 OUT A")

    # IN B → OUT B
    inb_y = YPWM + 60
    s.text(U3_X + 12, inb_y + 4, "pin", "4 IN B")
    xob = s.inverter(inv_x, inb_y)
    loop_l = U3_X + 60
    s.line(loop_l, inb_y, inv_x, inb_y, "earth")
    s.text(xob + 4, inb_y - 8, "pin", "5 OUT B")

    # OUT A → IN B. Stay off the OUT B exit so the nets do not short.
    s.path(
        f"M{xoa} {ina_y} H{U3_X + 240} V{inb_y - 20} H{loop_l} V{inb_y}",
        "wire",
    )
    s.circle(xoa, ina_y, 3.5, C_INK)
    s.circle(loop_l, inb_y, 3.5, C_INK)

    # VDD / GND stay in the Q2–U3 corridor (not through the pin labels)
    vdd_x = 808
    s.text(U3_X + 12, U3_Y + 188, "pin", "6 VDD")
    s.text(U3_X + 12, U3_Y + 220, "pin", "3 GND")
    s.text(U3_X + 152, U3_Y + 188, "pin", "1 / 8 NC")
    s.path(f"M{U3_X} {U3_Y + 184} H{vdd_x}", "v12")
    s.hop_v(vdd_x, U3_Y + 184, Y12, [ina_y, Y70BUS, Y5], "v12", C_12)
    s.circle(vdd_x, Y12, 5, C_12)
    s.path(f"M{U3_X} {U3_Y + 216} H{vdd_x} V{Y_MID70} H{X70R}", "m70")
    s.circle(vdd_x, Y_MID70, 4, C_70)

    s.text(U3_X + 12, U3_Y + 248, "small", "100 nF + 1 µF at pins 6–3 · 47 µF nearby")

    # OUT B → 100Ω → T9
    rg_x = 1150
    s.path(f"M{xob} {inb_y} H{rg_x}", "earth")
    s.res_h(rg_x, inb_y, 36, 24, "100")
    T9_X = 1238
    s.path(f"M{rg_x + 36} {inb_y} H{T9_X}", "earth")
    s.text(rg_x, inb_y + 28, "small", "Rg  as-built")

    # T9
    T9_Y = inb_y + 16
    T9_N = T9_X + 160
    s.rect(T9_X, T9_Y, 180, 88, "termbox", rx=5)
    s.text(T9_X + 12, T9_Y + 20, "label", "Out to Board C")
    s.text(T9_X + 12, T9_Y + 36, "small", "as-built upper right")
    s.text(T9_X + 12, T9_Y + 64, "pin", "T9  PWM out")
    s.circle(T9_N, T9_Y + 60, 4.5, C_EARTH)
    s.path(f"M{T9_X} {inb_y} H{T9_N} V{T9_Y + 60}", "earth")
    s.circle(T9_N, inb_y, 4, C_EARTH)
    s.text(T9_X + 12, T9_Y + 80, "small", "U3 OUT B → Board C T3")

    warn_x = 1160
    s.rect(warn_x, U3_Y, 256, 72, "isocall", rx=4)
    s.text(warn_x + 12, U3_Y + 20, "label", "T8 / T9 warning")
    s.text(warn_x + 12, U3_Y + 38, "small", "As-built they share the upper-right")
    s.text(warn_x + 12, U3_Y + 54, "small", "block. T8 is earth; T9 is floating.")
    s.text(warn_x + 12, U3_Y + 70, "small", "Do not jumper those two screws.")

    # ========== U4 4N35 ==========
    # Package only: pin labels + isolation. LED / phototransistor are inside the IC.
    U4_Y = 910
    s.rect(230, U4_Y, 300, 220, "part", rx=6)
    s.text(242, U4_Y + 22, "label", "U4  4N35")
    s.text(242, U4_Y + 40, "small", "DIR opto · DIP-6")
    s.line(ISO_X, U4_Y, ISO_X, U4_Y + 220, "iso")

    # T8 → 270 Ω (discrete, on the earth lead) → pin 1
    s.path("M210 912 V940", "earth")
    s.res_v(210, 940, 26, 48, "270")
    s.path(f"M210 988 V{YDIR} H246", "earth")
    s.text(160, 968, "small", "Rled")
    s.text(246, YDIR + 4, "pin", "1 A")

    s.text(246, YDIR + 36, "pin", "2 K")
    s.path(f"M230 {YDIR + 32} H{GX}", "gnd")
    s.circle(GX, YDIR + 32, 4, C_GND)

    s.text(430, YDIR - 20, "pin", "5 C")
    s.text(430, YDIR + 24, "pin", "4 E")
    s.text(430, YDIR + 48, "small", "6 B  nc")

    # Collector to +12V_GD: step right off the U4 border, then the
    # dedicated +12V lane above the coil box.
    u4c_x = 556
    s.path(f"M530 {YDIR - 20} H{u4c_x} V{Y12C} H{X12R}", "v12")
    s.circle(530, YDIR - 20, 4, C_12)
    s.circle(u4c_x, YDIR - 20, 4, C_12)
    s.circle(u4c_x, Y12C, 4, C_12)

    em = (530, YDIR + 20)
    s.path(f"M{em[0]} {em[1]} H560", "v12")
    s.res_h(560, em[1], 32, 22, "1k")
    s.path(f"M592 {em[1]} H620", "v12")
    s.text(560, em[1] + 28, "small", "Rg3")

    s.text(242, U4_Y + 236, "small", "DIR LOW / open / dark U4 → Q3 off, K1 released.")

    # ========== Q3 2N7000 ==========
    q3 = s.nmos(720, YDIR + 20)
    s.text(668, YDIR + 8, "pin", "G")
    s.text(760, YDIR - 48, "pin", "D")
    s.text(754, YDIR + 80, "pin", "S")
    s.text(770, YDIR + 98, "label", "Q3 2N7000TA")
    s.text(770, YDIR + 114, "small", "TO-92 · S-G-D")

    s.path(f"M620 {YDIR + 20} H{q3['g'][0]}", "v12")
    s.circle(620, YDIR + 20, 4, C_12)

    # 10k G-S
    s.path(f"M620 {YDIR + 20} V{YDIR + 72}", "m70")
    s.res_v(620, YDIR + 72, 26, 48, "10k")
    s.path(f"M620 {YDIR + 120} V{Y70}", "m70")
    s.circle(620, Y70, 5, C_70)
    s.text(552, YDIR + 100, "small", "Rgs3")

    # Source to −70V
    s.path(f"M{q3['s'][0]} {q3['s'][1]} V{Y70}", "m70")
    s.circle(q3["s"][0], Y70, 5, C_70)

    # ========== D5 + T10 T11 ==========
    # Box first so the T10 drain lead paints on top of the fill.
    COIL_X, COIL_W, COIL_Y = 1000, 460, 920
    T11_Y = COIL_Y + 72
    T10_Y = COIL_Y + 168
    T11_N, T10_N = 1160, 1160
    DRAIN_X = 900
    s.rect(COIL_X, COIL_Y, COIL_W, 250, "termbox", rx=5)
    s.text(COIL_X + 12, COIL_Y + 20, "label", "Out to K1 coil")
    s.text(
        COIL_X + 12,
        COIL_Y + 36,
        "small",
        "as-built lower right · D5 on these screws",
    )
    s.text(COIL_X + 12, COIL_Y + 216, "small", "Do not route coil current through T9.")
    s.text(COIL_X + 12, COIL_Y + 232, "small", "Phoenix PLC-RSC-12DC/21-21  A1+ / A2−")

    s.circle(q3["d"][0], q3["d"][1], 4, C_70)
    s.path(
        f"M{q3['d'][0]} {q3['d'][1]} H{DRAIN_X} V{T10_Y} H{T10_N}",
        "m70",
    )
    s.circle(DRAIN_X, q3["d"][1], 4, C_70)
    s.circle(DRAIN_X, T10_Y, 4, C_70)

    s.text(T11_N - 100, T11_Y - 22, "pin", "T11  Coil+")
    s.circle(T11_N, T11_Y, 4.5, C_12)
    s.path(f"M{T11_N} {T11_Y} H{X12R}", "v12")
    s.circle(X12R, T11_Y, 4.5, C_12)

    s.text(T10_N - 100, T10_Y - 22, "pin", "T10  Coil−")
    s.circle(T10_N, T10_Y, 4.5, C_70)

    # D5 across T11/T10 (cathode/band at T11 / +12V)
    D5_X = 1340
    s.path(f"M{T11_N} {T11_Y} H{D5_X}", "v12")
    s.diode_v(D5_X, T11_Y, T10_Y, C_12, cathode_up=True)
    s.path(f"M{D5_X} {T10_Y} H{T10_N}", "m70")
    mid_d5 = (T11_Y + T10_Y) / 2
    s.text(1192, mid_d5 - 4, "label", "D5 1N4007")
    s.text(1192, mid_d5 + 14, "small", "K → T11  ·  A → T10")
    s.text(1192, mid_d5 + 30, "small", "coil flyback only")

    # Earth leftover notes
    s.text(66, 1180, "small", "Top earth rails unused.")
    s.text(66, 1196, "small", "Bottom − at col 5 = MCU_GND.")
    s.text(66, 1228, "small", "R7 10 kΩ DIR pull-down")
    s.text(66, 1244, "small", "lives on Board A, not here.")

    # As-built footer inside onboard
    s.text(
        56,
        1344,
        "small",
        "As-built screws:  UL T1 +12V · T2 12V− · T3 −70V  ·  "
        "top T4 PWM · T5 GND  ·  LL T6 +5V · T7 5V−  ·  "
        "UR T8 DIR · T9 PWM out  ·  LR T10 Coil− · T11 Coil+",
    )

    # ---- OFF BOARD ----
    # Wide gutter so T9 / T3 / T11 / T10 do not stack on the board edge.
    OFF_X, OFF_W, OFF_IN = 1720, 520, 1736
    TX, TX2 = OFF_IN + 12, OFF_IN + 232
    s.rect(OFF_X, 162, OFF_W, 1190, "off", rx=8)
    s.text(OFF_IN, 186, "section", "OFF BOARD")
    s.text(OFF_IN, 204, "small", "Dashed outline. Not on Board B copper.")

    s.rect(OFF_IN, 220, 488, 132, "box", rx=5)
    s.text(TX, 244, "label", "Board A · earth control")
    s.text(TX, 268, "pin", "T5  PWM      GPIO16  →  Board B T4")
    s.text(TX, 286, "pin", "T6  DIR   GPIO21  →  Board B T8")
    s.text(TX, 304, "pin", "T12 MCU_GND           →  Board B T5")
    s.text(TX, 326, "small", "3-pin earth cable. No floating supply on it.")
    s.text(TX, 342, "small", "R7 10 kΩ holds DIR low if A is dark.")

    s.rect(OFF_IN, 368, 488, 100, "box", rx=5)
    s.text(TX, 392, "label", "HDR-15-12  ·  +12V_GD / −70V")
    s.text(TX, 416, "pin", "+  →  T1     −  →  T2")
    s.text(TX, 436, "small", "Negative bonded only to −70V.")
    s.text(TX, 454, "small", "Feeds U3, Q2 bias, K1 coil, U4 collector.")

    s.rect(OFF_IN, 484, 488, 88, "box", rx=5)
    s.text(TX, 508, "label", "HDR-15-5 #2  ·  +5V_ISO / −70V")
    s.text(TX, 532, "pin", "+  →  T6     −  →  T7")
    s.text(TX, 554, "small", "U1 VCC and VE only. Not MCU_GND.")

    s.rect(OFF_IN, 588, 488, 120, "box", rx=5)
    s.text(TX, 612, "label", "Board C · HV switch")
    s.text(TX, 636, "pin", "T3  GATE  ←  Board B T9   (100 Ω on B)")
    s.text(TX, 654, "pin", "T2  −70V  ←  Board B T3   (≤150 mm)")
    s.text(TX, 676, "small", "Q1 source / U3 pin 3 share this return.")
    s.text(TX, 694, "small", "Rgs 10 kΩ and Dz BZX85C15 sit on C.")

    # Off-board exits: one column per net. T11 rides the existing +12V
    # riser (X12R). T3 is the same −70V net as X70R — leave at Board C
    # height, do not drop under the floating-region box.
    GUT9, GUT10 = 1588, 1660
    s.hop_h(T9_Y + 60, T9_N, GUT9, [X12R, X70R], "earth", C_EARTH)
    s.path(f"M{GUT9} {T9_Y + 60} V636 H{OFF_IN}", "earth")
    s.circle(OFF_IN, 636, 4, C_EARTH)
    s.circle(X70R, 654, 4, C_70)
    s.hop_h(654, X70R, OFF_IN, [GUT9], "m70", C_70)
    s.circle(OFF_IN, 654, 4, C_70)
    s.text(T9_N + 8, T9_Y + 52, "earthtext", "T9")
    s.text(X70R - 28, 648, "m70text", "T3")

    s.rect(OFF_IN, 724, 488, 200, "box", rx=5)
    s.text(TX, 748, "label", "K1 Phoenix PLC-RSC-12DC/21-21")
    s.text(TX, 766, "small", "Coil on this drawing. Contacts on Board C.")
    s.text(TX, 792, "pin", "A1+   ←  T11  Coil+   +12V_GD")
    s.text(TX, 810, "pin", "A2−   ←  T10  Coil−   Q3 drain")
    s.text(TX, 834, "small", "Released = one polarity; energized = reverse.")
    s.text(TX, 850, "small", "PWM off, wait for current decay, switch K1,")
    s.text(TX, 866, "small", "wait ≥25 ms, then resume PWM.")
    s.text(TX, 888, "small", "Housing DC13 does not rate 140 VDC / 2 A.")

    s.circle(X12R, 792, 4.5, C_12)
    s.path(f"M{X12R} 792 H{OFF_IN}", "v12")
    s.hop_h(T10_Y, T10_N, GUT10, [X70R], "m70", C_70)
    s.path(f"M{GUT10} {T10_Y} V810 H{OFF_IN}", "m70")
    s.circle(OFF_IN, 792, 4, C_12)
    s.circle(OFF_IN, 810, 4, C_70)
    s.text(X12R - 36, 808, "v12text", "T11")
    s.text(GUT10 - 8, T10_Y + 18, "m70text", "T10")

    s.rect(OFF_IN, 940, 488, 90, "box", rx=5)
    s.text(TX, 964, "label", "Not on Board B")
    s.text(TX, 986, "small", "Q1 IRFP460 · D9 · TVS1 · SW · motor leads")
    s.text(TX, 1004, "small", "ACS712 and F1 stay on the DIN HV island.")
    s.text(TX, 1022, "small", "See schematics/Board_C.svg")

    s.rect(OFF_IN, 1046, 488, 280, "part", rx=5)
    s.text(TX, 1074, "label", "Private Reserve")
    s.text(TX, 1098, "text", "DWG  schematics/Board_B.svg")
    s.text(TX, 1120, "text", "REV  2026-08-22 · gate driver")
    s.text(TX, 1152, "small", "Datasheets:")
    s.text(TX, 1172, "small", "6N137 · TC4426 · 4N35")
    s.text(TX, 1190, "small", "PN2222A · 2N7000TA · 1N4007")
    s.text(TX2, 1152, "small", "HDR-15-12 · HDR-15-5")
    s.text(TX2, 1172, "small", "PLC-RSC-12DC/21-21")
    s.text(TX2, 1190, "small", "See Board_C for IRFP460")

    s.text(
        40,
        1408,
        "small",
        "Loss of +5V_EARTH extinguishes U1 LED → Q2 on → gate off. "
        "Prove +5V_ISO-loss fail-off on the bench before 140 V. "
        "An open Rb2 or reversed Q2 can command continuous full duty.",
    )

    a("</svg>")
    return s.out()


def main():
    svg = build()
    OUT.write_text(svg)
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
