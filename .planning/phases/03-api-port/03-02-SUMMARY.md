---
phase: 03-api-port
plan: "02"
subsystem: src/main.cpp
tags: [main, port, m5unified, ledc, beep, axp, rtc, idle-sleep, d-08, d-09, d-10, port-01, rf-03]
dependency_graph:
  requires: [03-01]
  provides: [main-cpp-port]
  affects: [src/main.cpp]
tech_stack:
  added: []
  patterns:
    - M5.begin(cfg) with explicit internal_imu/spk/mic flags (D-09)
    - core-3.x pin-based ledcAttach/ledcWrite (Blocker 1 fix)
    - compatRailSleep/Wake for idle rail-cut (RF-03/D-08)
    - M5.Imu.sleep/begin for board-agnostic IMU power management
    - compat helpers at every former M5.Axp/M5.Rtc/M5.Beep call site
key_files:
  created: []
  modified:
    - src/main.cpp
decisions:
  - D-09: M5.begin(cfg) with internal_imu=true, internal_spk=true, internal_mic=false, clear_display=true
  - Blocker 1: ledcSetup+ledcAttachPin removed; ledcAttach(VIBRATE_PIN,500,8) + ledcWrite(pin) throughout
  - VIBRATE_CH constant left in place (harmless; documented as unused in core-3.x)
  - Lines 263/273 (M5.Axp.SetSleep/WakeUpDisplayAfterLightSleep) handled in Task 2 to satisfy Task 2 verify gate (M5.Axp==0); Task 3 completed its unique work (Wire1 removal + imuSleep body)
  - VIBRATE_CH comment updated to remove stale M5.Beep reference
metrics:
  duration_minutes: 15
  completed_date: "2026-06-29"
  tasks_completed: 3
  files_modified: 1
---

# Phase 03 Plan 02: main.cpp Port Summary

**One-liner:** All 40+ legacy M5StickCPlus call sites in main.cpp replaced — include swap, D-09 init, core-3.x LEDC, idle-sleep RF-03 path, every M5.Axp/M5.Rtc/M5.Beep swapped to compat helpers.

## What Was Built

Ported `src/main.cpp` from direct M5StickCPlus API usage to the M5Unified + compat helper layer, satisfying PORT-01. Three atomic commits:

### Task 1 — Include swap + M5.begin(cfg) D-09 + core-3.x LEDC (Blocker 1)
- Swapped `#include <M5StickCPlus.h>` → `#include "compat.h"` (line 1).
- Replaced bare `M5.begin()` with the D-09 config block: `auto cfg = M5.config(); cfg.internal_imu = true; cfg.internal_spk = true; cfg.internal_mic = false; cfg.clear_display = true; M5.begin(cfg);`.
- Deleted `M5.Imu.Init()` and `M5.Beep.begin()` — both now handled by `cfg`.
- Replaced `pinMode(LED_PIN,OUTPUT)+digitalWrite(LED_PIN,HIGH)` → `compatLedInit()`.
- Migrated motor LEDC: `ledcSetup(VIBRATE_CH,500,8)+ledcAttachPin(VIBRATE_PIN,VIBRATE_CH)` → `ledcAttach(VIBRATE_PIN,500,8)`; all three `ledcWrite(VIBRATE_CH,x)` → `ledcWrite(VIBRATE_PIN,x)` (lines 103, 112, setup).
- Deleted `M5.Beep.update()` — M5.Speaker plays asynchronously (Pitfall 6 avoided).
- Cleaned LEDC comment to remove `ledcSetup+ledcAttachPin` from text (which would confuse the verify grep).

### Task 2 — All M5.Axp / M5.Rtc / M5.Beep.tone call sites
Applied all main.cpp port-map rows from RESEARCH.md:
- `applyBrightness()` (line 192) and nap handler (1625): `M5.Axp.ScreenBreath` → `compatScreenBreath`.
- `wake()` light-on (208), power-btn short-press (1435), auto-screen-off (1642): `M5.Axp.SetLDO2(false/true)` → `compatBacklight(false/true)`.
- `beep()` body (294): `M5.Beep.tone` → `compatBeep`.
- `menuConfirm()` (529): `M5.Axp.PowerOff()` → `compatPowerOff()`.
- `buttonsPoll()` (605), timer-wake gate (1290), busy-poll (1677): `M5.Axp.GetBtnPress()` → `compatPowerBtnShort()`.
- `clockRefreshRtc()` (628-630): `M5.Axp.GetVBusVoltage`, `M5.Rtc.GetTime/Date` → `compatVBusVoltage`, `compatRtcGetTime/GetDate`.
- DEVICE info page (879-881): `M5.Axp.GetBatVoltage/GetBatCurrent/GetVBusVoltage` → `compatBatVoltage/compatBatCurrent/compatVBusVoltage`.
- DEVICE info page (912): `M5.Axp.GetTempInAXP192()` → `compatChipTempC()`.
- LED pulse/off (1321, 1323): `digitalWrite(LED_PIN, LOW/HIGH)` → `compatLedSet(bool)`.
- `idlePowerDown/Restore` (263, 273-274): `M5.Axp.SetSleep()` → `compatRailSleep()`, `M5.Axp.WakeUpDisplayAfterLightSleep()` → `compatRailWake()`, `M5.Imu.Init()` → `M5.Imu.begin()` (pulled into Task 2 to satisfy Task 2 verify gate — see deviation note).
- Updated VIBRATE_CH comment to remove stale `M5.Beep` reference.
- `M5.Lcd.*` sites left completely intact (RF-05, 21 occurrences preserved).

