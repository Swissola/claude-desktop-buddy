# Phase 2: Compatibility Shim - Context

**Gathered:** 2026-06-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver `src/compat.h` — a header that re-creates the legacy M5StickCPlus API names on
top of M5Unified + M5GFX so the existing ~3k lines of UI / render / power / RTC code
**compile unchanged** for the StickS3 (ESP32-S3) build, while the M5StickC Plus build keeps
its native AXP192/RTC paths via board-conditional code (`#if defined(BOARD_STICKS3)`).

**In scope:** the shim header itself — graphics typedefs, software RTC, power/AXP shims,
LED shim, and the `compatOnUsb` / `compatLed` / `compatChipTempC` helpers. Goal is a
**clean compile of the StickS3 env**, replacing the Phase-1 stopping point
(`M5StickCPlus.h: No such file` + old-API errors).

**Out of scope (deferred to other phases):**
- Editing `src/main.cpp` and the buddy/character sources to *include* compat.h and adopt the
  unified API — that is **Phase 3 (API Port)**. (Phase 2 may add the `#include "compat.h"`
  swap only insofar as it is needed to prove the header compiles; substantive API porting is
  Phase 3.)
- Haptics → chimes (`M5.Speaker`) — **Phase 4**.
- Any host-side (Claude Desktop/Code companion) protocol changes.

</domain>

<decisions>
## Implementation Decisions

### RTC / Clock (StickS3 has no RTC chip)
- **D-01:** The software RTC in compat.h **syncs from the BLE host time** as the primary
  source: when a time value is available from the connected Claude Desktop/Code host, seed
  the software clock from it; free-run from `millis()` between syncs.
