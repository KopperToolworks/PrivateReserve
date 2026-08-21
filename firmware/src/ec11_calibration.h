#pragma once

#include <Arduino.h>

// EC11 quadrature → detent scaling.
// Procedure and hardware notes: docs/components/parts/EC11/README.md
//
// A typical EC11 is 4 quadrature edges per mechanical detent. The 2026-08-11
// bench ratio (3.62 = 362/100) was slightly greedy: leftover counts plus
// bounce could emit two menu steps from one detent. 4.00 needs a full
// detent of edges before the next item.
constexpr int32_t kEc11DetentScaleNumerator = 100;
constexpr int32_t kEc11DetentScaleDenominator = 400;

// HOLD back window. Hold fires once when the button has been down for at
// least min, and not again until release. A release before min is PRESS.
// max is the end of the window (no auto-repeat if the finger stays down).
constexpr uint32_t kEc11HoldMinMs = 400;
constexpr uint32_t kEc11HoldMaxMs = 1800;

// If the highlight moved this close to an EC11 PRESS, treat the move as
// shaft twist from the click and restore the previous item.
constexpr uint32_t kEc11PressRevertMs = 100;

// Two-choice pages (limit tiles, rocker throws) wrap 0↔1, so one bounce
// detent looks like the highlight thrashing. Ignore further TURN this long
// after a click, and clamp instead of wrapping.
constexpr uint32_t kEc11TwoChoiceSettleMs = 220;
