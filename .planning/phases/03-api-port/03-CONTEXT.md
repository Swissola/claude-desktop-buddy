# Phase 3: API Port - Context

**Gathered:** 2026-06-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire the Phase-2 `src/compat.h` shim into the application sources so **both** PlatformIO
environments (`m5stickc-plus` and `m5stack-sticks3`) compile cleanly from the identical
`src/` tree. This means replacing the direct M5StickCPlus API calls in `src/main.cpp` and the
buddy/character/render code with the unified API + compat helpers, and resolving the
`class TFT_eSPI;` forward-declaration conflicts.

**In scope:**
- `src/main.cpp`: include `compat.h` instead of `<M5StickCPlus.h>`; replace `M5.Axp.*`,
  `M5.Rtc.*`, direct `LED_PIN`/`digitalWrite`, and `M5.Beep.*` with compat helpers /
  unified API (PORT-01).
- `src/buddies/*.cpp` (incl. doge + llama), `src/character.*`, `src/buddy.*`, and any header
  using legacy types: include/type changes so both envs compile, including removing the
  `class TFT_eSPI;` forward decls at `buddy.h:11` / `character.h:27` (PORT-02).
- Amending `src/compat.h` where required to make **both** boards build/behave correctly (see
  RF-04 — the current `#else` StickC-Plus branch references `M5.Axp.*`, which no longer exists
  now that Phase 1 moved StickC Plus to M5Unified). Adding the `M5.Beep`→`M5.Speaker` shim
  (D-10) lives here too.
- A single unified `M5.begin()` init for both boards (D-09).
- Acceptance: **both** `pio run -e m5stickc-plus` AND `pio run -e m5stack-sticks3` build with
  zero errors from one source (PORT-03), plus hardware smoke verification (D-08).

**Out of scope (other phases / milestones):**
- Haptics → audio chimes (event tones via `M5.Speaker`) — **Phase 4**. Phase 3 only enables the
  speaker at init (D-09) and shims the existing UI beep (D-10); it does NOT add event chimes.
- v2 items: branch reconciliation (RECON-01/02), Grove Vibrator Unit (BUZZ-01).

</domain>

<decisions>
## Implementation Decisions

### StickC Plus runtime parity (D-08)
- **D-08:** **Preserve the StickC Plus runtime behavior** through the port — especially the
  recently-optimized idle-sleep power path (`M5.Axp.SetSleep` / `WakeUpDisplayAfterLightSleep`
  rail-cutting, BLE-advertising-off during idle sleep) — and treat a **hardware smoke-test on
  the StickC Plus** as part of "done" for this phase. Port the AXP power/sleep/RTC/LED/beep
  call sites to M5Unified equivalents that keep current behavior, not just call sites that
  merely compile. Expect this phase's verification to surface `human_needed` items for
  on-device confirmation (battery %, clock, LED, idle power draw, UI beep). StickS3 hardware
  testing is desirable too **if a StickS3 device is available** — otherwise StickS3 is verified
  by clean compile/link.

### M5.begin() initialization (D-09)
- **D-09:** Use a single unified `M5.begin()` for both boards and **enable the speaker now**
  (alongside display + IMU) so Phase 4 chimes only need `tone()` calls. The speaker stays idle
  until used. Note the known StickC-Plus LEDC-channel interaction (motor LEDC ch2 vs
  speaker/M5.Beep ch0, documented at `main.cpp:31-35`) is a **Phase 4** concern, not a blocker
  for merely enabling the speaker here.

### UI beep (D-10)
- **D-10:** **Shim `M5.Beep`→`M5.Speaker` in `compat.h`** (e.g. a `compatBeep`/`M5.Beep`
  facade) so the existing `M5.Beep.tone(...)` UI-beep call sites (`settings().sound` path) stay
  nearly unchanged and keep working on both boards through Phase 3. Requires the speaker enabled
  (D-09). This is the UI beep only — event chimes remain Phase 4.

### Claude's Discretion
- Exact mechanics of resolving the `class TFT_eSPI;` forward-decl conflicts (remove decl +
  include compat.h vs. forward-declare the M5GFX type) — Claude's call.
- Whether `M5.Lcd` call sites stay as-is (M5Unified aliases `M5.Lcd`→`M5.Display`) or are
  migrated — verify and choose the least-churn option (see RF-05).
- How the compat helpers replace each specific `M5.Axp.*`/`M5.Rtc.*` site in main.cpp.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Prior-phase artifacts (the shim being wired in)
- `.planning/phases/02-compatibility-shim/02-RESEARCH.md` — verbatim PR #48 base, the
  legacy-name→`M5.Power`/`M5.Display` mapping table with exact signatures, and pitfalls
  (esp. Pitfall 1: the `buddy.h:11` / `character.h:27` forward-decl conflicts — to be fixed
  HERE in Phase 3).
- `.planning/phases/02-compatibility-shim/02-CONTEXT.md` — D-01..D-07 (shim decisions carried
  forward).