- **D-02 (fallback, mandatory):** If the host BLE protocol does **not** carry a usable time
  value (see RF-01), fall back to a **free-running software RTC** seeded at first connect /
  boot (PR #48 style). The shim must compile and behave sanely with NO host time available —
  host-time sync is an enhancement layered on top of a working free-running clock, never a
  hard dependency. Do **not** introduce host-side protocol changes to satisfy D-01.
- compat.h must provide `RTC_TimeTypeDef` / `RTC_DateTypeDef` and
  `M5.Rtc.GetTime/SetTime/GetDate/SetDate` equivalents (4 call sites + 2 type uses today).

### Power / AXP192 (StickS3 has no AXP192)
- **D-03:** Map the AXP calls that have a real M5Unified equivalent onto **`M5.Power`** so the
  battery/charging UI shows real data on StickS3:
  - battery voltage / level / current → `M5.Power` battery APIs
  - VBus / USB-power / charging state → `M5.Power` (drives `compatOnUsb`)
  - power button press (`GetBtnPress`) → M5Unified power-button API
  - screen brightness (`ScreenBreath`) → `M5.Display.setBrightness`
  - power off (`PowerOff`) → `M5.Power.powerOff`
- **D-04:** **Stub the AXP-only calls** that have no clean StickS3 analog to safe no-ops /
  zero returns so the code compiles and does not misbehave: `SetLDO`,
  `EnableCoulombcounter` / `GetCoulombData`, `GetTempInAXP` (chip temp), `SetSleep`,
  `WakeUpDisplayAfterLightSleep`. (Battery-% derived from coulomb counter falls back to a
  voltage-based estimate where the UI needs a number.)
- All power shims are board-conditional — the StickC Plus keeps calling the real AXP192.

### Chip temperature (`compatChipTempC`)
- **D-05 (Claude's discretion):** StickC Plus reads AXP192 temp (`GetTempInAXP`); on StickS3
  use the ESP32-S3 internal temperature sensor if trivially available, otherwise return a
  sentinel/0. Low priority — does not block the compile bar.

### LED (`compatLed`, StickC Plus red LED on GPIO10)
- **D-06:** Preserve LED status feedback across both boards — **map `compatLed` to a StickS3
  LED** if the hardware exposes a controllable user LED (see RF-02). If StickS3 has no usable
  LED, fall back to a no-op stub (compile must not depend on a StickS3 LED existing).

### Shim build strategy
- **D-07:** Build compat.h by **starting from PR #48's `compat.h` as the verbatim base, then
  extending it only as this fork's compile errors actually demand** (the fork adds
  doge + llama buddies, `buddy_common.h`/`stats.h`/`xfer.h`, and inline power/sleep code not
  present upstream). Iterate compile → add missing name → recompile. Do NOT cherry-pick PR #48
  commits; replicate the header content/technique.

### Claude's Discretion
- Graphics typedefs are mechanical and pre-decided: `TFT_eSprite` (24 uses) → `M5Canvas`,
  `TFT_eSPI` (13 uses) → `lgfx::LGFXBase`. No user input needed.
- Exact board-conditional structure (`#if defined(BOARD_STICKS3)` blocks vs. paired headers),
  and how `compatOnUsb` is wired, are Claude's call.
- Chip-temp source (D-05) and any voltage→percentage curve for the battery fallback.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Reference implementation (the recipe)
- Upstream PR #48 — `anthropics/claude-desktop-buddy` @ branch `feat/m5sticks3-port`,
  file `src/compat.h` — the board-agnostic shim this phase replicates (verbatim base per D-07).
  Reference-only; do NOT cherry-pick its commits. (External GitHub ref — researcher should
  fetch the actual `compat.h` from that branch to use as the starting point.)

### Project context
- `.planning/PROJECT.md` — StickS3 I/O (ESP32-S3-PICO-1-N8R8, no RTC chip, ES8311 speaker,
  native USB-CDC), haptics→chimes rationale, fork-divergence notes, key decisions.
- `.planning/REQUIREMENTS.md` §"Compatibility Shim" — **SHIM-01** (the sole requirement here).
- `.planning/ROADMAP.md` §"Phase 2" — goal + success criteria.

### Code the shim must satisfy (in this repo)
- `src/main.cpp` — heaviest legacy-API consumer (44 hits): `M5.Axp.*` (~15 distinct calls),
  `M5.Rtc.*`, `LED_PIN`/`digitalWrite`, inline light-sleep/power code.
- `src/ble_bridge.cpp` / `src/ble_bridge.h` — Nordic UART Service (NUS) bridge; relevant to
  RF-01 (whether host time is available in the inbound stream).
- `src/character.*`, `src/buddy.*`, `src/stats.h`, `src/xfer.h`, `src/data.h`,
  `src/buddies/*.cpp` — `TFT_eSprite` / `TFT_eSPI` render code that must compile through the
  graphics typedefs.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- PR #48 `compat.h` is the board-agnostic starting point — most graphics + RTC + helper
  scaffolding can be lifted from it.
- `M5.Power` (M5Unified) already abstracts battery/charging/power-button across M5 boards —
  the natural target for D-03.

### Established Patterns
- Board-conditional compilation is the project-wide pattern (`#if defined(BOARD_STICKS3)`,
  set via `-DBOARD_STICKS3` build flag added in Phase 1).
- BLE transport is **Nordic UART (NUS)** serial passthrough — UUIDs `6e40000{1,2,3}-…`;
  inbound host data arrives via `RxCallbacks::onWrite` in `src/ble_bridge.cpp`. Any host-time
  sync (D-01) reads from this existing stream — no new characteristic.

### Integration Points
- compat.h is included by `src/main.cpp` (replacing `#include <M5StickCPlus.h>`) and
  transitively by render/buddy code — wiring that include is mostly Phase 3, but the header
  must compile standalone here.
- Phase-1 build flags: StickS3 env already defines `BOARD_STICKS3`, `memory_type=qio_opi`,
  native USB-CDC. (Note: `-DBOARD_HAS_PSRAM` added to the StickS3 env as a Phase-1 follow-up.)

</code_context>

<specifics>
## Specific Ideas / Research Flags

- **RF-01 (research, blocks D-01 only — NOT the phase):** Determine whether the Claude
  Desktop/Code host sends any usable time/timestamp over the NUS inbound stream
  (`RxCallbacks::onWrite` in `src/ble_bridge.cpp`). If yes → wire D-01 host sync. If no → ship
  D-02 free-running fallback and note host-time sync as future work. Must NOT require host-side
  changes in this phase.
- **RF-02 (research, resolves D-06):** Confirm whether the M5StickS3 exposes a
  controllable user LED (onboard LED / RGB / GPIO) and its pin. If yes → map `compatLed` to it.
  If no → `compatLed` is a no-op stub on StickS3. PROJECT.md documents no StickS3 LED, so treat
  "no LED" as the likely outcome and the no-op as the safe default.
- Graphics mapping is settled (`TFT_eSprite`→`M5Canvas`, `TFT_eSPI`→`lgfx::LGFXBase`).

</specifics>

<deferred>
## Deferred Ideas

- **Host-side time broadcast:** if RF-01 finds no host time today, adding a timestamp to the
  companion's BLE protocol so D-01 can be fully realized — future work, host-side, out of scope.
- **StickS3 chip-temperature fidelity:** a proper ESP32-S3 internal-temp reading for
  `compatChipTempC` beyond the D-05 minimum — polish, not required for the compile bar.
- API porting of `main.cpp` + buddy/character sources to actually adopt the unified API →
  **Phase 3**. Haptics → chimes → **Phase 4**.

None of these belong in Phase 2.

</deferred>

---

*Phase: 2-compatibility-shim*
*Context gathered: 2026-06-28*
