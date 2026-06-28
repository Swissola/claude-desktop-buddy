---
phase: 02-compatibility-shim
plan: 01
subsystem: infra
tags: [compat-shim, m5unified, m5gfx, esp32-s3, platformio, software-rtc, board-conditional]

# Dependency graph
requires:
  - phase: 01-build-system
    provides: "Dual-env platformio.ini on M5Unified + M5GFX, BOARD_STICKS3 flag, 8MB partitions"
provides:
  - "src/compat.h — legacy M5StickCPlus name shim (TFT_eSprite/TFT_eSPI aliases, GREEN/RED macros)"
  - "Software RTC (RTC_*TypeDef + compatRtc{Set,GetTime,GetDate}) backed by the ESP32 system clock, UTC both directions, free-running with host-seed support"
  - "Board-conditional power helpers mapping AXP calls onto M5.Power/M5.Display (compatBatVoltage/Current/VBusVoltage/BatteryPct, compatScreenBreath/Backlight, compatPowerBtnShort/Off) + D-04 safe stubs (compatEnableCoulomb/RailSleep/RailWake)"
  - "compatOnUsb, no-op compatLed on StickS3, compatChipTempC"
  - "Proven standalone-compile surface for Phase 3 mechanical call-site swaps"
affects: [03-api-port, 04-haptics-chimes]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Board-conditional helper idiom: #if defined(BOARD_STICKS3) StickS3 path / #else native AXP192 path; M5.Axp.* confined to the #else branch so it is never parsed in the StickS3 TU"
    - "Software RTC over the ESP32 system clock (settimeofday/time/gmtime_r) with a portable _compatTimegm; never routed through M5.Rtc (no-op on chip-less boards)"
    - "Self-cleaning compile probe (isolated [env:compat-probe] + throwaway probe TU) to prove a header compiles standalone, then removed so net repo change is the header only"

key-files:
  created:
    - "src/compat.h"
  modified: []

key-decisions:
  - "Wrote the FULL compat* helper set in Phase 2 (not error-by-error in Phase 3) since the source-inventory mapping is fully specified — leaves Phase 3 as pure call-site swaps (Open-Question 1)"
  - "compatBacklight(false) emulates the absent AXP LDO2 rail via M5.Display.setBrightness(0); avoids M5.Display.sleep() (Open-Question 2)"
  - "compatBatteryPct routes to M5.Power.getBatteryLevel() on BOTH boards (away from the AXP coulomb path), keeping it compiling everywhere (D-04)"

patterns-established:
  - "All board-specific peripheral access goes through compat* helpers gated on BOARD_STICKS3"

requirements-completed: [SHIM-01]

# Metrics
duration: 14min
completed: 2026-06-28
---

# Phase 2 Plan 01: Compatibility Shim Summary

**`src/compat.h` re-creates the legacy M5StickCPlus API (TFT_eSprite/TFT_eSPI, software RTC, AXP/power helpers) over M5Unified + M5GFX, board-conditional on BOARD_STICKS3, and compiles standalone under the StickS3 toolchain with zero errors.**

## Performance

- **Duration:** ~14 min
- **Started:** 2026-06-28
- **Completed:** 2026-06-28
- **Tasks:** 2
- **Files modified:** 1 (`src/compat.h`)

## Accomplishments
- Created `src/compat.h` (138 lines): graphics typedefs (`TFT_eSprite`→`M5Canvas`, `TFT_eSPI`→`lgfx::LGFXBase`), `GREEN`/`RED` macros, software RTC, full board-conditional power/LED/chip-temp helper surface.
- Software RTC (`RTC_TimeTypeDef`/`RTC_DateTypeDef` + `compatRtcSet`/`compatRtcGetTime`/`compatRtcGetDate`) backed by the ESP32 system clock, seeded from the host `{"time":[epoch,tz]}` message (D-01) with an automatic free-running fallback when no host time is present (D-02). UTC both directions via `_compatTimegm`/`gmtime_r` so the host-localized value is never offset twice.
- AXP→`M5.Power`/`M5.Display` maps (D-03) + D-04 safe stubs, every board-specific helper gated on `#if defined(BOARD_STICKS3)` with all `M5.Axp.*` references confined to the non-StickS3 `#else` branch.
- Proved ROADMAP criterion 3: a translation unit including ONLY `compat.h` (+ M5Unified/M5GFX) compiles with zero errors under the StickS3 toolchain, via a self-cleaning compile probe.

## Criterion 3 Proof (durable evidence — probe is self-cleaning and gone)

