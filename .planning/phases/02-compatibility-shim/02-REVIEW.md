---
phase: 02-compatibility-shim
reviewed: 2026-06-28T00:00:00Z
depth: standard
files_reviewed: 1
files_reviewed_list:
  - src/compat.h
findings:
  critical: 0
  warning: 2
  info: 3
  total: 5
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-06-28
**Depth:** standard
**Files Reviewed:** 1 (`src/compat.h`, commit `e46fb79`)
**Status:** issues_found

## Summary

`src/compat.h` is a single board-conditional compatibility header that re-creates the
legacy M5StickCPlus API surface (`TFT_eSprite`/`TFT_eSPI` typedefs, `RTC_*TypeDef` + a
software RTC, and `compat*` power/LED/temp helpers) on top of M5Unified + M5GFX. It is the
PR #48 verbatim base (D-07) extended with the power/battery/brightness/sleep helpers this
fork additionally needs.

I verified the shim against the RESEARCH.md mapping table and the documented call-site
signatures. The core type-correctness is **sound** and should not produce Phase-3 compile
errors:

- Return types match every documented caller expectation: `compatBatVoltage/Current/VBusVoltage`
  return `float`, `compatBatteryPct` returns `int`, the void helpers are void, and
  `compatPowerBtnShort` returns `bool`.
- The unit shift in Pitfall 3 is handled correctly: voltage helpers divide mV by `1000.0f`
  (volts), while `compatBatCurrent` correctly does **not** divide (mA on both sides). This is
  the exact place a naive port goes wrong, and it is right here.
- The software-RTC UTC round-trip is correct. `_compatTimegm` matches libc `timegm` for all
  valid broken-down inputs I tested (incl. boundaries 1970-01-01, 2000-02-29, 2038-01-01), and
  the `timegm`/`gmtime_r` pairing avoids the double-timezone-offset trap given that `data.h`
  pre-localizes. Free-run before first host sync degrades sanely (returns a 1970 wall clock,
  gated downstream by `_rtcValid`), satisfying D-02.
- Board-conditional hygiene is correct: every `M5.Axp.*` reference is confined to the
  non-StickS3 `#else` branch, so `M5.Axp` is never parsed in the StickS3 TU. The D-04 stubs
  are type-correct no-ops. No name collisions, no missing includes; the header is self-contained.
- `compatLed*` no-op (RF-02/D-06) and `compatChipTempC` (D-05) are as specified.

No blockers. Two warnings concern defensive robustness and a brightness-restore divergence;
the info items flag StickC-Plus parity tradeoffs that surface only once Phase 3 swaps call sites.

## Warnings

### WR-01: `_compatTimegm` / `compatRtcSet` have no input validation — `mdays[-1]` out-of-bounds read when `Month == 0`

**File:** `src/compat.h:44` (and `:50-58`)
**Issue:** `compatRtcSet` computes `t.tm_mon = (int)dt->Month - 1`, then `_compatTimegm` indexes
`mdays[t->tm_mon % 12]`. The author added `% 12` to guard the high end, but C/C++ truncated
modulo leaves negatives negative: if `dt->Month == 0`, `tm_mon == -1`, and `-1 % 12 == -1`, so
`mdays[-1]` is an out-of-bounds read (UB — reads whatever precedes the static array, yielding a
garbage epoch). Symmetrically, an out-of-range `dt->Year` (e.g. 0) produces a wildly negative
epoch that `settimeofday` will accept. This is **not reachable from the current call path** —
`data.h::_applyJson` builds the structs from `gmtime_r`, which guarantees `Month` in 1..12 — but
`compatRtcSet` is a public helper with no bounds guard against a future/alternate caller.
**Fix:** Clamp defensively before use:
```cpp
int mon0 = (int)dt->Month - 1;
if (mon0 < 0 || mon0 > 11) mon0 = 0;          // or early-return without setting the clock
t.tm_mon = mon0;
```
and/or guard `_compatTimegm` with `int m = ((t->tm_mon % 12) + 12) % 12;` so a negative month
cannot index out of bounds.