### Task 3 — Idle-sleep power path (RF-03 / D-08)
- Replaced `imuSleep()` body (raw `Wire1.beginTransmission(0x68)` sequence) with a single `M5.Imu.sleep()` call — board-agnostic, covers both MPU6886 (StickC Plus) and BMI270 (StickS3).
- Updated surrounding comments to remove stale references to `Wire1` and `AXP SetSleep`.
- Generic light-sleep machinery (`esp_light_sleep_start`, `gpio_wakeup_enable`, `BUTTON_A_PIN`/`BUTTON_B_PIN` from `compat.h`) left completely unchanged (RF-03 scope).
- BLE `bleStopAdvertising`/`bleStartAdvertising` calls left unchanged.

## Verification Results

```
M5StickCPlus.h: 0          PASS
compat.h included: 1       PASS
M5.begin(cfg): 1           PASS
ledcSetup: 0               PASS
ledcAttachPin: 0           PASS
M5.Beep.begin/update: 0    PASS
ledcWrite(VIBRATE_CH: 0    PASS
M5.Axp: 0                  PASS
M5.Rtc: 0                  PASS
M5.Beep: 0                 PASS
M5.Lcd: 21                 PASS (untouched, RF-05)
Wire1: 0                   PASS
M5.Imu.sleep: 3            PASS (>=1)
compatRailSleep: 2         PASS (>=1)
compatRailWake: 1          PASS (>=1)
esp_light_sleep: 1         PASS (>=1)
compat helpers: 16         PASS (>=10)
```

## Commits

| Task | Commit | Files | Description |
|------|--------|-------|-------------|
| 1 | 335b788 | src/main.cpp | Include swap, M5.begin(cfg) D-09, core-3.x LEDC migration |
| 2 | 4700fdc | src/main.cpp | All M5.Axp/M5.Rtc/M5.Beep call sites → compat helpers |
| 3 | 1ac83cc | src/main.cpp | Idle-sleep path: imuSleep → M5.Imu.sleep(), comment cleanup |

## Deviations from Plan

### Task 2 absorbed lines 263 and 273 (planned for Task 3)

**Found during:** Task 2 verification
**Issue:** Task 2's acceptance criteria requires `grep -c 'M5.Axp' src/main.cpp` == 0, but lines 263 (`M5.Axp.SetSleep()`) and 273 (`M5.Axp.WakeUpDisplayAfterLightSleep()`) were listed in Task 3's port-map. Leaving them in Task 2 would fail the Task 2 verify gate.
**Fix:** Applied `compatRailSleep()` (line 263) and `compatRailWake()` (line 273) and `M5.Imu.begin()` (line 274) during Task 2. Task 3 still performed its unique work: replacing the raw `Wire1` body of `imuSleep()` with `M5.Imu.sleep()` and updating stale comments.
**Impact:** None — outcome is identical to the plan's intended final state; only the task boundary shifted.

### VIBRATE_CH comment updated (M5.Beep reference)

**Found during:** Task 2 — `grep -c 'M5.Beep' src/main.cpp` returned 1 after all functional sites were cleared.
**Issue:** The `VIBRATE_CH` constant's comment block still contained the text `M5.Beep (Speaker) uses TONE_PIN_CHANNEL 0` — a factual note about the old LEDC channel collision. This triggered the M5.Beep grep gate.
**Fix:** Rewrote the comment to note that `VIBRATE_CH` is now unused under core-3.x pin-based LEDC, preserving the historical rationale without the now-stale API name.
**Files modified:** src/main.cpp (comment only)

## Known Stubs

None. Every swapped call site is wired to a verified compat helper or M5Unified API. The only open item is:

- **[ASSUMED A1]** `compatRailWake()` restore voltage 3000mV for LDO2/LDO3. Flagged in `compat.h` comment and in plan 03-05 hardware smoke checklist. If the display comes back garbled after idle sleep on the StickC Plus, this voltage needs tuning.

## Threat Flags

No new security-relevant surface introduced. T-03-02-D1 (idle-sleep rail cut) is mitigated as specified: `compatRailSleep/Wake` wrap `M5.Power.Axp192.setLDOx` (verified register-level calls), `M5.Imu.sleep/begin` are the board-agnostic equivalents of the old raw-I2C approach, and the generic light-sleep + GPIO-wake machinery is unchanged.

## Self-Check: PASSED

- `src/main.cpp` modified: FOUND
- Task 1 commit 335b788: FOUND (git log)
- Task 2 commit 4700fdc: FOUND (git log)
- Task 3 commit 1ac83cc: FOUND (git log)
- M5.Axp, M5.Rtc, M5.Beep, Wire1 counts all 0: PASS
- ledcSetup, ledcAttachPin counts 0: PASS
- compat.h included, M5.begin(cfg) present: PASS
- M5.Lcd untouched (21 occurrences): PASS
- compatRailSleep/Wake, M5.Imu.sleep/begin all present: PASS
