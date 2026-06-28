# Phase 2: Compatibility Shim - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-28
**Phase:** 2-compatibility-shim
**Areas discussed:** RTC/clock, Power/AXP, Shim scope, LED

---

## RTC / Clock (StickS3 has no RTC chip)

| Option | Description | Selected |
|--------|-------------|----------|
| Sync from BLE host | Software RTC seeded from connected host time over BLE; free-runs from millis() between syncs. | ✓ |
| Free-running only | PR #48 style: starts at fixed epoch each boot, drifts, resets on power-cycle. | |
| Hide clock on StickS3 | RTC stubs return fixed/zero; clock UI hidden on StickS3. | |

**User's choice:** Sync from BLE host
**Notes:** Codebase scout found the BLE transport is Nordic UART (serial passthrough) with no explicit time field — host-time availability is unknown. Captured as decision D-01 with a **mandatory free-running fallback (D-02)** and research flag RF-01 so the shim never hard-depends on host time or requires host-side protocol changes.

---

## Power / AXP192 (StickS3 has no AXP192)

| Option | Description | Selected |
|--------|-------------|----------|
| Map to M5.Power, stub rest | Real battery/charging/button/brightness/power-off via M5.Power; stub AXP-only calls (SetLDO, coulomb counter, GetTempInAXP). | ✓ |
| Stub all to safe zeros | Every M5.Axp.* a no-op/zero; UI shows placeholders; real values later. | |
| Hide power UI on StickS3 | Stub calls AND suppress battery/charging widgets. | |

**User's choice:** Map to M5.Power, stub rest
**Notes:** ~15 distinct M5.Axp.* calls in src/main.cpp. Coulomb-counter battery-% falls back to a voltage-based estimate. All board-conditional — StickC Plus keeps real AXP192. (D-03/D-04.)

---

## Shim scope / build strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Verbatim #48 + extend on error | Start from PR #48's compat.h, add only names this fork's compile errors demand. Iterative. | ✓ |
| Full upfront audit | Enumerate all 94 legacy usages across 28 files, build complete shim in one pass. | |

**User's choice:** Verbatim #48 + extend on error
**Notes:** Matches the handover plan. Replicate header content/technique; do not cherry-pick #48 commits. (D-07.)

---

## LED (StickC Plus red LED on GPIO10)

| Option | Description | Selected |
|--------|-------------|----------|
| You decide | Map to StickS3 LED if present, else safe no-op. | |
| No-op on StickS3 | compatLed is an empty stub on StickS3. | |
| Map to a StickS3 LED | Drive a StickS3 LED equivalent so status feedback is preserved on both boards. | ✓ |

**User's choice:** Map to a StickS3 LED
**Notes:** PROJECT.md documents no StickS3 user LED. Captured as D-06 (map if hardware exposes one) with research flag RF-02 to confirm StickS3 LED existence/pin; no-op fallback if none, so the compile never depends on a StickS3 LED.

## Claude's Discretion

- Graphics typedefs (`TFT_eSprite`→`M5Canvas`, `TFT_eSPI`→`lgfx::LGFXBase`) — mechanical, pre-decided.
- Board-conditional structure, `compatOnUsb` wiring, chip-temp source (D-05), battery voltage→% curve.

## Deferred Ideas

- Host-side BLE time broadcast (if RF-01 finds no host time) — future, host-side.
- Higher-fidelity StickS3 chip temperature — polish.
- main.cpp / buddy / character API porting → Phase 3. Haptics → chimes → Phase 4.
