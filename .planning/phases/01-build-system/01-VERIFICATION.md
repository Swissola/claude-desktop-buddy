---
phase: 01-build-system
verified: 2026-06-28T00:00:00Z
status: passed
score: 5/5 must-haves verified
overrides_applied: 0
re_verification:
  previous_status: none
gaps: []
---

# Phase 01: Build System Verification Report

**Phase Goal:** A dual-environment PlatformIO build stands up on M5Unified + M5GFX, with the StickS3 env wired to 8MB flash partitions, replacing the board-specific M5StickCPlus library.
**Verified:** 2026-06-28
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | platformio.ini exposes `[common]` + two envs (m5stickc-plus, m5stack-sticks3) building from the same src/ tree (BUILD-01) | ✓ VERIFIED | `[common]` (L1), `[env:m5stickc-plus]` (L16) + `[env:m5stack-sticks3]` (L22), both `extends = common`; `build_src_filter = +<*> +<buddies/>` only in `[common]` so both envs share src/ |
| 2 | Deps resolve M5Unified @ ^0.2.x + M5GFX, AnimatedGIF + ArduinoJson retained, no M5StickCPlus (BUILD-02) | ✓ VERIFIED | lib_deps L10-14: M5Unified ^0.2.0, M5GFX ^0.2.0, AnimatedGIF ^2.1.1, ArduinoJson ^7.0.0; `grep -c M5StickCPlus platformio.ini` = 0; live build resolved M5Unified@0.2.17, M5GFX@0.2.24, AnimatedGIF@2.2.2, ArduinoJson@7.4.3 |
| 3 | partitions_8mb.csv exists (8MB no-OTA, sums to 0x800000), referenced by StickS3 env via board_build.partitions (BUILD-03) | ✓ VERIFIED | File present: nvs/otadata/app0(0x640000)/spiffs/coredump; last partition 0x7F0000+0x10000 = 0x800000 exactly; `board_build.partitions = partitions_8mb.csv` (L27); build reported "ESP32-S3-DevKitC-1-N8 (8 MB QD)" with no partition-overflow error |
| 4 | `pio run -e m5stack-sticks3` resolves deps + partitions and reaches the source-compile stage; old-API source errors are the expected stopping point | ✓ VERIFIED | Live build: platform/board/toolchain resolved, all 4 libs resolved, partitions accepted, libraries compiled, then began `Compiling .../src/ble_bridge.cpp.o`, `.../buddies/*.cpp.o` and failed on old-API errors (`esp_ble_remove_bond_device` etc.) — exactly the deferred Phase 3 surface. NO config-level errors (no Unknown board / package-not-found / partition overflow) |
| 5 | m5stickc-plus env retains original board/partition/f_cpu config (no regression) | ✓ VERIFIED | L18-20: `board = m5stick-c`, `board_build.partitions = no_ota.csv`, `board_build.f_cpu = 160000000L`. Git diff vs `8ac960d`: original used the same `no_ota.csv` (framework built-in) + `m5stick-c`; only change is M5StickCPlus removal + inheritance from `[common]` |

**Score:** 5/5 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `partitions_8mb.csv` | 8MB flash partition table, contains app0 | ✓ VERIFIED | Present at repo root; app0 (app/ota_0, 0x640000); arithmetic sums to 0x800000; accepted by live build |
| `platformio.ini` | Dual-env config on M5Unified+M5GFX, contains `[common]` | ✓ VERIFIED | `[common]` + 2 envs; M5Unified/M5GFX in lib_deps; M5StickCPlus removed |

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `[env:m5stack-sticks3]` | partitions_8mb.csv | board_build.partitions | ✓ WIRED | L27 `board_build.partitions = partitions_8mb.csv`; build resolved the table |
| `[env:*]` | `[common]` | extends | ✓ WIRED | Both envs `extends = common` (L17, L23) |
| `[common] lib_deps` | M5Unified + M5GFX | PlatformIO registry | ✓ WIRED | Resolved to M5Unified@0.2.17, M5GFX@0.2.24 in live build |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| StickS3 env reaches source-compile stage | `pio run -e m5stack-sticks3` | Deps+partitions resolved; compiled libs; reached `src/*.cpp` compile; stopped at old-API source errors | ✓ PASS (per Phase 1 bar) |
| No config-level resolution failure | grep build log for Unknown board / package-not-found / partition overflow | NONE found | ✓ PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| BUILD-01 | 01-01-PLAN | `[common]` + two envs from same src/ tree | ✓ SATISFIED | Truth 1 |
| BUILD-02 | 01-01-PLAN | M5StickCPlus replaced by M5Unified+M5GFX, AnimatedGIF/ArduinoJson kept | ✓ SATISFIED | Truth 2 |
| BUILD-03 | 01-01-PLAN | partitions_8mb.csv added + referenced by StickS3 env | ✓ SATISFIED | Truth 3 |

No orphaned requirements — REQUIREMENTS.md maps exactly BUILD-01/02/03 to Phase 1, all declared in plan frontmatter.

### Anti-Patterns Found

None. Modified files are config (platformio.ini, partitions_8mb.csv) — no stubs, debt markers, or hollow implementations.

### Notes (Info — not gaps)

- The build's PLATFORM line reads "8 MB QD, No PSRAM" — the board manifest default display string. The env sets `board_build.arduino.memory_type = qio_opi` for the N8R8 OPI PSRAM. This does not affect the Phase 1 compile-stage bar; PSRAM runtime availability is a hardware concern for later phases and should be confirmed when flashing real StickS3 hardware (Phase 3+).

### Gaps Summary

None. The dual-environment build config and 8MB partition table are in place, M5StickCPlus is removed in favor of M5Unified+M5GFX, the m5stickc-plus env is preserved unchanged, and the StickS3 env resolves dependencies + partition table and reaches the source-compile stage. The build stops only at the expected old-M5StickCPlus-API source errors in `src/ble_bridge.cpp` / `src/buddies/*.cpp`, which are deliberately deferred to Phase 2 (compat.h) and Phase 3 (API port) per the phase's success criteria. This is the Phase 1 PASS condition.

---

_Verified: 2026-06-28_
_Verifier: Claude (gsd-verifier)_
