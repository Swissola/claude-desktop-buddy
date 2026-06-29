# Roadmap: Claude Desktop Buddy — StickS3 + Chimes Migration

## Overview

This milestone takes the fork's M5StickC Plus-only firmware and makes it build and run on the M5StickS3 (ESP32-S3) from a single shared source tree, without regressing the working StickC Plus device. The journey is linear: first stand up the dual-environment build on M5Unified + M5GFX (Phase 1), then re-create the legacy library's names in a `compat.h` shim so the ~3k lines of UI/render code stay untouched (Phase 2), then port `main.cpp` and the buddy/character code to the unified API until both envs compile clean (Phase 3), and finally replace haptic events with audible ES8311 chimes on the StickS3 while keeping the StickC Plus LEDC motor path intact (Phase 4). Each phase ends with a verifiable build or device behaviour.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Build System** - Dual-env `platformio.ini` on M5Unified + M5GFX with 8MB partitions (completed 2026-06-28)
- [x] **Phase 2: Compatibility Shim** - `src/compat.h` re-creates legacy names so UI code stays untouched (completed 2026-06-28)
- [ ] **Phase 3: API Port** - Port `main.cpp` + buddy/character code to the unified API; both envs build clean
- [ ] **Phase 4: Haptics → Chimes** - Board-conditional ES8311 chimes on StickS3, LEDC motor kept on StickC Plus

## Phase Details

### Phase 1: Build System

**Goal**: A dual-environment PlatformIO build stands up on M5Unified + M5GFX, with the StickS3 env wired to 8MB flash partitions, replacing the board-specific M5StickCPlus library.
**Depends on**: Nothing (first phase)
**Requirements**: BUILD-01, BUILD-02, BUILD-03
**Success Criteria** (what must be TRUE):

  1. `platformio.ini` exposes a shared `[common]` section plus two envs, `m5stickc-plus` and `m5stack-sticks3`, both pointing at the same `src/` tree
  2. The dependency list resolves `m5stack/M5Unified @ ^0.2.x` + `M5GFX` (with `AnimatedGIF` and `ArduinoJson` retained) and no longer references the `M5StickCPlus` library
  3. `partitions_8mb.csv` exists (8MB flash, QIO-OPI, USB-CDC-on-boot) and the `m5stack-sticks3` env references it via `board_build.partitions`
  4. `pio run -e m5stack-sticks3` reaches the compile stage on the new toolchain (dependency + partition resolution succeeds; source-level API errors are expected and addressed in Phase 3)

**Plans**: 1 plan
Plans:

- [x] 01-01-PLAN.md — Dual-env platformio.ini on M5Unified + M5GFX with 8MB partitions (BUILD-01/02/03)

### Phase 2: Compatibility Shim

**Goal**: A `src/compat.h` shim re-creates the legacy M5StickCPlus names on top of M5Unified/M5GFX so the existing UI and render code compiles against the new libraries without edits.
**Depends on**: Phase 1
**Requirements**: SHIM-01
**Success Criteria** (what must be TRUE):

  1. `src/compat.h` defines the legacy type aliases `TFT_eSprite`→`M5Canvas` and `TFT_eSPI`→`lgfx::LGFXBase`
  2. The shim provides a software RTC and the `compatOnUsb` / `compatLed` / `compatChipTempC` helpers so no UI code calls AXP/RTC/LED APIs directly
  3. A translation unit that includes only `compat.h` (plus M5Unified/M5GFX) compiles with zero errors under `pio run -e m5stack-sticks3`
  4. The UI/render source files remain unedited — the shim absorbs all name changes

**Plans**: 1 plan
Plans:

- [x] 02-01-PLAN.md — Create src/compat.h (verbatim PR #48 base + full board-conditional helper set) and prove it compiles standalone under the StickS3 env (SHIM-01)

### Phase 3: API Port

**Goal**: `main.cpp` and the buddy/character code are moved onto the unified API through `compat.h`, so both board environments compile cleanly from the identical source.
**Depends on**: Phase 2
**Requirements**: PORT-01, PORT-02, PORT-03
**Success Criteria** (what must be TRUE):

  1. `src/main.cpp` includes `compat.h` instead of `M5StickCPlus.h` and uses `M5.Power`, `compatRtc*`, `compatLed*`, and `compatChipTempC` in place of the old `M5.Axp` / `M5.Rtc` / direct-LED / AXP-temp calls
  2. `src/buddies/*.cpp` (including doge + llama), `src/character.*`, and `src/buddy.*` are updated for the include/type changes
  3. `pio run -e m5stack-sticks3` builds with zero errors
  4. `pio run -e m5stickc-plus` still builds green from the same source (no regression to the working device)

**Plans**: 5 plansPlans:
**Wave 1**

- [x] 03-01-PLAN.md — Amend compat.h: board-agnostic M5.Power power block (RF-04) + rail-cut idle-sleep (RF-03/D-08) + compatBeep (D-10) + button-pin macros (PORT-01/03)

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 03-02-PLAN.md — Port main.cpp: include swap + M5.begin(cfg) (D-09) + core-3.x LEDC migration + all M5.Axp/M5.Rtc/M5.Beep swaps + idle-sleep path (PORT-01)
- [ ] 03-03-PLAN.md — Port stats.h/xfer.h/data.h: power-helper swaps + coulomb-gauge drop (O2) + compatRtcSet time-seed (PORT-02)
- [ ] 03-04-PLAN.md — Fix TFT_eSPI forward-decl conflicts + swap M5StickCPlus.h include across buddy/character + all 20 buddies (PORT-02)

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 03-05-PLAN.md — Dual-env build (m5stickc-plus first) to zero-error + D-08 StickC-Plus hardware smoke checklist (PORT-03)

### Phase 4: Haptics → Chimes

**Goal**: Haptic events become audible chimes on the StickS3 via `M5.Speaker` (ES8311), while the StickC Plus keeps its LEDC vibration-motor path, and the settings UI reflects the change.
**Depends on**: Phase 3
**Requirements**: HAPT-01, HAPT-02, HAPT-03
**Success Criteria** (what must be TRUE):

  1. On the StickS3 (`#if defined(BOARD_STICKS3)`), the pattern engine (`vibratePatternAmp*`) drives `M5.Speaker` tones, and approve / deny / attention / celebrate / connect each play an audibly distinct, recognisable chime on the speaker
  2. On the StickC Plus the existing LEDC vibration-motor path is unchanged and the motor still buzzes on events
  3. The "vibrate" settings entry is relabelled to "chime"/"haptics", and the UI-beep-vs-event-tone overlap is resolved so cues don't clash
  4. Both `pio run -e m5stack-sticks3` and `pio run -e m5stickc-plus` build green with the board-conditional feedback code in place

**Plans**: TBD

Plans:

- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Build System | 1/1 | Complete    | 2026-06-28 |
| 2. Compatibility Shim | 1/1 | Complete    | 2026-06-28 |
| 3. API Port | 1/5 | In Progress|  |
| 4. Haptics → Chimes | 0/TBD | Not started | - |
