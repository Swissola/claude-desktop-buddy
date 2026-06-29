# Requirements: Claude Desktop Buddy — StickS3 + Chimes Migration

**Defined:** 2026-06-28
**Core Value:** The buddy runs on the M5StickS3 with chime feedback, without breaking the M5StickC Plus build.

## v1 Requirements

Requirements for this migration. Each maps to a roadmap phase.

### Build System

- [x] **BUILD-01**: `platformio.ini` defines a shared `[common]` section plus two envs,
  `m5stickc-plus` and `m5stack-sticks3`, both building from the same `src/` tree
- [x] **BUILD-02**: The board-specific `M5StickCPlus` library is replaced by
  `m5stack/M5Unified` (^0.2.x) + `M5GFX`, keeping `AnimatedGIF` and `ArduinoJson`
- [x] **BUILD-03**: `partitions_8mb.csv` (8MB flash, QIO-OPI, USB-CDC-on-boot) is added and
  referenced by the `m5stack-sticks3` env

### Compatibility Shim

- [x] **SHIM-01**: `src/compat.h` re-creates legacy names on top of M5Unified/M5GFX
  (`TFT_eSprite`→`M5Canvas`, `TFT_eSPI`→`lgfx::LGFXBase`, software RTC,
  `compatOnUsb`/`compatLed`/`compatChipTempC`) so UI/render code is untouched

### API Port

- [x] **PORT-01**: `src/main.cpp` includes `compat.h` instead of `M5StickCPlus.h` and uses the
  unified APIs (`M5.Power`, `compatRtc*`, `compatLed*`, `compatChipTempC`) in place of
  `M5.Axp` / `M5.Rtc` / direct LED / AXP-temp calls
- [ ] **PORT-02**: `src/buddies/*.cpp` (incl. doge + llama), `src/character.*`, `src/buddy.*`
  are updated for the include/type changes so both envs compile cleanly
- [x] **PORT-03**: Both `pio run -e m5stickc-plus` and `pio run -e m5stack-sticks3` build with
  zero errors from the identical source

### Haptics → Chimes

- [ ] **HAPT-01**: On the StickS3 (`#if defined(BOARD_STICKS3)`), the haptic pattern engine
  (`vibratePatternAmp*`) drives `M5.Speaker` tones; each event
  (approve/deny/attention/celebrate/connect) has a distinct, recognisable chime
- [ ] **HAPT-02**: On the StickC Plus, the existing LEDC vibration-motor path is preserved unchanged
- [ ] **HAPT-03**: The "vibrate" settings entry is relabelled to "chime"/"haptics", and the
  sound-vs-chime overlap (UI beep vs event tone) is resolved so cues don't clash

## v2 Requirements

Deferred to future work. Tracked but not in this roadmap.

### Branch Reconciliation

- **RECON-01**: Rebase `multi-host-bonding` onto the migrated `main`
- **RECON-02**: Rebase any unmerged `fix/*` branches onto the migrated `main`

### Optional Physical Haptics

- **BUZZ-01**: Support an M5Stack Grove Vibrator Unit on Port.A (G9) as an optional plug-in
  (no soldering), reusing the existing LEDC pattern engine with `VIBRATE_PIN=9`

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Emotion system (`feat/emotion-system`) | User dislikes the visuals; stays unmerged |
| Rewiring/adapting the physical Vibration HAT to the StickS3 | User has ruled out hardware modification |
| Cherry-picking PR #48's commits | Fork `main` diverged; replicate the technique instead |
| Grove Vibrator Unit purchase (this milestone) | Optional future enhancement; tracked as BUZZ-01 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUILD-01 | Phase 1 | Complete |
| BUILD-02 | Phase 1 | Complete |
| BUILD-03 | Phase 1 | Complete |
| SHIM-01 | Phase 2 | Complete |
| PORT-01 | Phase 3 | Complete |
| PORT-02 | Phase 3 | Pending |
| PORT-03 | Phase 3 | Complete |
| HAPT-01 | Phase 4 | Pending |
| HAPT-02 | Phase 4 | Pending |
| HAPT-03 | Phase 4 | Pending |
| RECON-01 | v2 (deferred) | Deferred |
| RECON-02 | v2 (deferred) | Deferred |
| BUZZ-01 | v2 (deferred) | Deferred |

**Coverage:**
- v1 requirements: 10 total
- Mapped to phases: 10
- Unmapped: 0 ✓
- v2 (deferred, not in this milestone): RECON-01, RECON-02, BUZZ-01

---
*Requirements defined: 2026-06-28*
*Last updated: 2026-06-28 after initialization*