### WR-02: `compatBacklight(true)` forces full brightness (255) on StickS3 instead of restoring the prior level

**File:** `src/compat.h:115-119`
**Issue:** On StickS3 the backlight rail is emulated via brightness: `setBrightness(on ? 255 : 0)`.
The "off" path (`0`) matches the research decision (Open-Q2: reversible by `applyBrightness`).
The "on" path hard-codes `255`, which clobbers any dimmed level previously set by
`compatScreenBreath` (e.g. an 8/100 → ~20/255 dim). The StickC-Plus `#else` path
(`M5.Axp.SetLDO2(on)`) restores the rail without altering the brightness register, so the two
boards diverge: a `compatBacklight(true)` not immediately followed by an `applyBrightness()` call
leaves the StickS3 panel at max brightness rather than its configured level — a visible
brightness glitch. The research only specified the off=0 behavior; the on=255 value is an
unstated assumption that `applyBrightness` always follows.
**Fix:** Cache the last non-zero brightness and restore it, instead of assuming 255:
```cpp
static uint8_t _compatLastBright = 255;
static inline void compatScreenBreath(int v) {
  _compatLastBright = (uint8_t)map(constrain(v,0,100),0,100,0,255);
  M5.Display.setBrightness(_compatLastBright);
}
static inline void compatBacklight(bool on) {
  M5.Display.setBrightness(on ? _compatLastBright : 0);
}
```
(If Phase-3 call sites always pair `compatBacklight(true)` with `applyBrightness()`, this is
harmless — but the shim should not depend on caller ordering it cannot see.)

## Info

### IN-01: `compatChipTempC` is not board-conditional — changes the StickC-Plus temp source

**File:** `src/compat.h:99`
**Issue:** `compatChipTempC()` calls `temperatureRead()` unconditionally for **both** boards. On
StickC Plus this replaces the legacy `M5.Axp.GetTempInAXP192()` reading with the classic-ESP32
internal sensor, which is documented (Pitfall 4) as inaccurate there. This is permitted by D-05
(chip temp is discretionary, non-blocking) but is one of the few places the StickC-Plus path is
*not* preserved unchanged once Phase 3 adopts the helper.
**Fix:** Optional — board-condition StickC Plus back to `M5.Axp.GetTempInAXP192()` if temp
fidelity matters there; otherwise accept per D-05 and leave as-is.

### IN-02: Software RTC (`compatRtc*`) is not board-conditional — StickC Plus loses coin-cell persistence

**File:** `src/compat.h:50-70`
**Issue:** The `compatRtc*` helpers back the wall clock with the ESP32 system clock for both
boards. RESEARCH.md frames this positively (the software clock "also benefits the StickC Plus,
works without its coin-cell RTC"), so this is intended. The tradeoff to record: where the
StickC-Plus coin-cell RTC survived a full power-off, the system clock does not — after Phase 3
adopts `compatRtc*`, StickC Plus also free-runs from boot until the next host time sync.
**Fix:** None required (matches research intent). Note for the planner if StickC-Plus
cross-power-off time persistence is a product requirement.

### IN-03: `compatBatteryPct` (`#else`) uses `M5.Power.getBatteryLevel()` rather than the legacy coulomb gauge

**File:** `src/compat.h:130`
**Issue:** On the StickC-Plus branch `compatBatteryPct` returns `M5.Power.getBatteryLevel()`
instead of the fork's existing coulomb-counter gauge (`stats.h`). This is the documented D-04
intent (route battery-% off the coulomb path; the inline comment states it) and compiles on both
boards, so it is correct — recorded only because it is another deliberate StickC-Plus behavior
change that lands when Phase 3 swaps the call sites.
**Fix:** None — intended per D-04.

---

_Reviewed: 2026-06-28_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
