---
phase: 01-build-system
reviewed: 2026-06-28T00:00:00Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - platformio.ini
  - partitions_8mb.csv
findings:
  critical: 0
  warning: 1
  info: 3
  total: 4
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-06-28
**Depth:** standard
**Files Reviewed:** 2
**Status:** issues_found

## Summary

Reviewed the two substantive build-system changes for the StickS3 migration:
`partitions_8mb.csv` (new) and the `platformio.ini` refactor into `[common]` +
dual envs (`m5stickc-plus`, `m5stack-sticks3`).

**Partition table is correct.** Verified arithmetic: offsets are contiguous with
no gaps or overlaps, app0 is 64KB-aligned at `0x10000`, and the table sums to
exactly `0x800000` (8 MB). Layout (nvs/otadata/app0 6.5 MB/spiffs 1.625 MB/coredump
64 KB) is sane for an N8R8 part.

**StickC Plus env is genuinely preserved.** Compared against the pre-refactor
single-env config: `board = m5stick-c`, `board_build.partitions = no_ota.csv`,
`board_build.f_cpu = 160000000L`, the LittleFS filesystem, monitor settings,
`build_src_filter`, and `CORE_DEBUG_LEVEL=0` all survive (the latter group via the
shared `[common]` section). `no_ota.csv` is a framework-provided partition CSV
(it was referenced identically before this change and is resolved from the
Arduino-ESP32 toolchain, not the repo), so its absence from the tree is expected.

**lib_deps migration is correct.** `m5stack/M5StickCPlus` removed; `M5Unified @ ^0.2.0`
and `M5GFX @ ^0.2.0` added; `AnimatedGIF @ ^2.1.1` and `ArduinoJson @ ^7.0.0` retained.

**No flag duplication or conflict.** The StickS3 env redefines `build_flags` but
re-includes `${common.build_flags}`, so `CORE_DEBUG_LEVEL=0` appears exactly once;
the StickC Plus env inherits `build_flags` via `extends` without redefinition.

The one substantive concern is PSRAM enablement on the StickS3 (below). The
expected staged-migration breakage (src/ still using the M5StickCPlus API until
Phase 3) is explicitly out of scope and not flagged.

## Warnings

### WR-01: StickS3 env does not define `-DBOARD_HAS_PSRAM` for the N8R8's OPI PSRAM

**File:** `platformio.ini:22-32`
**Issue:** The target is an ESP32-S3-PICO-1 **N8R8** — 8 MB flash *and* 8 MB OPI
PSRAM. `board_build.arduino.memory_type = qio_opi` correctly selects the OPI-PSRAM
precompiled SDK, but the `esp32-s3-devkitc-1` base board does not define
`BOARD_HAS_PSRAM`, and the env does not add it. Arduino-level PSRAM helpers
(`psramFound()`, `ps_malloc()`, `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`) and
M5GFX's PSRAM sprite/canvas allocation rely on this define being present in several
core versions. Omitting it risks PSRAM-backed allocations silently failing or
falling back to internal RAM — which matters here because the firmware drives M5GFX
plus AnimatedGIF frame buffers. This is also a divergence from the standard M5Stack
S3 / upstream PR #48 recipe, which sets the flag explicitly.
**Fix:** Add the define to the StickS3 env's `build_flags`:
```ini
build_flags =
    ${common.build_flags}
    -DBOARD_STICKS3
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
```
If PSRAM is intentionally deferred to a later phase, note that explicitly in the
phase summary so it is not lost.

## Info

### IN-01: `coredump` partition allocated but Arduino-ESP32 does not write to it by default

**File:** `partitions_8mb.csv:6`
**Issue:** A 64 KB `coredump` partition is reserved, but the Arduino-ESP32 core
does not enable core-dump-to-flash (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`) by
default, so without extra SDK config this 64 KB is unused.
**Fix:** Harmless to keep for future use. If the space is wanted for the
filesystem instead, drop the `coredump` row and extend `spiffs` size by `0x10000`
(new spiffs size `0x1B0000`, ending at `0x800000`). Keep the table summing to 8 MB.

### IN-02: `m5stack/M5GFX` is pinned explicitly though it is a transitive dependency of M5Unified

**File:** `platformio.ini:11-12`
**Issue:** M5Unified already depends on M5GFX; listing it separately is redundant.
It is pinned to a matching `^0.2.0` range here, so it is harmless and arguably
intentional (locking the transitive version), but it is one more version to keep in
sync if M5Unified's own M5GFX constraint ever moves.
**Fix:** Optional — leave as-is for an explicit pin, or remove the M5GFX line and
let M5Unified resolve it.

### IN-03: Data partition subtype is `spiffs` while the filesystem is LittleFS

**File:** `partitions_8mb.csv:5` / `platformio.ini:6`
**Issue:** `board_build.filesystem = littlefs` mounts the partition labeled
`spiffs`, so this works (matching the StickC Plus's `no_ota.csv` convention). The
`spiffs` subtype/label is just a naming artifact, not a bug — noted only so the
mismatch is not mistaken for a defect later.
**Fix:** None required. Optionally relabel for clarity, but keeping `spiffs`
preserves the Arduino LittleFS default-mount behavior with no code change.

---

_Reviewed: 2026-06-28_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
