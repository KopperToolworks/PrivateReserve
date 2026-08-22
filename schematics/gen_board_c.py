#!/usr/bin/env python3
"""Generate Board_C.svg — HV switch-stage service schematic."""

from pathlib import Path

OUT = Path(__file__).with_name("Board_C.svg")

W, H = 1600, 1080

Y70P = 248
YSW = 492
Y70N = 900
YGATE = 612

X_IN = 214
X_TVS = 304
X_GATE_DROP = 198
X_CLAMP = 440
X_Q1 = 640
X_D9 = 860
X_SW_END = 1008
X_SW_DROP = 1032
X_BOX_R = 1072
X_OFF = 1094

# Left terminal pins (inside the In block)
T1 = (172, 286)
T2 = (172, 336)
T3 = (172, 386)


def hop_h(x0, x1, y, hop_x, r=9):
    """Horizontal wire left→right with a semicircle hop at hop_x."""
    lo, hi = (x0, x1) if x0 < x1 else (x1, x0)
    if not (lo < hop_x < hi):
        return f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}" class="lv"/>'
    # draw left, hop, right
    return (
        f'<line x1="{x0}" y1="{y}" x2="{hop_x - r}" y2="{y}" class="lv"/>'
        f'<path d="M{hop_x - r} {y} A{r} {r} 0 0 1 {hop_x + r} {y}" '
        f'fill="none" stroke="#175cd3" stroke-width="3"/>'
        f'<line x1="{hop_x + r}" y1="{y}" x2="{x1}" y2="{y}" class="lv"/>'
    )


