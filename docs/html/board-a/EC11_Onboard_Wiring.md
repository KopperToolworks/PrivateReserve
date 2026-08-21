# Board A — EC11 on-board wiring

The **EC11** menu encoder mounts on the Board A Perma-Proto carrier, to the
**right** of the T-Display-S3 (opposite USB-C). It is **not** wired through
screw terminals.

Signals run as **short on-board leads** from the EC11 breakout to the
T-Display-S3 **bottom** header pins. GPIO11, GPIO12, and GPIO13 never leave
the carrier on field cables.

**Terminal map (T1–T24):** [`index.html`](index.html)  

---

## EC11 → T-Display-S3

View with the display facing you and USB-C on the left. The bottom header uses
the pinout **LEFT** row (pin 12 nearest USB).

| EC11 breakout | T-Display header | GPIO | Net |
|---------------|------------------|------|-----|
| **CLK** | LEFT GPIO11 | **GPIO11** | Quadrature A |
| **DT** | LEFT GPIO12 | **GPIO12** | Quadrature B |
| **SW** | LEFT GPIO13 | **GPIO13** | Push (active-low) |
| **`+`** | LEFT or RIGHT **3V** | `+3V3_TDISPLAY` | 3.3 V only (breakout 10 kΩ pull-ups) |
| **GND** | header **GND** | `MCU_GND` | Common return |

Do not use **5 V** on these GPIOs. The breakout **`+`** pin expects 3.3 V.

On-board jumper routing (Perma-Proto rows/columns) is documented separately.
This page only records **which header pins** the encoder uses.

---

## Screw terminals T17–T19 are NC

Three bottom-edge screw terminals (**T17**, **T18**, **T19**) sit in the
same left-to-right index as GPIO13, GPIO12, and GPIO11. On the as-built
carrier they are **not connected**. The EC11 wires go directly to the
T-Display header, not to those blocks.

| Terminal | Header column (display-up) | As-built |
|----------|----------------------------|----------|
| **T17** | LEFT GPIO13 | NC — EC11 **SW** is on-board from the header |
| **T18** | LEFT GPIO12 | NC — EC11 **DT** is on-board from the header |
| **T19** | LEFT GPIO11 | NC — EC11 **CLK** is on-board from the header |

Do not land field cables on T17–T19 expecting encoder signals.

---

## Related

- [`index.html`](index.html) — Board A placement and screw-terminal table
