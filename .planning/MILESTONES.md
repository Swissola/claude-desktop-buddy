# Milestones

## v1.0 StickS3 + Chimes (Shipped: 2026-06-30)

**Phases completed:** 4 phases, 9 plans, 20 tasks

**Key accomplishments:**

- Refactored platformio.ini into a shared [common] section plus m5stickc-plus and m5stack-sticks3 envs on M5Unified + M5GFX, and added an 8MB no-OTA partitions_8mb.csv; the new StickS3 env resolves all deps + partitions and reaches the source-compile stage (old-API source errors deferred to Phase 3).
- `src/compat.h` re-creates the legacy M5StickCPlus API (TFT_eSprite/TFT_eSPI, software RTC, AXP/power helpers) over M5Unified + M5GFX, board-conditional on BOARD_STICKS3, and compiles standalone under the StickS3 toolchain with zero errors.
- Ported `main.cpp` and the buddy/character/stats code from the M5StickCPlus library to the M5Unified + M5GFX unified API; both `m5stickc-plus` and `m5stack-sticks3` envs build zero-error from one tree (Phase 3).
- Resolved a 3-bug StickS3 first-boot bootloop: PSRAM init crash (disabled SPIRAM), NULL sprite `_parent` (push to `&M5.Display`), and an HWCDC `available() == -1` splash freeze (`> 0` guard) — device boots splash → buddy → stable (Phase 3 debug).
- Board-conditional chiptune chime engine on the StickS3 ES8311 speaker: five audibly-distinct event chimes via `M5.Speaker`, UI-beep precedence, "chime"/"vibrate" relabel, and a persisted 6-step volume cycler — hardware-verified; the StickC Plus LEDC motor path is byte-for-byte unchanged (Phase 4).
- StickS3 quality-of-life defaults: octopus buddy, 60% brightness, 12-hour clock, plus an IMU X/Y-swap fix for clock auto-rotation.
- Root-caused and fixed a battery-only ~15s reboot loop (throttling the CPU to 40MHz while BLE was live starved the ESP32-S3 APB clock) via a board-conditional `compatSetCpuMhz()` no-op — hardware-confirmed (post-Phase-4 debug session).

**Known deferred follow-ups at close:** 4 (firm up connected-idle power saving, restore encrypted BLE bond, format LittleFS, eyeball clock auto-rotation) — see `.planning/todos/pending/`.

---
