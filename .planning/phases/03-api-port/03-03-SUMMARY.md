---
phase: 03-api-port
plan: "03"
subsystem: stats.h / xfer.h / data.h power + RTC port
tags: [compat, power, rtc, stats, xfer, data, coulomb-drop, d-04, d-08, o2, port-02]
dependency_graph:
  requires: ["03-01"]
  provides: [stats-h-ported, xfer-h-ported, data-h-ported]
  affects: [src/stats.h, src/xfer.h, src/data.h]
tech_stack:
  added: []
  patterns:
    - compatBatteryPct / M5.Power.getBatteryLevel() replaces AXP192 coulomb counter
    - compatRtcSet single-call replaces M5.Rtc.SetTime/SetDate pair
    - all power reads via compat wrappers (compatBatVoltage/Current/VBusVoltage)
key_files:
  created: []
  modified:
    - src/stats.h
    - src/xfer.h
    - src/data.h
decisions:
  - "O2 RESOLVED: coulomb gauge dropped; batteryPct() returns compatBatteryPct() (getBatteryLevel()) on both boards (D-04)"
  - "batteryFull() retained with compat voltage helpers for USB-detect and diagnostic comparisons"
  - "compatRtcSet atomic call preserves WR-01 month-clamp from Phase 2 (T-03-03-T1 mitigated)"
metrics:
  duration_minutes: 5
  completed_date: "2026-06-29"
  tasks_completed: 2
  files_modified: 3
---

# Phase 03 Plan 03: stats.h / xfer.h / data.h Port Summary

**One-liner:** Coulomb-gauge replaced with M5.Power.getBatteryLevel() on both boards; battery/VBus reads ported via compat wrappers; host time-seed routed through compatRtcSet — zero M5.Axp/M5.Rtc references remain.

## What Was Built

### Task 1 — Port stats.h (include + power swaps + coulomb-gauge drop)

- Added `#include "compat.h"` near the top of `src/stats.h`.
- `batteryInit()`: `M5.Axp.EnableCoulombcounter()` → `compatEnableCoulomb()` (no-op per D-04); removed now-unused NVS reads for `cb_full`/`cb_cal`.
- `batteryFull()`: all three `M5.Axp.Get*` calls replaced with `compatVBusVoltage()`, `compatBatVoltage()`, `compatBatCurrent()`.
- `batteryPctVoltage()`: `M5.Axp.GetBatVoltage()` → `compatBatVoltage()`.
- `batteryTick()`: reduced to no-op (no coulomb counter in M5Unified on either board; D-04 / O2).
- `batteryPct()`: rewritten as `return compatBatteryPct();` — delegates to `M5.Power.getBatteryLevel()`, a PMIC voltage estimate on both boards.
- Removed dead static state: `_battFullCoulomb`, `_battCalibrated`, `BATT_CAPACITY_MAH` (no longer referenced).
- Behavior change flagged: `[D-08 human_needed: battery-% sanity check on hardware — StickC Plus gauge source changed from coulomb integrator to getBatteryLevel() (PMIC estimate)]`.

### Task 2 — Port xfer.h + data.h

**xfer.h:**
- `#include <M5StickCPlus.h>` (line 75) → `#include "compat.h"`.
- Status command handler (lines 115-117): three `M5.Axp.Get*` reads replaced with `compatBatVoltage()`, `compatBatCurrent()`, `compatVBusVoltage()`.
- Updated stale comment on the `pct` line to reflect `getBatteryLevel()` source.

**data.h:**
- Added `#include "compat.h"` explicitly at the top (before `ArduinoJson.h`) for clarity, even though it arrives transitively via `xfer.h` → `stats.h`.
- `M5.Rtc.SetTime(&tm); M5.Rtc.SetDate(&dt);` replaced with the single Phase-2 combined helper `compatRtcSet(&tm, &dt);` — the WR-01 month-clamp inside `compatRtcSet` is preserved intact (T-03-03-T1 mitigated).

## Verification

```
grep -c 'M5.Axp'        src/stats.h  => 0  PASS
grep -c 'GetCoulombData' src/stats.h  => 0  PASS
grep -c 'compat.h'      src/stats.h  => 1  PASS
grep -c 'compatBatteryPct|compatBatVoltage|compatVBusVoltage|compatEnableCoulomb' src/stats.h => 6  PASS (>=3)
grep -c 'M5.Axp'        src/xfer.h   => 0  PASS
grep -c 'M5StickCPlus.h' src/xfer.h  => 0  PASS
grep -c 'compatBatVoltage|compatBatCurrent|compatVBusVoltage' src/xfer.h => 3  PASS (>=3)
grep -c 'M5.Rtc'        src/data.h   => 0  PASS
grep -c 'compatRtcSet'  src/data.h   => 1  PASS (>=1)
grep -c 'compat.h'      src/data.h   => 1  PASS (>=1)
```

## Commits

| Task | Commit | Files | Description |
|------|--------|-------|-------------|
| 1 | 6a20fe6 | src/stats.h | include compat.h + coulomb-gauge drop (D-04/O2) |
| 2 | 28f8813 | src/xfer.h, src/data.h | compat helpers + compatRtcSet (PORT-02) |

## Deviations from Plan

None — plan executed exactly as written. The RESEARCH.md port map was followed verbatim for all three files. The O2 RECOMMENDED minimal path (drop coulomb gauge, use getBatteryLevel()) was applied as directed.

## Known Stubs

None. All compat helpers are fully wired to verified M5Unified APIs (per plan 03-01 SUMMARY). `compatEnableCoulomb()` is intentionally a no-op (D-04) — documented in both compat.h and stats.h. The `[ASSUMED A1 — 3000mV restore]` in compatRailWake is inherited from plan 03-01 and flagged for hardware confirmation in plan 03-05.

## Threat Flags

No new security surface introduced.
- **T-03-03-T1 (Tampering — data.h host time-seed):** Mitigated — route now goes through `compatRtcSet` which contains the WR-01 month-clamp (`int mon = t->tm_mon < 0 ? 0 : (t->tm_mon > 11 ? 11 : t->tm_mon)`). The trust boundary is identical to pre-port (bonded/encrypted BLE peer).
- **T-03-03-I1 (battery/VBus telemetry over BLE):** Accepted — pre-existing telemetry; no new exposure.

## Self-Check: PASSED

- `src/stats.h` exists and modified: FOUND
- `src/xfer.h` exists and modified: FOUND
- `src/data.h` exists and modified: FOUND
- Task 1 commit 6a20fe6: FOUND (git log)
- Task 2 commit 28f8813: FOUND (git log)
- Zero M5.Axp refs across all three files: PASS
- Zero M5.Rtc refs across all three files: PASS
- Zero GetCoulombData refs: PASS
- compatRtcSet in data.h: PASS
- compat helpers >= 3 in each of stats.h and xfer.h: PASS