`pio run -e compat-probe` (an isolated env extending `m5stack-sticks3`, `build_src_filter = -<*> +<compat_probe.cpp>`, compiling ONLY a TU that `#include "compat.h"` and references every shim symbol under `-DBOARD_STICKS3`) completed with **exit code 0**. Verbatim final lines from the build:

```
RAM:   [=         ]   8.3% (used 27216 bytes from 327680 bytes)
Flash: [=         ]   8.0% (used 525903 bytes from 6553600 bytes)
========================= [SUCCESS] Took 38.67 seconds =========================

Environment    Status    Duration
-------------  --------  ------------
compat-probe   SUCCESS   00:00:38.667
========================= 1 succeeded in 00:00:38.667 =========================
```

A confirmation re-run also returned exit code 0. The probe linked a full ESP32-S3 firmware image (bootloader + partitions + firmware.bin), so the compat.h-only TU compiled AND linked clean.

## Task Commits

1. **Task 1: Create src/compat.h (verbatim PR #48 base + full board-conditional helper set)** — `e46fb79` (feat)
2. **Task 2: Prove compat.h compiles standalone via self-cleaning probe** — no code commit. The probe compiled `src/compat.h` clean on the first attempt, so the file is unchanged from the Task 1 commit. Task 2's artifacts (`[env:compat-probe]` block in `platformio.ini` and `src/compat_probe.cpp`) were created, used to produce the criterion-3 proof above, then fully removed during the mandatory cleanup. Net repo change for the plan is `src/compat.h` only.

## Files Created/Modified
- `src/compat.h` — Legacy M5StickCPlus name shim over M5Unified/M5GFX: graphics aliases, `GREEN`/`RED`, software RTC, USB/LED/chip-temp helpers, and board-conditional power helpers with D-04 stubs.

## Decisions Made
- Wrote the complete `compat*` helper set now (vs. growing it error-by-error in Phase 3) — the legacy-API inventory is fully enumerated in RESEARCH.md, so the demands are already known; Phase 3 becomes mechanical call-site swaps (resolves Open-Question 1).
- `compatBacklight(false)` → `setBrightness(0)` to emulate the missing AXP LDO2 rail; avoids `M5.Display.sleep()` (resolves Open-Question 2).
- `compatBatteryPct()` returns `M5.Power.getBatteryLevel()` on both boards, deliberately avoiding the AXP coulomb-counter path (D-04 fallback) and keeping the helper compiling on StickC Plus too.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The Task 2 verify gate's literal `git status --porcelain src/ | grep -q "compat.h"` check assumed `compat.h` was still an uncommitted working-tree change. Because this sequential executor committed Task 1 atomically first (per the per-task commit protocol), `compat.h` is already tracked and no longer appears in `git status`. The gate's *intent* — compat.h present, probe gone, `platformio.ini` diff empty — was verified directly instead: `git ls-files src/compat.h` confirms it is tracked; `src/compat_probe.cpp` does not exist; `grep` finds no `compat-probe` in `platformio.ini`; `git diff --quiet -- platformio.ini` passes (empty diff). Substance satisfied; no code change needed.

## Scope / Criterion 4 Confirmation

- The `class TFT_eSPI;` forward declarations at `src/buddy.h:11` and `src/character.h:27` were left untouched (their `using`-alias conflict is deliberately deferred to Phase 3 / PORT-02).
- No UI/render source (`main.cpp`, `buddy.*`, `character.*`, `buddies/*`, `data.h`, `stats.h`, `xfer.h`) was edited.
- After probe cleanup, `git diff -- platformio.ini` is empty and `src/compat_probe.cpp` is gone — the plan's only net change is `src/compat.h`.

## Known Stubs

The D-04 no-op stubs (`compatEnableCoulomb`, `compatRailSleep`, `compatRailWake`) and the brightness-based `compatBacklight` on StickS3 are **intentional** per locked decision D-04 (AXP-only features with no clean StickS3 analog). They are not placeholders awaiting a future fix; they are the correct StickS3 behavior. The StickC Plus `#else` branch retains the real AXP192 calls. No stubs block the phase goal.

## Next Phase Readiness
- The complete shim surface is in place and compile-verified, so Phase 3 (API Port) is reduced to mechanical call-site swaps: replace `#include <M5StickCPlus.h>` with `#include "compat.h"`, swap `M5.Axp.*`/`M5.Rtc.*`/direct-LED/AXP-temp calls onto the `compat*` helpers, and remove the `class TFT_eSPI;` forward decls at `buddy.h:11` / `character.h:27` (the known Phase-3 redeclaration-conflict fix).
- No blockers introduced.

## Self-Check: PASSED

---
*Phase: 02-compatibility-shim*
*Completed: 2026-06-28*