svg = f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"
     viewBox="0 0 {W} {H}" version="1.1" role="img"
     aria-labelledby="title desc">
  <title id="title">Board C — HV switch stage</title>
  <desc id="desc">Service schematic for the Private Reserve Board C
    Perma-Proto. Q1 IRFP460, D9 STTH8R06 flyback, TVS1 P6KE200A, Rgs and
    Dz at the gate. T1–T5 screw terminals. K1 and the motor are off-board.
  </desc>
  <defs>
    <style>
      .wire {{ fill:none; stroke:#222; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .hv {{ fill:none; stroke:#b42318; stroke-width:4; stroke-linecap:round; stroke-linejoin:round }}
      .sw {{ fill:none; stroke:#d97706; stroke-width:4; stroke-linecap:round; stroke-linejoin:round }}
      .lv {{ fill:none; stroke:#175cd3; stroke-width:3; stroke-linecap:round; stroke-linejoin:round }}
      .m70 {{ fill:none; stroke:#78716c; stroke-width:4; stroke-linecap:round; stroke-linejoin:round }}
      .box {{ fill:#fff; stroke:#344054; stroke-width:2 }}
      .part {{ fill:#f9fafb; stroke:#344054; stroke-width:2 }}
      .onboard {{ fill:#fff; stroke:#344054; stroke-width:2.4 }}
      .off {{ fill:#f9fafb; stroke:#667085; stroke-width:2; stroke-dasharray:8 5 }}
      .warn {{ fill:#fffaeb; stroke:#dc6803; stroke-width:2 }}
      .termbox {{ fill:#fff; stroke:#344054; stroke-width:2 }}
      .callout {{ fill:#fffaeb; stroke:#dc6803; stroke-width:1.6; stroke-dasharray:5 3 }}
      .title {{ font:700 26px sans-serif; fill:#101828 }}
      .section {{ font:700 16px sans-serif; fill:#101828 }}
      .label {{ font:600 15px sans-serif; fill:#101828 }}
      .text {{ font:14px sans-serif; fill:#344054 }}
      .small {{ font:12px sans-serif; fill:#475467 }}
      .pin {{ font:12px monospace; fill:#344054 }}
      .hvtext {{ font:700 14px sans-serif; fill:#b42318 }}
      .swtext {{ font:700 14px sans-serif; fill:#b54708 }}
      .lvtext {{ font:700 13px sans-serif; fill:#175cd3 }}
      .m70text {{ font:700 14px sans-serif; fill:#78716c }}
    </style>
  </defs>

  <rect width="{W}" height="{H}" fill="#fff"/>

  <text x="40" y="40" class="title">Board C — HV switch stage</text>
  <text x="40" y="64" class="text">Private Reserve · as-built Adafruit Perma-Proto 1/4 (cols 1–15) · all copper referenced to −70V · Rev 2026-08-22</text>

  <rect x="40" y="78" width="1520" height="62" rx="5" class="warn"/>
  <text x="56" y="102" class="label">DANGER — 140 V DC can kill.</text>
  <text x="56" y="124" class="text">Every net on this board is hazardous relative to earth when the motor supply is on. −70V is not MCU_GND. Scope gate-to-source with an isolated probe. Discharge the bus before service.</text>

  <!-- BOARD C -->
  <rect x="40" y="158" width="{X_BOX_R - 40}" height="800" rx="8" class="onboard"/>
  <text x="56" y="184" class="section">BOARD C · on-board</text>
  <text x="56" y="204" class="small">Perma-Proto ± rails unused. HV uses column jumpers. No MCU_GND, +5V_EARTH, +5V_ISO, or +12V_GD on this board.</text>

  <!-- Rails -->
  <line x1="{X_IN}" y1="{Y70P}" x2="{X_SW_END}" y2="{Y70P}" class="hv"/>
  <text x="{X_IN + 10}" y="{Y70P - 12}" class="hvtext">+70V  fused high rail</text>

  <line x1="{X_CLAMP - 30}" y1="{YSW}" x2="{X_SW_DROP}" y2="{YSW}" class="sw"/>
  <text x="{X_CLAMP + 8}" y="{YSW - 12}" class="swtext">SW  Q1 drain · D9 anode · K1 return</text>

  <line x1="{X_IN}" y1="{Y70N}" x2="{X_SW_END}" y2="{Y70N}" class="m70"/>
  <text x="{X_IN + 10}" y="{Y70N + 22}" class="m70text">−70V  Q1 source / gate-driver return  ·  not MCU_GND</text>

  <!-- In terminals T1 T2 T3 -->
  <rect x="56" y="220" width="128" height="200" rx="5" class="termbox"/>
  <text x="68" y="242" class="label">In</text>
  <text x="68" y="258" class="small">bottom edge T1–T3</text>

  <text x="68" y="290" class="pin">T1  +70V</text>
  <circle cx="{T1[0]}" cy="{T1[1]}" r="4.5" fill="#b42318"/>
  <path d="M{T1[0]} {T1[1]} H{X_IN} V{Y70P}" class="hv"/>
  <circle cx="{X_IN}" cy="{Y70P}" r="5" fill="#b42318"/>

  <text x="68" y="340" class="pin">T2  −70V</text>
  <circle cx="{T2[0]}" cy="{T2[1]}" r="4.5" fill="#78716c"/>
  <path d="M{T2[0]} {T2[1]} H{X_IN} V{Y70N}" class="m70"/>
  <circle cx="{X_IN}" cy="{Y70N}" r="5" fill="#78716c"/>
  <circle cx="{X_IN}" cy="{T2[1]}" r="4" fill="#78716c"/>

  <text x="68" y="390" class="pin">T3  GATE / PWM</text>
  <circle cx="{T3[0]}" cy="{T3[1]}" r="4.5" fill="#175cd3"/>
  <path d="M{T3[0]} {T3[1]} H{X_GATE_DROP} V{YGATE}" class="lv"/>
  {hop_h(X_GATE_DROP, X_CLAMP, YGATE, X_TVS)}
  <circle cx="{X_GATE_DROP}" cy="{YGATE}" r="4" fill="#175cd3"/>

  <text x="68" y="444" class="small">T1 T2 T4 T5:</text>
  <text x="68" y="460" class="small">heavy HV wire.</text>
  <text x="68" y="480" class="small">T3: PWM</text>
  <text x="68" y="496" class="small">gate lead.</text>

  <text x="228" y="640" class="lvtext">PWM from Board B T9</text>
  <text x="228" y="656" class="small">Rg 100 Ω on Board B (as-built)</text>
  <text x="228" y="672" class="small">U3 OUT B → 100 Ω → T3 GATE</text>

  <!-- TVS1 -->
  <line x1="{X_TVS}" y1="{Y70P}" x2="{X_TVS}" y2="298" class="hv"/>
  <circle cx="{X_TVS}" cy="{Y70P}" r="5" fill="#b42318"/>
  <line x1="{X_TVS - 18}" y1="298" x2="{X_TVS + 18}" y2="298" stroke="#b42318" stroke-width="3"/>
  <line x1="{X_TVS - 18}" y1="304" x2="{X_TVS + 18}" y2="304" stroke="#b42318" stroke-width="3"/>
  <polygon points="{X_TVS - 16},346 {X_TVS + 16},346 {X_TVS},304" fill="none" stroke="#b42318" stroke-width="3"/>
  <line x1="{X_TVS}" y1="346" x2="{X_TVS}" y2="{Y70N}" class="m70"/>
  <circle cx="{X_TVS}" cy="{Y70N}" r="5" fill="#78716c"/>
  <text x="{X_TVS + 22}" y="316" class="label">TVS1 P6KE200A</text>
  <text x="{X_TVS + 22}" y="334" class="small">K / band → +70V</text>
  <text x="{X_TVS + 22}" y="350" class="small">A → −70V</text>
  <text x="{X_TVS + 22}" y="366" class="small">unidirectional</text>

  <!-- Rgs -->
  <line x1="{X_CLAMP}" y1="{YGATE}" x2="{X_CLAMP - 40}" y2="{YGATE}" class="lv"/>
  <line x1="{X_CLAMP - 40}" y1="{YGATE}" x2="{X_CLAMP - 40}" y2="700" class="lv"/>
  <rect x="{X_CLAMP - 54}" y="700" width="28" height="68" class="part"/>
  <text x="{X_CLAMP - 50}" y="740" class="pin">10k</text>
  <line x1="{X_CLAMP - 40}" y1="768" x2="{X_CLAMP - 40}" y2="{Y70N}" class="m70"/>
  <circle cx="{X_CLAMP - 40}" cy="{YGATE}" r="4" fill="#175cd3"/>
  <circle cx="{X_CLAMP - 40}" cy="{Y70N}" r="5" fill="#78716c"/>
  <text x="{X_CLAMP - 96}" y="738" class="label">Rgs</text>

  <!-- Dz cathode at gate -->
  <line x1="{X_CLAMP}" y1="{YGATE}" x2="{X_CLAMP + 40}" y2="{YGATE}" class="lv"/>
  <line x1="{X_CLAMP + 40}" y1="{YGATE}" x2="{X_CLAMP + 40}" y2="704" class="lv"/>
  <circle cx="{X_CLAMP + 40}" cy="{YGATE}" r="4" fill="#175cd3"/>
  <line x1="{X_CLAMP + 22}" y1="704" x2="{X_CLAMP + 58}" y2="704" stroke="#175cd3" stroke-width="3"/>
  <polyline points="{X_CLAMP + 22},704 {X_CLAMP + 22},696 {X_CLAMP + 14},696" fill="none" stroke="#175cd3" stroke-width="3"/>
  <polygon points="{X_CLAMP + 24},740 {X_CLAMP + 56},740 {X_CLAMP + 40},704" fill="none" stroke="#175cd3" stroke-width="3"/>
  <line x1="{X_CLAMP + 40}" y1="740" x2="{X_CLAMP + 40}" y2="{Y70N}" class="m70"/>
  <circle cx="{X_CLAMP + 40}" cy="{Y70N}" r="5" fill="#78716c"/>
  <text x="{X_CLAMP + 62}" y="716" class="label">Dz BZX85C15</text>
  <text x="{X_CLAMP + 62}" y="734" class="small">K / band → gate</text>
  <text x="{X_CLAMP + 62}" y="750" class="small">A → source / −70V</text>

  <!-- Gate to Q1 -->
  <line x1="{X_CLAMP}" y1="{YGATE}" x2="{X_Q1 - 10}" y2="{YGATE}" class="lv"/>

  <!-- Q1 MOSFET -->
  <circle cx="{X_Q1}" cy="{YSW}" r="5" fill="#d97706"/>
  <line x1="{X_Q1}" y1="{YSW}" x2="{X_Q1}" y2="548" class="sw"/>
  <text x="{X_Q1 + 10}" y="520" class="pin">2 D / tab</text>
  <line x1="{X_Q1 - 8}" y1="548" x2="{X_Q1 + 8}" y2="548" class="wire"/>
  <line x1="{X_Q1}" y1="548" x2="{X_Q1}" y2="576" class="wire"/>
  <line x1="{X_Q1}" y1="576" x2="{X_Q1 + 22}" y2="576" stroke="#222" stroke-width="3"/>
  <line x1="{X_Q1}" y1="{YGATE}" x2="{X_Q1 + 22}" y2="{YGATE}" stroke="#222" stroke-width="3"/>
  <line x1="{X_Q1}" y1="648" x2="{X_Q1 + 22}" y2="648" stroke="#222" stroke-width="3"/>
  <line x1="{X_Q1 + 22}" y1="570" x2="{X_Q1 + 22}" y2="654" stroke="#222" stroke-width="3"/>
  <line x1="{X_Q1 - 10}" y1="568" x2="{X_Q1 - 10}" y2="656" stroke="#175cd3" stroke-width="3"/>
  <text x="{X_Q1 - 48}" y="{YGATE - 10}" class="pin">1 G</text>
  <line x1="{X_Q1 + 22}" y1="648" x2="{X_Q1 + 22}" y2="684" class="wire"/>
  <line x1="{X_Q1 + 22}" y1="684" x2="{X_Q1}" y2="684" class="wire"/>
  <line x1="{X_Q1}" y1="684" x2="{X_Q1}" y2="{Y70N}" class="m70"/>
  <polygon points="{X_Q1 + 22},648 {X_Q1 + 16},662 {X_Q1 + 28},662" fill="#222"/>
  <text x="{X_Q1 + 10}" y="704" class="pin">3 S</text>
  <circle cx="{X_Q1}" cy="{Y70N}" r="5" fill="#78716c"/>

  <!-- body diode -->
  <path d="M{X_Q1 + 22} 576 H{X_Q1 + 58} V598" class="wire"/>
  <line x1="{X_Q1 + 44}" y1="598" x2="{X_Q1 + 72}" y2="598" stroke="#222" stroke-width="2.5"/>
  <polygon points="{X_Q1 + 46},626 {X_Q1 + 70},626 {X_Q1 + 58},598" fill="none" stroke="#222" stroke-width="2.5"/>
  <path d="M{X_Q1 + 58} 626 V648 H{X_Q1 + 22}" class="wire"/>
  <text x="{X_Q1 + 76}" y="614" class="small">body diode</text>

  <rect x="668" y="780" width="176" height="72" rx="4" class="part"/>
  <text x="680" y="802" class="label">Q1 IRFP460NPBF</text>
  <text x="680" y="820" class="pin">TO-247  1 G  2 D  3 S</text>
  <text x="680" y="838" class="small">tab = drain = SW · insulate</text>

  <!-- D9 -->
  <circle cx="{X_D9}" cy="{Y70P}" r="5" fill="#b42318"/>
  <line x1="{X_D9}" y1="{Y70P}" x2="{X_D9}" y2="298" class="hv"/>
  <line x1="{X_D9 - 18}" y1="298" x2="{X_D9 + 18}" y2="298" stroke="#b42318" stroke-width="3"/>
  <polygon points="{X_D9 - 16},338 {X_D9 + 16},338 {X_D9},298" fill="none" stroke="#b42318" stroke-width="3"/>
  <line x1="{X_D9}" y1="338" x2="{X_D9}" y2="{YSW}" class="sw"/>
  <circle cx="{X_D9}" cy="{YSW}" r="5" fill="#d97706"/>
  <text x="{X_D9 + 22}" y="306" class="label">D9 STTH8R06D</text>
  <text x="{X_D9 + 22}" y="324" class="small">K / pin 1 / tab → +70V</text>
  <text x="{X_D9 + 22}" y="340" class="small">A / pin 2 → SW</text>
  <text x="{X_D9 + 22}" y="356" class="small">Do not use 1N4007 here.</text>

  <rect x="740" y="366" width="252" height="90" rx="4" class="callout"/>
  <text x="756" y="390" class="label">Flyback loop</text>
  <text x="756" y="410" class="small">SW → D9 A → D9 K → +70V</text>
  <text x="756" y="428" class="small">Keep short. Entirely on Board C.</text>
  <text x="756" y="446" class="small">Do not cable this path off the board.</text>

  <!-- Out terminals T4 T5 -->
  <rect x="900" y="548" width="148" height="122" rx="5" class="termbox"/>
  <text x="912" y="570" class="label">Out to K1</text>
  <text x="912" y="586" class="small">bottom edge T4–T5</text>
  <text x="912" y="616" class="pin">T4  NC1</text>
  <text x="912" y="652" class="pin">T5  NO2</text>
  <circle cx="1032" cy="612" r="4.5" fill="#d97706"/>
  <circle cx="1032" cy="648" r="4.5" fill="#d97706"/>
  <path d="M{X_SW_DROP} {YSW} V612" class="sw"/>
  <line x1="{X_SW_DROP}" y1="612" x2="{X_SW_DROP}" y2="648" class="sw"/>
  <circle cx="{X_SW_DROP}" cy="{YSW}" r="5" fill="#d97706"/>

  <text x="56" y="946" class="small">As-built bottom edge, heatsink up, left → right:  T1 +70V · T2 −70V · T3 GATE (PWM) · T4 NC1 · T5 NO2</text>

  <!-- OFF BOARD -->
  <rect x="{X_OFF}" y="158" width="466" height="800" rx="8" class="off"/>
  <text x="{X_OFF + 16}" y="184" class="section">OFF BOARD</text>
  <text x="{X_OFF + 16}" y="204" class="small">Dashed outline. Not on Board C copper.</text>

  <rect x="{X_OFF + 16}" y="220" width="434" height="108" rx="5" class="box"/>
  <text x="{X_OFF + 28}" y="244" class="label">Board B · floating gate driver</text>
  <text x="{X_OFF + 28}" y="268" class="pin">T9  PWM out / U3 OUT B</text>
  <text x="{X_OFF + 28}" y="284" class="small">PWM  ·  on-board 100 Ω  →  Board C T3 GATE</text>
  <text x="{X_OFF + 28}" y="308" class="pin">T3  −70V</text>
  <text x="{X_OFF + 28}" y="324" class="small">short return  →  Board C T2  (≤150 mm)</text>

  <rect x="{X_OFF + 16}" y="344" width="434" height="380" rx="5" class="box"/>
  <text x="{X_OFF + 28}" y="368" class="label">K1 Phoenix PLC-RSC-12DC/21-21</text>
  <text x="{X_OFF + 28}" y="386" class="small">DPDT reversing · shown released (coil off)</text>
  <text x="{X_OFF + 28}" y="402" class="small">Coil A1+ / A2− lands on Board B T11 / T10.</text>

  <!-- Pole 1 released: COM1 to NC1 -->
  <text x="{X_OFF + 36}" y="430" class="pin">Pole 1</text>
  <circle cx="{X_OFF + 56}" cy="448" r="4" fill="#b42318"/>
  <text x="{X_OFF + 70}" y="452" class="pin">NO1  14   fused +70V (Wago)</text>
  <circle cx="{X_OFF + 56}" cy="488" r="5" fill="#344054"/>
  <text x="{X_OFF + 70}" y="492" class="pin">COM1 11   MOTOR_A</text>
  <circle cx="{X_OFF + 56}" cy="528" r="4" fill="#d97706"/>
  <text x="{X_OFF + 70}" y="532" class="pin">NC1  12   ← T4  SW</text>
  <line x1="{X_OFF + 56}" y1="488" x2="{X_OFF + 56}" y2="524" class="wire"/>

  <text x="{X_OFF + 36}" y="564" class="pin">Pole 2</text>
  <circle cx="{X_OFF + 56}" cy="582" r="4" fill="#d97706"/>
  <text x="{X_OFF + 70}" y="586" class="pin">NO2  24   ← T5  SW</text>
  <circle cx="{X_OFF + 56}" cy="622" r="5" fill="#344054"/>
  <text x="{X_OFF + 70}" y="626" class="pin">COM2 21   MOTOR_B</text>
  <circle cx="{X_OFF + 56}" cy="662" r="4" fill="#b42318"/>
  <text x="{X_OFF + 70}" y="666" class="pin">NC2  22   fused +70V (Wago)</text>
  <line x1="{X_OFF + 56}" y1="622" x2="{X_OFF + 56}" y2="658" class="wire"/>

  <text x="{X_OFF + 28}" y="692" class="small">NO1 and NC2 take fused +70V from the Wago</text>
  <text x="{X_OFF + 28}" y="708" class="small">star. They do not pass through a Board C pin.</text>

  <circle cx="{X_OFF + 360}" cy="556" r="30" class="part"/>
  <text x="{X_OFF + 346}" y="552" class="label">M1</text>
  <text x="{X_OFF + 338}" y="568" class="small">39409A</text>
  <text x="{X_OFF + 328}" y="512" class="small">Lead A</text>
  <text x="{X_OFF + 328}" y="612" class="small">Lead B</text>
  <line x1="{X_OFF + 200}" y1="488" x2="{X_OFF + 330}" y2="538" class="wire"/>
  <line x1="{X_OFF + 200}" y1="622" x2="{X_OFF + 330}" y2="574" class="wire"/>

  <!-- T4 / T5 to K1: separate corridors, no shared gutter riser -->
  <path d="M1032 612 H1066 V528 H{X_OFF + 56}" class="sw"/>
  <path d="M1032 648 H1130 V582 H{X_OFF + 56}" class="sw"/>
  <circle cx="{X_OFF + 56}" cy="528" r="4" fill="#d97706"/>
  <circle cx="{X_OFF + 56}" cy="582" r="4" fill="#d97706"/>
  <text x="1072" y="518" class="swtext">T4</text>
  <text x="1072" y="666" class="swtext">T5</text>

  <rect x="{X_OFF + 16}" y="740" width="434" height="96" rx="5" class="box"/>
  <text x="{X_OFF + 28}" y="764" class="label">+70V feed (DIN HV island)</text>
  <text x="{X_OFF + 28}" y="786" class="small">+70V → F1 3 A → Wago → ACS712 → K1</text>
  <text x="{X_OFF + 28}" y="804" class="small">T1 branches after F1 for D9 and TVS1.</text>
  <text x="{X_OFF + 28}" y="822" class="small">ACS712 logic stays on +5V_EARTH / MCU_GND.</text>

  <rect x="{X_OFF + 16}" y="852" width="434" height="90" rx="5" class="part"/>
  <text x="{X_OFF + 28}" y="874" class="label">Private Reserve</text>
  <text x="{X_OFF + 28}" y="894" class="text">DWG  schematics/Board_C.svg</text>
  <text x="{X_OFF + 28}" y="914" class="text">REV  2026-08-22 · HV switch only</text>
  <text x="{X_OFF + 258}" y="874" class="small">Datasheets:</text>
  <text x="{X_OFF + 258}" y="890" class="small">IRFP460 · STTH8R06</text>
  <text x="{X_OFF + 258}" y="906" class="small">P6KE200A · BZX85C15</text>
  <text x="{X_OFF + 258}" y="922" class="small">PLC-RSC-12DC/21-21</text>

  <text x="40" y="1070" class="small">PWM-off, wait for current decay, then change K1, wait ≥25 ms, then resume PWM. Housing DC13 does not rate 140 VDC / 2 A inductive break.</text>
</svg>
"""

OUT.write_text(svg)
print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")
