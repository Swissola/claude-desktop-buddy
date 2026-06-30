# Phase 3: API Port - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-28
**Phase:** 3-api-port
**Areas discussed:** StickC Plus parity, M5.begin config, UI beep

---

## StickC Plus runtime parity

| Option | Description | Selected |
|--------|-------------|----------|
| Preserve behavior, verify on HW | Port to M5Unified preserving runtime behavior (esp. idle-sleep power saving); hardware smoke-test part of done. | ✓ |
| Compile-clean bar, accept drift | 'Both envs compile clean' is the bar; accept behavior shifts, capture regressions later. | |
| Preserve, no HW test now | Port carefully but don't gate on hardware testing. | |

**User's choice:** Preserve behavior, verify on HW
**Notes:** Phase 1 already moved StickC Plus to M5Unified (no more M5.Axp/M5.Beep on either board), so the port changes the working device's power/sleep behavior. User recently optimized idle sleep (commits f9a53cd, c9a5f19). Captured as D-08; spawned RF-03 (preserve idle-sleep rail management) and RF-04 (compat.h #else branch references nonexistent M5.Axp). Expect human_needed verification items.

---

## M5.begin() initialization

| Option | Description | Selected |
|--------|-------------|----------|
| Enable speaker now | M5.begin enables speaker + display + IMU on both boards now; speaker idle until Phase 4. | ✓ |
| Minimal init, speaker in Phase 4 | Enable only what Phase 3 needs; defer speaker to Phase 4. | |
| You decide | Mirror existing behavior; enable speaker only if free to leave idle. | |

**User's choice:** Enable speaker now
**Notes:** Captured as D-09. Known StickC-Plus LEDC channel interaction (motor ch2 vs speaker ch0, main.cpp:31-35) flagged as a Phase 4 concern, not a Phase 3 blocker.

---

## UI beep (M5.Beep → M5.Speaker)

| Option | Description | Selected |
|--------|-------------|----------|
| Shim M5.Beep in compat.h | compat M5.Beep→M5.Speaker facade so UI-beep call sites stay nearly unchanged; works both boards. | ✓ |
| Port call sites to M5.Speaker | Replace M5.Beep.tone with M5.Speaker.tone directly in main.cpp. | |
| Defer all audio to Phase 4 | Stub UI beep to no-op for Phase 3; wire all audio in Phase 4. | |

**User's choice:** Shim M5.Beep in compat.h
**Notes:** Captured as D-10. Requires speaker enabled (D-09). UI beep only — event chimes remain Phase 4.

## Claude's Discretion

- Forward-decl conflict resolution mechanics (buddy.h:11, character.h:27).
- Whether M5.Lcd call sites stay (M5Unified aliases M5.Lcd→M5.Display) — RF-05.
- Per-site compat helper substitution in main.cpp.

## Deferred Ideas

- Event chimes + StickC-Plus speaker/motor LEDC collision → Phase 4.
- v2: RECON-01/02 (branch reconciliation), BUZZ-01 (Grove Vibrator Unit on G9) — separate milestone; possible Phase-4 seam to make haptics pin-configurable so BUZZ-01 is a later config flip.