- `src/compat.h` — the shim being adopted; will be amended for RF-04 + D-10.

### Project context
- `.planning/PROJECT.md` — StickS3 I/O, fork divergence (doge/llama buddies, buddy_common.h,
  stats.h, recent power/sleep `fix/*` work), key decisions.
- `.planning/REQUIREMENTS.md` §"API Port" — PORT-01, PORT-02, PORT-03.
- `.planning/ROADMAP.md` §"Phase 3" — goal + success criteria.

### Code to port (this repo)
- `src/main.cpp` — primary port target (44 legacy hits): `M5.Axp.*`
  (ScreenBreath, SetLDO2, SetSleep, WakeUpDisplayAfterLightSleep, PowerOff, GetBtnPress,
  GetVBusVoltage), `M5.Rtc.*`, `LED_PIN`/`digitalWrite`, `M5.Beep.tone`, `esp_light_sleep`,
  `M5.Imu`, `M5.Lcd`.
- `src/buddy.h` (forward decl @11), `src/character.h` (forward decl @27), `src/buddy.cpp`,
  `src/character.cpp`, `src/buddies/*.cpp`, `src/stats.h`, `src/xfer.h`, `src/data.h`.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/compat.h` already provides the StickS3 helper set (graphics typedefs, software RTC,
  `compatScreenBreath`/`compatBacklight`/`compatPowerOff`/`compatPowerBtnShort`/
  `compatOnUsb`/`compatLed*`/`compatChipTempC`/`compatRailSleep`/`compatRailWake`). Phase 3
  maps each main.cpp call site onto these.
- M5Unified `M5.Power` works on the StickC Plus too (it wraps the AXP192), and `M5.Lcd` is an
  alias of `M5.Display` — both reduce churn.

### Established Patterns
- Board-conditional compilation via `#if defined(BOARD_STICKS3)` (build flag from Phase 1).
- Recent power architecture: cheap timer-wake idle sleep + BLE advertising stopped during idle
  (commits `f9a53cd`, `c9a5f19`) — D-08 says preserve this behavior on StickC Plus.

### Integration Points
- `src/main.cpp` is the hub; `compat.h` is included there and transitively by render code.
- The UI-beep path (`settings().sound` → `M5.Beep.tone`) and the event-feedback path are
  distinct; Phase 3 handles the UI beep (D-10), Phase 4 handles event chimes.

</code_context>

<specifics>
## Specific Ideas / Research Flags

- **RF-04 (critical — blocks PORT-03 for StickC Plus):** `compat.h`'s current `#else`
  (non-StickS3 = StickC Plus) branch calls `M5.Axp.*`, but Phase 1 removed the M5StickCPlus
  library and put BOTH boards on M5Unified — where `M5.Axp` does not exist. The `m5stickc-plus`
  env was never fully compiled yet, so this is latent. Research/plan must reconcile: likely
  route the StickC-Plus path through `M5.Power` as well (M5Unified abstracts AXP192), keeping
  board-conditionals only where the StickS3 genuinely differs. Confirm what `M5.Power` exposes
  on the StickC Plus (battery V/I, VBus, key state, brightness, powerOff) under M5Unified 0.2.x.
- **RF-03 (preserve idle-sleep power, D-08):** Find the M5Unified-era way to replicate the
  StickC-Plus AXP idle-sleep rail management (`SetSleep` cutting LDO2/LDO3 while keeping
  DCDC1+LDO1; `WakeUpDisplayAfterLightSleep`) so idle power draw is not regressed. Options to
  research: `M5.Power` sleep helpers, direct AXP192 register access via M5Unified's I2C, or an
  acceptable equivalent. Quantify any expected power-draw change.
- **RF-05 (low):** Confirm `M5.Lcd` remains a valid alias for `M5.Display` under M5Unified so
  the many `M5.Lcd.*` call sites can stay unedited (minimize churn).
- **D-10 mechanics:** the `M5.Beep`→`M5.Speaker` shim must live in compat.h and be board-safe;
  verify `M5.Speaker.tone(freq, dur)` signature parity with the old `M5.Beep.tone`.

</specifics>

<deferred>
## Deferred Ideas

- Event chimes (`M5.Speaker` tone sequences per approve/deny/etc.) → **Phase 4**.
- StickC-Plus speaker/motor LEDC-channel collision resolution → **Phase 4** (haptics).
- v2: RECON-01/02 (rebase multi-host-bonding + fix/* onto migrated main), BUZZ-01 (Grove
  Vibrator Unit on Port.A G9) — **kept fully deferred** to a separate post-merge milestone
  (user decision, 2026-06-28). Do NOT fold a BUZZ-01 configurability seam into Phase 4; Phase 4
  stays scoped to StickC-Plus motor + StickS3 chimes only.

None of the above belongs in Phase 3.

</deferred>

---

*Phase: 3-api-port*
*Context gathered: 2026-06-28*
