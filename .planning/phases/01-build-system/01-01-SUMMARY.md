---
phase: 01-build-system
plan: 01
subsystem: infra
tags: [platformio, esp32-s3, m5unified, m5gfx, partition-table, build-system]

# Dependency graph
requires: []
provides:
  - "Dual-environment PlatformIO build ([common] + m5stickc-plus + m5stack-sticks3) from a single src/ tree"
  - "M5Unified ^0.2.x + M5GFX dependency baseline (M5StickCPlus library removed)"
  - "partitions_8mb.csv 8MB no-OTA partition table for the StickS3 (N8R8) env"
affects: [02-compat-shim, 03-api-port, chimes]

# Tech tracking
tech-stack:
  added: ["m5stack/M5Unified @ ^0.2.0", "m5stack/M5GFX @ ^0.2.0"]
  patterns:
    - "Shared [common] PlatformIO section extended by per-board envs (one tree, two boards)"
    - "StickS3 env selects N8R8 module via board=esp32-s3-devkitc-1 + memory_type=qio_opi + flash_size=8MB"

key-files:
  created: [partitions_8mb.csv, .planning/phases/01-build-system/01-01-SUMMARY.md]
  modified: [platformio.ini]

key-decisions:
  - "Used board = esp32-s3-devkitc-1 (generic S3) + board_build.arduino.memory_type = qio_opi for the N8R8 module, since no official M5StickS3 board id exists"
  - "StickS3 env re-includes ${common.build_flags} when redefining build_flags so -DCORE_DEBUG_LEVEL=0 is not dropped"

patterns-established:
  - "[common] base section + extends = common per env keeps the m5stickc-plus config preserved while adding the StickS3 env"
  - "Native USB-CDC on StickS3 via -DARDUINO_USB_CDC_ON_BOOT=1 / -DARDUINO_USB_MODE=1"

requirements-completed: [BUILD-01, BUILD-02, BUILD-03]

# Metrics
duration: 6min
completed: 2026-06-28
---

# Phase 01 Plan 01: Dual-Environment Build System Summary

**Refactored platformio.ini into a shared [common] section plus m5stickc-plus and m5stack-sticks3 envs on M5Unified + M5GFX, and added an 8MB no-OTA partitions_8mb.csv; the new StickS3 env resolves all deps + partitions and reaches the source-compile stage (old-API source errors deferred to Phase 3).**

## Performance

- **Duration:** ~6 min
- **Started:** 2026-06-28T18:13:52Z
- **Completed:** 2026-06-28
- **Tasks:** 3
- **Files modified:** 2 (1 created, 1 modified)

## Accomplishments
- `partitions_8mb.csv` created: nvs / otadata / app0 (0x640000) / spiffs / coredump layout summing to exactly 0x800000, app0 0x10000-aligned (BUILD-03).
- `platformio.ini` refactored into `[common]` + two envs both building from the same `src/` tree, with `extends = common` (BUILD-01).
- Dependencies migrated: `m5stack/M5Unified @ ^0.2.0` + `m5stack/M5GFX @ ^0.2.0` added; `m5stack/M5StickCPlus` removed entirely; `bitbank2/AnimatedGIF` and `bblanchon/ArduinoJson` retained (BUILD-02).
- `pio run -e m5stack-sticks3` reached the source-compile stage — see acceptance results below.

## Acceptance Criteria Results

- **lib_deps resolution:** PASS — M5Unified@0.2.17, M5GFX@0.2.24, AnimatedGIF@2.2.2, ArduinoJson@7.4.3 all installed; no "library not found" / version-conflict error.
- **Partition table accepted:** PASS — partitions_8mb.csv accepted; no "partitions do not fit" / overflow error. Platform reported `ESP32-S3-DevKitC-1-N8 (8 MB QD), 8MB Flash`.
- **Reached source-compile stage:** PASS (source errors expected/deferred) — the build compiled all libraries (M5Unified, M5GFX, BLE, LittleFS, etc.) and began compiling `src/` files, then stopped at `src/buddies/axolotl.cpp:3:10: fatal error: M5StickCPlus.h: No such file or directory` plus old-API errors in `src/ble_bridge.cpp`. These are the **expected** old-M5StickCPlus-API source errors, deliberately deferred to Phase 2 (compat.h) / Phase 3 (API port). This is the Phase 1 PASS condition, NOT a build-system failure.
- **No real resolution failure:** PASS — no "Unknown board", "could not find the package", partition overflow, or UnknownPackageError.
- **m5stickc-plus env preserved:** PASS — still contains `board = m5stick-c`, `board_build.partitions = no_ota.csv`, `board_build.f_cpu = 160000000L`; all other settings inherited from `[common]` (unchanged behaviour vs. the original single env).

## Task Commits

1. **Task 1: Add partitions_8mb.csv (8MB no-OTA partition table)** - `f2662fc` (feat)
2. **Task 2: Refactor platformio.ini into [common] + two envs** - `4d533dc` (feat)
3. **Task 3: Build-smoke the StickS3 env to the compile stage** - no source change (build verification only; `.pio/` is gitignored)

## Files Created/Modified
- `partitions_8mb.csv` - 8MB no-OTA partition table for the StickS3 N8R8 env; sums to 0x800000.
- `platformio.ini` - `[common]` + `m5stickc-plus` + `m5stack-sticks3` envs on M5Unified + M5GFX.

## Decisions Made
- `board = esp32-s3-devkitc-1` + `board_build.arduino.memory_type = qio_opi` + `board_upload.flash_size = 8MB` selects the N8R8 module, since no official M5StickS3 board id exists (Claude's-discretion per plan).
- StickS3 env redefines `build_flags` and re-includes `${common.build_flags}` so `-DCORE_DEBUG_LEVEL=0` is preserved alongside `-DBOARD_STICKS3`, `-DARDUINO_USB_CDC_ON_BOOT=1`, `-DARDUINO_USB_MODE=1`.

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
- During M5GFX download PlatformIO emitted a transient `Package Mirror: [Errno 9] Bad file descriptor` warning and automatically fell back to another mirror; the install succeeded. No action required.

## Known Stubs
None.

## Threat Flags
None — no new security surface introduced beyond the build-system changes covered by the plan's threat model (pinned, namespaced lib_deps; validated partition arithmetic).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- The one-tree / two-board build foundation is in place. The StickS3 env now compiles cleanly up to the source layer.
- Phase 2 can introduce `src/compat.h` to remap the old M5StickCPlus API onto M5Unified/M5GFX; the specific failing source surface observed is `src/buddies/*.cpp` (`#include <M5StickCPlus.h>`) and `src/ble_bridge.cpp` (BLE callback signatures / `esp_ble_*` symbols / `String`→`std::string`). These are deferred and expected.

## Self-Check: PASSED

- FOUND: partitions_8mb.csv
- FOUND: platformio.ini
- FOUND: .planning/phases/01-build-system/01-01-SUMMARY.md
- FOUND commit: f2662fc (Task 1)
- FOUND commit: 4d533dc (Task 2)

---
*Phase: 01-build-system*
*Completed: 2026-06-28*
