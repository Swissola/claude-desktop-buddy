---
phase: 03-api-port
plan: "01"
subsystem: compat.h / power-shim
tags: [compat, power, m5unified, axp192, rf-04, d-08, d-10, button-macros]
dependency_graph:
  requires: []
  provides: [compat-power-block, compatBeep, BUTTON_A_PIN, BUTTON_B_PIN, MC_DATUM, TL_DATUM]
  affects: [src/main.cpp, src/stats.h, src/xfer.h]
tech_stack:
  added: []
  patterns:
    - board-agnostic M5.Power.* power helpers (single definition, both boards)
    - board-specific rail-cut inside #else (M5.Power.Axp192 on classic-ESP32 only)
key_files:
  created: []
  modified:
    - src/compat.h
decisions:
  - RF-04 resolved: collapsed the #if BOARD_STICKS3/#else power split to one board-agnostic M5.Power definition set; only rail-sleep/wake and LED remain board-specific
  - A1 (ASSUMED): restore voltage 3000mV for LDO2/LDO3 in compatRailWake — flagged for D-08 hardware smoke test in plan 03-05
  - O1 (RESOLVED): StickS3 BtnA=GPIO11 / BtnB=GPIO12 — confirmed from M5Unified.cpp board_M5StickS3 gpio_in(GPIO_NUM_11/12)
metrics:
  duration_minutes: 3
  completed_date: "2026-06-29"
  tasks_completed: 2
  files_modified: 1
---

# Phase 03 Plan 01: compat.h Power Block + New Helpers Summary

**One-liner:** Board-agnostic M5.Power power block with AXP192 rail-cut in #else, plus compatBeep/BUTTON macros/datum insurance defines — eliminating all M5.Axp references (RF-04).

## What Was Built

Amended `src/compat.h` to serve as the correct foundation for all Wave-2 port plans. Two commits:

### Task 1 — Board-agnostic power block (RF-04 fix)
- Replaced the entire `#if BOARD_STICKS3 / #else` power split (lines 104-148) with a single board-agnostic definition set using `M5.Power.*`.
- `_compatBrightness()` accessor moved out of the conditional to shared scope.
- All nine helpers (`compatBatVoltage`, `compatBatCurrent`, `compatVBusVoltage`, `compatBatteryPct`, `compatScreenBreath`, `compatBacklight`, `compatPowerBtnShort`, `compatPowerOff`, `compatEnableCoulomb`) now have exactly one definition, valid on both boards.
- `M5.Axp.*` references deleted from the `#else` branch entirely — 0 live references remain.
- `compatRailSleep` / `compatRailWake` kept board-specific: no-ops on `BOARD_STICKS3`; `M5.Power.Axp192.setLDO3/setLDO2` rail-cut on the classic-ESP32 `#else`. All five `M5.Power.Axp192.*` lines are strictly inside the `#else` branch (Pitfall 4 mitigated).
- Restore voltage annotated as `[ASSUMED A1 — confirm on hardware]`.

### Task 2 — New helpers
- `compatBeep(uint16_t freq, uint16_t dur)`: shims `M5.Beep` → `M5.Speaker.tone((float)freq, (uint32_t)dur)` (D-10). Board-safe on both boards. Speaker initialized by `M5.begin(cfg)` in plan 03-02 (D-09).
- `BUTTON_A_PIN` / `BUTTON_B_PIN`: board-conditional macros guarded with `#ifndef`. StickS3: GPIO 11/12 `[VERIFIED O1-RESOLVED]`; StickC Plus: GPIO 37/39 `[VERIFIED]`.
- `MC_DATUM` / `TL_DATUM`: `#ifndef` insurance defines (`middle_center` / `top_left`) to pre-empt potential undeclared-identifier errors in Wave-3 builds (RF-05, MEDIUM confidence).

## Verification

```
grep -v '^[[:space:]]*//' src/compat.h | grep -c 'M5\.Axp'  => 0  PASS
grep -c 'M5\.Power\.Axp192' src/compat.h                     => 5  PASS (>=4)
compatBeep, BUTTON_A_PIN, BUTTON_B_PIN, MC_DATUM, TL_DATUM   => all PRESENT
```

All M5.Power.Axp192 lines verified inside the `#else` (classic-ESP32) branch.

Note: `pio` compile is NOT run in this plan (per plan specification); the dual-env build verification is deferred to plan 03-05.

## Commits

| Task | Commit | Files | Description |
|------|--------|-------|-------------|
| 1 | be4cc85 | src/compat.h | Board-agnostic M5.Power power block, rail-cut #else |
| 2 | de2c600 | src/compat.h | compatBeep, BUTTON_A_PIN/B macros, MC/TL_DATUM defines |

## Deviations from Plan

None — plan executed exactly as written. The verbatim replacement block from RESEARCH.md §RF-04 was used for Task 1. Task 2 additions match RESEARCH.md §D-10, §NEW Blocker 2, and §RF-05.

## Known Stubs

None. All helpers are fully wired to verified M5Unified APIs. The only assumption (`A1 — 3000mV restore voltage`) is documented in a comment and flagged for hardware confirmation in plan 03-05.

## Threat Flags

No new security-relevant surface introduced. T-03-D1 (rail-cut `compatRailSleep`/`compatRailWake`) mitigated by using the verified `M5.Power.Axp192.setLDOx` register-level calls as specified in the threat register. The assumed 3000mV restore voltage is flagged for D-08 hardware confirmation (plan 03-05).

## Self-Check: PASSED

- `src/compat.h` exists and is modified: FOUND
- Task 1 commit be4cc85: FOUND (git log)
- Task 2 commit de2c600: FOUND (git log)
- All required symbols present in src/compat.h: PASS
- Zero live M5.Axp refs (non-comment): PASS
- M5.Power.Axp192 >= 4 occurrences all in #else branch: PASS
