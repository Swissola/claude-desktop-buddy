# Phase 3: API Port - Research

**Researched:** 2026-06-28
**Domain:** Embedded C++ (Arduino/PlatformIO) — wiring the Phase-2 `src/compat.h` shim into `main.cpp` + buddy/character/render/stats/xfer/data so BOTH PlatformIO envs build zero-error on M5Unified
**Confidence:** HIGH (every M5Unified API claim below was read from the *installed* library headers under `.pio/libdeps/`, not from training data)

## Summary

Phase 1 removed the `M5StickCPlus` library and put **both** boards on `m5stack/M5Unified ^0.2.0` + `M5GFX`. Phase 2 produced `src/compat.h`. The application code, however, still `#include <M5StickCPlus.h>` and still calls `M5.Axp.*` / `M5.Rtc.*` / `M5.Beep.*` / `M5.Imu.Init()` directly — none of which exist under M5Unified. Phase 3 is the mechanical-but-careful swap of every such call onto a compat helper or the unified API, plus three structural fixes that are larger than they look.

**The single most important finding (RF-04, and bigger than CONTEXT framed it):** `M5.Axp` does not exist on *either* board now, so the `#else` (StickC-Plus) branch of the current `compat.h` is **itself broken** — and so are the **direct** `M5.Axp.*` calls in `stats.h` and `xfer.h`, which CONTEXT's PORT-01/02 list as in-scope. The fix is clean: M5Unified's `M5.Power` wraps the AXP192 on the StickC Plus, so almost every helper **collapses to one board-agnostic definition**. The only genuinely board-specific helpers left are rail-cut idle-sleep and the onboard LED.

**Two NEW blockers not in CONTEXT, both verified, both gating PORT-03:**
1. **Arduino core is 3.3.9 (arduino-esp32 3.x).** `ledcSetup()` / `ledcAttachPin()` are **deleted** in core 3.x. `main.cpp`'s motor code (lines 1228-1230, 103, 112) will not compile on *either* env. Must port to the pin-based `ledcAttach()` / `ledcWrite(pin,…)` API. (Haptic *behavior* stays Phase 4; making it *compile* is unavoidably Phase 3.)
2. **`BUTTON_A_PIN` / `BUTTON_B_PIN` are undefined.** They were `M5StickCPlus.h` macros (used at `main.cpp:279-280` for light-sleep GPIO wake); nothing in M5Unified/M5GFX defines them. `compat.h` must provide them.

**Primary recommendation:** First amend `compat.h` (collapse the power helpers onto `M5.Power`, add `compatBeep`, add the button-pin macros, fix the rail-sleep bodies). Then do the call-site swaps file-by-file using the port map below. Build `m5stickc-plus` first (it exercises the AXP192 path), fix, then `m5stack-sticks3`. Expect 2-4 first-time-compile errors beyond the mapped ones.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-08 (StickC Plus runtime parity):** Preserve StickC Plus runtime behavior through the port — especially the optimized idle-sleep power path (`M5.Axp.SetSleep` / `WakeUpDisplayAfterLightSleep` rail-cutting, BLE-advertising-off during idle sleep). Port AXP power/sleep/RTC/LED/beep call sites to M5Unified equivalents that **keep current behavior**, not merely compile. A **hardware smoke-test on the StickC Plus is part of "done."** Expect `human_needed` items (battery %, clock, LED, idle power draw, UI beep). StickS3 hardware testing desirable if a device is available; otherwise StickS3 is verified by clean compile/link.
- **D-09 (M5.begin):** Single unified `M5.begin()` for both boards; **enable the speaker now** (alongside display + IMU) so Phase 4 chimes only need `tone()` calls. Speaker stays idle until used. The StickC-Plus LEDC-channel interaction (motor ch2 vs speaker ch0, `main.cpp:31-35`) is a **Phase 4** concern, not a blocker for merely enabling the speaker.
- **D-10 (UI beep):** Shim `M5.Beep`→`M5.Speaker` in `compat.h` (e.g. `compatBeep`) so the existing `M5.Beep.tone(...)` UI-beep call sites (`settings().sound` path) keep working on both boards. UI beep only — event chimes remain Phase 4. Requires the speaker enabled (D-09).

### Claude's Discretion
- Exact mechanics of resolving the `class TFT_eSPI;` forward-decl conflicts (remove decl + include compat.h vs. forward-declare the M5GFX type) — Claude's call.
- Whether `M5.Lcd` call sites stay as-is or are migrated — verify and choose the least-churn option (RF-05).
- How the compat helpers replace each specific `M5.Axp.*`/`M5.Rtc.*` site in main.cpp.

### Deferred Ideas (OUT OF SCOPE)
- Event chimes (`M5.Speaker` tone sequences per approve/deny/etc.) → **Phase 4**.
- StickC-Plus speaker/motor LEDC-channel collision resolution → **Phase 4**.
- v2: RECON-01/02 (branch reconciliation), BUZZ-01 (Grove Vibrator Unit on Port.A G9) — fully deferred. Do NOT fold a BUZZ-01 seam into this phase.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| PORT-01 | `main.cpp` includes `compat.h` instead of `M5StickCPlus.h`; uses unified API / compat helpers in place of `M5.Axp` / `M5.Rtc` / direct-LED / AXP-temp / `M5.Beep` | Per-call-site port map below covers all 40+ `main.cpp` hits with exact replacements. |
| PORT-02 | `buddies/*.cpp` (incl. doge + llama), `character.*`, `buddy.*` (and `stats.h`/`xfer.h`/`data.h` which also use legacy names) updated for include/type changes so both envs compile | Forward-decl fix + the 24 `#include` swaps + stats/xfer/data port rows below. |
| PORT-03 | Both `pio run -e m5stickc-plus` AND `pio run -e m5stack-sticks3` build zero-error from one source | RF-04 power collapse + the two NEW blockers (LEDC core-3.x, button-pin macros) + build/verify section below. |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Battery / VBus / charge / power-button | `compat.h` → `M5.Power` | — | `M5.Power` wraps AXP192 (StickC+) and the S3 gauge; **board-agnostic** now (RF-04) |
| Backlight / brightness | `compat.h` → `M5.Display.setBrightness` | — | Display tier owns backlight on both boards |
| Idle-sleep rail cut (LDO2/LDO3) | `compat.h` `#else` → `M5.Power.Axp192.setLDOx` | ESP32 `esp_light_sleep_start` (generic, stays) | Genuinely board-specific; `M5.Power.Axp192` exists **only on classic-ESP32 builds** |
| IMU read / sleep | `M5.Imu` (unified) | — | `getAccelData`, `sleep()`, `begin()` all exist on both (MPU6886 / BMI270) |
| UI beep | `compat.h` `compatBeep` → `M5.Speaker.tone` | — | `M5.Speaker` replaces `M5.Beep`; same 2-arg `tone` shape; both boards |
| Wall clock (RTC) | `compat.h` `compatRtc*` (system clock) | BLE host `{"time":…}` seed | No RTC chip on S3; do NOT route through `M5.Rtc` (Phase-2 decision) |
| Onboard LED | `compat.h` `compatLed*` (GPIO10 on StickC+; no-op S3) | — | Board-specific GPIO |
| Motor PWM (LEDC) | `main.cpp` (core-3.x `ledcAttach`) | — | Generic ESP32; **API changed in core 3.x** — must port to compile (behavior = Phase 4) |
| Sprite / canvas drawing | `M5Canvas` / `lgfx::LGFXBase` via `TFT_eSprite`/`TFT_eSPI` aliases | — | 1:1 method parity; only the forward-decl conflict needs fixing |

## Standard Stack

No new packages. Everything is already in `platformio.ini [common]`:

| Library | Installed Version | Purpose |
|---------|-------------------|---------|
| `m5stack/M5Unified` | `^0.2.0` (read from `.pio/libdeps`) | `M5.Power`, `M5.Display`/`M5.Lcd`, `M5.Imu`, `M5.Speaker`, `M5.Btn*` |
| `m5stack/M5GFX` | `^0.2.0` | `M5Canvas`, `lgfx::LGFXBase` |
| `bitbank2/AnimatedGIF` | `^2.1.1` | unchanged (character.cpp) |
| `bblanchon/ArduinoJson` | `^7.0.0` | unchanged (data.h/xfer.h) |

**Toolchain (verified, load-bearing):**
- `espressif32` platform **55.03.39** → **arduino-esp32 core 3.3.9** `[VERIFIED: ~/.platformio/platforms/espressif32/platform.json + framework-arduinoespressif32 package]`. This is the source of the LEDC-API blocker.

## Package Legitimacy Audit

Not applicable — Phase 3 installs **no** external packages. All dependencies were vetted and locked in Phase 1. No registry queries needed.

---

## RF-04 — The corrected `compat.h` power section (CRITICAL, blocks the build)

`M5.Axp` exists nowhere under M5Unified. The current `compat.h` `#else` branch (lines 136-148) references it and will fail to compile the `m5stickc-plus` env. Verified facts from the installed headers:

**`M5.Power` (board-agnostic — works on AXP192 *and* the S3 gauge)** `[VERIFIED: Power_Class.hpp]`
| Method | Returns | Notes |
|--------|---------|-------|
| `getBatteryVoltage()` | `int16_t` **mV** | divide by 1000 for legacy float-volts |
| `getBatteryCurrent()` | `int32_t` **mA** | +charge / −discharge |
| `getVBUSVoltage()` | `int16_t` **mV** | −1 if unsupported |
| `getBatteryLevel()` | `int32_t` **0..100** | the battery-% source for both boards |
| `getKeyState()` | `uint8_t` | `0=none 1=long 2=short 3=both` |
| `isCharging()` | enum | `is_discharging/is_charging/charge_unknown` |
| `powerOff()` | `void` | |

**`M5.Power.Axp192` — exposed ONLY on classic-ESP32 builds** `[VERIFIED: Power_Class.hpp:208-231 — the AXP192_Class member is in the `#else` (non-S3/C3/C6/P4) branch; the `CONFIG_IDF_TARGET_ESP32S3` branch has Axp2101/PY32pmic/Ina226 but NO Axp192]`. Therefore `M5.Power.Axp192.*` **must stay inside `compat.h`'s `#if !defined(BOARD_STICKS3)` (`#else`) branch** or the StickS3 build breaks. Available: `setLDO2(mV)`, `setLDO3(mV)`, `setLDO0`, `setDCDC1/2/3`, `powerOff()`, `getInternalTemperature()`, `getPekPress()`, `isVBUS()`, `getVBUSVoltage()` (float V), `getBatteryVoltage()` (float V). `[VERIFIED: AXP192_Class.hpp]`. Crucially, `setLDOx(0)` **clears that rail's enable bit in reg 0x12** (`_set_LDO → bitOff(0x12, 1<<num)` `[VERIFIED: AXP192_Class.cpp:59-86]`) — i.e. it is the faithful M5Unified equivalent of the old `SetSleep` rail-cut.

### Replacement `compat.h` power block (replace current lines 104-148)

```cpp
// --- Power / battery / brightness (RF-04) ---
// Phase 1 removed M5StickCPlus; M5.Axp no longer exists on EITHER board.
// M5.Power wraps the AXP192 (StickC Plus) and the S3 gauge, so these helpers
// are board-AGNOSTIC. Only rail-cut sleep + the onboard LED stay board-specific.
inline uint8_t& _compatBrightness() { static uint8_t b = 255; return b; }

static inline float compatBatVoltage()  { return M5.Power.getBatteryVoltage() / 1000.0f; } // mV->V
static inline float compatBatCurrent()  { return (float)M5.Power.getBatteryCurrent(); }     // mA
static inline float compatVBusVoltage() { return M5.Power.getVBUSVoltage() / 1000.0f; }     // mV->V (-1mV->neg)
static inline int   compatBatteryPct()  { return M5.Power.getBatteryLevel(); }              // 0..100
static inline void  compatScreenBreath(int v) {
  uint8_t b = (uint8_t)map(constrain(v,0,100),0,100,0,255);
  _compatBrightness() = b;
  M5.Display.setBrightness(b);
}
static inline void  compatBacklight(bool on) { M5.Display.setBrightness(on ? _compatBrightness() : 0); }
static inline bool  compatPowerBtnShort() { return M5.Power.getKeyState() == 2; }
static inline void  compatPowerOff()      { M5.Power.powerOff(); }
static inline void  compatEnableCoulomb() {}   // M5Unified exposes no coulomb counter (D-04)

// --- Idle-sleep rail cut (RF-03 / D-08) — genuinely board-specific ---
#if defined(BOARD_STICKS3)
static inline void compatRailSleep() {}        // no AXP; backlight handled by compatBacklight
static inline void compatRailWake()  {}
#else
// Replicate the old M5.Axp.SetSleep(): drop LDO2 (backlight) + LDO3 (panel
// logic), keep DCDC1=ESP32 + LDO1=RTC. setLDOx(0) clears the reg-0x12 enable
// bit (verified). M5.Power.Axp192 only compiles on classic-ESP32 (this branch).
static inline void compatRailSleep() {
  M5.Power.Axp192.setLDO3(0);   // panel logic off
  M5.Power.Axp192.setLDO2(0);   // backlight rail off
}
static inline void compatRailWake() {
  M5.Power.Axp192.setLDO3(3000);  // restore panel logic ~3.0V
  M5.Power.Axp192.setLDO2(3000);  // restore backlight rail; caller re-applies brightness
}
#endif
```

> **Net effect:** the `#if defined(BOARD_STICKS3) / #else` split in the power section shrinks from ~12 paired helpers to just `compatRailSleep/Wake` (and the existing LED block). Everything else is one definition. `compatBatteryPct()` is now identical on both boards (`getBatteryLevel()`), which also resolves the `stats.h` coulomb problem (below).

**[ASSUMED] restore voltage 3000mV** for `compatRailWake`: the M5StickC Plus runs LDO2/LDO3 at ~3.0V. I could not find the exact value M5Unified re-applies at init (the StickCPlus case in `M5Unified.cpp` does not re-set LDO2/LDO3 voltages explicitly in the section read). 3000mV is the documented panel voltage and is safe; **confirm on hardware** (display must come back cleanly after idle sleep) — this is a D-08 `human_needed` item.

---

## RF-03 — Preserving idle-sleep power (D-08)

The existing path (`main.cpp` idlePowerDown/idlePowerRestore, lines 262-275) does three things; the generic ESP32 light-sleep machinery (`esp_light_sleep_start`, `gpio_wakeup_enable`, timer wake) **stays unchanged**. Only the AXP + IMU bits move:

| Old (M5StickCPlus) | New (M5Unified) | Confidence |
|---|---|---|
| `M5.Axp.SetSleep()` (reg 0x12 &= 0xA1, cuts LDO2/LDO3) | `compatRailSleep()` → `Axp192.setLDO3(0); setLDO2(0)` | HIGH (verified `setLDO(0)`→`bitOff(0x12,…)`) |
| `imuSleep()` raw `Wire1` 0x68 reg-0x6B SLEEP write | `M5.Imu.sleep()` (returns bool) | HIGH `[VERIFIED: IMU_Class.hpp:84]` — works on both MPU6886 + BMI270, cleaner than raw I2C |
| `M5.Axp.WakeUpDisplayAfterLightSleep()` | `compatRailWake()` → `Axp192.setLDO3/2(3000)` | HIGH for mechanism; voltage = `[ASSUMED 3000mV]` |
| `M5.Imu.Init()` (on wake) | `M5.Imu.begin()` | HIGH `[VERIFIED: IMU_Class.hpp:82]` (no separate `wake()`; `begin()` re-inits) |
| `bleStopAdvertising()` / `bleStartAdvertising()` | unchanged | — |

**Expected power impact:** Functionally equivalent. `setLDO2(0)+setLDO3(0)` cuts the same two rails the old `SetSleep` did (verified at the register level), so the backlight + panel-logic draw is removed identically. `M5.Imu.sleep()` puts the MPU6886 into the same low-power mode the old raw write targeted. The big idle win the code comments credit to **advertising-off** is untouched. **Risk:** LOW on the rail mechanism; the only uncertainty is the wake-restore LDO voltage (above) — a wrong value shows as a dark/garbled screen after the first idle cycle, caught by the D-08 idle-power + display hardware check. **Recommendation:** implement exactly as the tables show; flag idle-power-draw + post-sleep-display-restore as `human_needed`.

> Replace `imuSleep()`'s body (`main.cpp:243-248`) with a single `M5.Imu.sleep();` and drop the `Wire1` includes-by-usage. Do NOT keep the raw `Wire1` path — under M5Unified the IMU is driven via `M5.In_I2C` and mixing a manual `Wire1` transaction risks bus contention.

---

## D-10 — `M5.Beep` → `M5.Speaker` shim

`M5.Speaker.tone()` has drop-in 2-arg parity `[VERIFIED: Speaker_Class.hpp:165]`:
```cpp
bool tone(float frequency, uint32_t duration = UINT32_MAX, int channel = -1, bool stop_current_sound = true);
```
Old `M5.Beep.tone(uint16_t freq, uint16_t dur)` maps directly. A **helper** (not a fake `M5.Beep` object — you cannot add a member to the M5Unified `M5` instance) is cleanest and matches the PR #48 technique. Add to `compat.h`:

```cpp
// --- UI beep (D-10): M5.Beep -> M5.Speaker ---
// M5Unified has no M5.Beep. Speaker must be enabled at M5.begin (D-09).
// Works on both boards (StickC Plus buzzer + StickS3 ES8311/AW8737).
static inline void compatBeep(uint16_t freq, uint16_t dur) {
  M5.Speaker.tone((float)freq, (uint32_t)dur);
}
```
- `main.cpp:294` `beep()` body → `if (settings().sound) compatBeep(freq, dur);`
- `M5.Beep.begin()` (`main.cpp:1224`) → **delete** (speaker is begun by `M5.begin(cfg)` with `internal_spk`).
- `M5.Beep.update()` (`main.cpp:1297`) → **delete** (`M5.Speaker` plays asynchronously on its own; no per-loop pump needed). `setVolume(uint8_t)` available if a default is wanted `[VERIFIED: Speaker_Class.hpp:121]`.

---

## D-09 — Unified `M5.begin()` config

`M5.config()` returns a `config_t` whose relevant fields **all default `true`** `[VERIFIED: M5Unified.hpp:85-147]`: `clear_display=true`, `internal_imu=true`, `internal_rtc=true`, `internal_mic=true`, `internal_spk=true`, `output_power=true`. So bare `M5.begin()` already enables IMU + speaker — but be explicit for intent and to disable the unused mic:

```cpp
void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;   // IMU on (replaces M5.Imu.Init())
  cfg.internal_spk = true;   // D-09: enable speaker now, idle until used
  cfg.internal_mic = false;  // unused; don't claim mic resources
  cfg.clear_display = true;
  M5.begin(cfg);
  M5.Display.setRotation(0);   // (M5.Lcd.setRotation(0) also works — alias)
  // M5.Speaker.setVolume(128); // optional default for the UI beep
  startBt();
  compatLedInit();             // replaces pinMode(LED_PIN,..)+digitalWrite(LED_PIN,HIGH)
  // --- motor PWM: core-3.x LEDC API (see blocker below) ---
  ledcAttach(VIBRATE_PIN, 500, 8);  // pin-based; replaces ledcSetup+ledcAttachPin
  ledcWrite(VIBRATE_PIN, 0);
  ...
```
- Delete `M5.Imu.Init()` (`:1223`) and `M5.Beep.begin()` (`:1224`).
- **LEDC-channel note (D-09 / Phase 4):** enabling the speaker is safe in Phase 3. On the StickC Plus the buzzer is `spk_cfg.buzzer=true` `[VERIFIED: M5Unified.cpp:2446]` and uses an internally-managed LEDC channel; under core 3.x LEDC is pin-based so the old "ch0 vs ch2 collision" framing no longer applies the same way. The simultaneous beep+motor collision resolution is **Phase 4**; Phase 3 only needs both to compile and the beep to sound.

---

## RF-05 — `M5.Lcd` alias (CONFIRMED — zero churn)

`M5GFX &Lcd = Display;` `[VERIFIED: M5Unified.hpp:218]`. **All `M5.Lcd.*` call sites stay exactly as-is** (`main.cpp:587,588,589,717,720,728,729,732,733,748,749,759`, plus `TFT_eSprite spr = TFT_eSprite(&M5.Lcd)` at `:10`). `M5.Lcd` is an `M5GFX` which is an `lgfx::LGFXBase`, so it binds to the `TFT_eSPI* = lgfx::LGFXBase*` parameters in `buddyRenderTo` / `characterRenderTo` with no change. Do **not** migrate them — keep the least-churn option per discretion.

Likewise the **text-datum constants `MC_DATUM` / `TL_DATUM`** (`main.cpp:700,710,728,733,1253,1267`) — M5GFX's LGFXBase consumes `textdatum_t` and provides the legacy `*_DATUM` names via its lgfx compatibility enum `[CITED: M5GFX LGFXBase.hpp setTextDatum(uint8_t) overload exists]`. **Confidence MEDIUM** — if the build reports `MC_DATUM`/`TL_DATUM` undeclared, add two `#define`s to `compat.h` (`MC_DATUM=middle_center`, `TL_DATUM=top_left`). Cheap insurance the planner can pre-empt.

---

## Forward-decl fix (Pitfall 1) + the include swaps

`buddy.h:11` and `character.h:27` declare `class TFT_eSPI;`. With `using TFT_eSPI = lgfx::LGFXBase;` in `compat.h`, a `class TFT_eSPI;` is a **conflicting redeclaration** → hard error. **Recommended fix (PR #48 technique):** in each header, delete the `class TFT_eSPI;` line and add `#include "compat.h"` near the top (after `#include <stdint.h>`). `compat.h` is `#pragma once` and self-contained (it includes `<M5Unified.h>`), so pulling it into these headers is safe and gives the real `TFT_eSPI` alias to every TU that includes them. Include-order is fine: `main.cpp` includes `buddy.h` at line 8; that now transitively loads `compat.h` early, which is harmless.

### Every `#include <M5StickCPlus.h>` → compat.h swap (24 files)
| File:line | New include |
|-----------|-------------|
| `main.cpp:1` | `#include "compat.h"` |
| `buddy.cpp:3` | `#include "compat.h"` |
| `character.cpp:2` | `#include "compat.h"` |
| `xfer.h:75` | `#include "compat.h"` |
| `buddies/*.cpp:3` (all **20**: axolotl, blob, cactus, capybara, cat, chonk, doge, dragon, duck, ghost, goose, llama, mushroom, octopus, owl, penguin, rabbit, robot, snail, turtle) | `#include "../compat.h"` |

The 20 buddy files use **only** `extern TFT_eSprite spr;` + the `buddyPrintSprite`/`buddySetColor`/`buddySetCursor`/`buddyPrint` helpers (which call `LGFXBase` methods via a `TFT_eSPI*`). They reference **no** `M5.Axp`/`M5.Rtc`/`M5.Beep`/power names — so the include swap is their *only* change. Verified by reading doge.cpp + grepping the buddies dir.

### Headers that gain `#include "compat.h"` for the alias / helpers
- `buddy.h` (remove `class TFT_eSPI;` @11)
- `character.h` (remove `class TFT_eSPI;` @27)
- `stats.h` (uses compat power helpers — add the include so it compiles in any TU; it is compiled into both `main.cpp` and `buddy.cpp`)
- `data.h` (uses `RTC_*TypeDef` + `compatRtcSet`; add the include for safety, though it arrives transitively via `xfer.h`→`stats.h`)

---

## NEW Blocker 1 — LEDC API changed in arduino-esp32 core 3.x (gates PORT-03)

`[VERIFIED: ~/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-ledc.h — only ledcAttach(pin,freq,res) / ledcWrite(pin,duty) / ledcWriteChannel(ch,duty) exist; ledcSetup + ledcAttachPin are GONE]`

`main.cpp` motor code will not compile on core 3.3.9 (both envs):
| File:line | Old (core ≤2.x) | New (core 3.x) |
|-----------|------------------|----------------|
| `:1228` | `ledcSetup(VIBRATE_CH, 500, 8)` | `ledcAttach(VIBRATE_PIN, 500, 8)` |
| `:1229` | `ledcAttachPin(VIBRATE_PIN, VIBRATE_CH)` | *(folded into `ledcAttach` above — delete)* |
| `:1230, :103, :112` | `ledcWrite(VIBRATE_CH, x)` | `ledcWrite(VIBRATE_PIN, x)` *(pin, not channel)* |
| `:36` | `const int VIBRATE_CH = 2;` | now unused — keep or remove; `VIBRATE_PIN` (26) is what `ledcAttach`/`ledcWrite` take |

This compiles on both boards (GPIO26 is a valid pin number on both; on StickS3 the motor is never triggered because `settings().vibrate` gates it and Phase 4 routes StickS3 to chimes). **Scope note for the planner:** this is *compile-only* work needed for PORT-03; the haptic *behavior/collision* design stays Phase 4. Do not expand it into the Phase-4 chime engine here.

## NEW Blocker 2 — `BUTTON_A_PIN` / `BUTTON_B_PIN` undefined (gates PORT-03)

Used at `main.cpp:279-280` for light-sleep GPIO wake. They were `M5StickCPlus.h` macros; **nothing in M5Unified/M5GFX defines them** `[VERIFIED: grep over M5Unified/src + M5GFX/src — zero hits]`. Add to `compat.h`:
```cpp
// Button GPIOs (were M5StickCPlus.h macros; M5Unified does not define them).
#if defined(BOARD_STICKS3)
#ifndef BUTTON_A_PIN
#define BUTTON_A_PIN 37   // [ASSUMED] confirm StickS3 BtnA GPIO from PROJECT.md / board docs
#endif
#ifndef BUTTON_B_PIN
#define BUTTON_B_PIN 39   // [ASSUMED] confirm StickS3 BtnB GPIO
#endif
#else
#ifndef BUTTON_A_PIN
#define BUTTON_A_PIN 37   // StickC Plus BtnA (GPIO37)  [VERIFIED: M5Unified.cpp button setup]
#endif
#ifndef BUTTON_B_PIN
#define BUTTON_B_PIN 39   // StickC Plus BtnB (GPIO39)
#endif
#endif
```
StickC Plus pins are **verified** (M5Unified.cpp registers GPIO_NUM_37 for BtnA and GPIO_NUM_39 for BtnB on `board_M5StickCPlus`). StickS3 button GPIOs are `[ASSUMED]` — the light-sleep GPIO-wake is a StickC-Plus power feature; on StickS3 it must at least *compile*, and the correct wake pins should be confirmed from PROJECT.md's StickS3 I/O before relying on button-wake there. **Open question O1.**

---

## Per-call-site port map (the heart of PORT-01/02)

`M5.Imu.getAccelData(&ax,&ay,&az)` (`:188, :635, :777`) and **all** `M5.Lcd.*` / `MC_DATUM` / `TL_DATUM` sites are **UNCHANGED** (verified above) and omitted from the table.

### `src/main.cpp`
| Line(s) | Legacy | Replacement |
|---------|--------|-------------|
| 1 | `#include <M5StickCPlus.h>` | `#include "compat.h"` |
| 103, 112, 1230 | `ledcWrite(VIBRATE_CH, x)` | `ledcWrite(VIBRATE_PIN, x)` (core-3.x) |
| 192 | `M5.Axp.ScreenBreath(20+brightLevel*20)` | `compatScreenBreath(20+brightLevel*20)` |
| 208 | `M5.Axp.SetLDO2(true)` | `compatBacklight(true)` |
| 243-248 | `imuSleep()` raw `Wire1` 0x68 write | body → `M5.Imu.sleep();` |
| 263 | `M5.Axp.SetSleep()` | `compatRailSleep()` |
| 273 | `M5.Axp.WakeUpDisplayAfterLightSleep()` | `compatRailWake()` |
| 274 | `M5.Imu.Init()` | `M5.Imu.begin()` |
| 279-280 | `BUTTON_A_PIN`/`BUTTON_B_PIN` | now from `compat.h` (no edit; macros resolve) |
| 294 | `M5.Beep.tone(freq, dur)` | `compatBeep(freq, dur)` |
| 529 | `M5.Axp.PowerOff()` | `compatPowerOff()` |
| 605 | `M5.Axp.GetBtnPress() == 0x02` | `compatPowerBtnShort()` |
| 628 | `M5.Axp.GetVBusVoltage() > 4.0f` | `compatVBusVoltage() > 4.0f` (or `compatOnUsb()`) |
| 629 | `M5.Rtc.GetTime(&_clkTm)` | `compatRtcGetTime(&_clkTm)` |
| 630 | `M5.Rtc.GetDate(&_clkDt)` | `compatRtcGetDate(&_clkDt)` |
| 879-881 | `M5.Axp.GetBatVoltage/GetBatCurrent/GetVBusVoltage` | `compatBatVoltage()/compatBatCurrent()/compatVBusVoltage()` |
| 912 | `M5.Axp.GetTempInAXP192()` | `compatChipTempC()` |
| 1221 | `M5.begin()` | `M5.begin(cfg)` (D-09 block) |
| 1223 | `M5.Imu.Init()` | **delete** (cfg.internal_imu) |
| 1224 | `M5.Beep.begin()` | **delete** (cfg.internal_spk) |
| 1226-1227 | `pinMode(LED_PIN,OUTPUT); digitalWrite(LED_PIN,HIGH)` | `compatLedInit()` |
| 1228-1229 | `ledcSetup(VIBRATE_CH,500,8); ledcAttachPin(VIBRATE_PIN,VIBRATE_CH)` | `ledcAttach(VIBRATE_PIN,500,8)` |
| 1290 | `M5.Axp.GetBtnPress() != 0x02` | `!compatPowerBtnShort()` |
| 1297 | `M5.Beep.update()` | **delete** |
| 1321, 1323 | `digitalWrite(LED_PIN, LOW/HIGH)` | `compatLedSet(true/false)` (active-low handled inside) — e.g. `compatLedSet((now/400)%2)` then `else compatLedSet(false)` |
| 1435, 1642 | `M5.Axp.SetLDO2(false)` | `compatBacklight(false)` |
| 1625 | `M5.Axp.ScreenBreath(8)` | `compatScreenBreath(8)` |
| 1677 | `M5.Axp.GetBtnPress() == 0x02` | `compatPowerBtnShort()` |

> `LED_PIN`/`VIBRATE_CH` constants (`:29, :36`) become unused after the swaps — `compatLedSet` uses `COMPAT_LED_PIN` internally and core-3.x LEDC is pin-based. Remove or leave; harmless either way.

### `src/stats.h` (compiled into both `main.cpp` and `buddy.cpp`)
| Line | Legacy | Replacement |
|------|--------|-------------|
| top | (uses compat helpers) | add `#include "compat.h"` |
| 314 | `M5.Axp.EnableCoulombcounter()` | `compatEnableCoulomb()` (no-op) |
| 324 | `M5.Axp.GetVBusVoltage() > 4.0f` | `compatVBusVoltage() > 4.0f` |
| 325, 332 | `M5.Axp.GetBatVoltage() * 1000` | `compatBatVoltage() * 1000` |
| 326 | `M5.Axp.GetBatCurrent()` | `compatBatCurrent()` |
| 341, 357 | `M5.Axp.GetCoulombData()` | **no M5Unified equivalent** — see note |

**Coulomb-gauge note (D-04 / D-08):** M5Unified exposes no coulomb counter on either board. Recommended rewrite (minimal, both-board): make `batteryPct()` (`:355`) simply `return compatBatteryPct();` and reduce `batteryTick()` (`:339`) / `batteryInit()` (`:313`) to no-ops (or keep `batteryFull()`/`batteryPctVoltage()` as-is using the compat voltage helpers, but stop persisting the coulomb baseline). This is a **behavior change for the StickC Plus** (loses the coulomb-integrated accuracy in favor of `M5.Power.getBatteryLevel()`, a voltage/PMIC estimate). Per D-08 this needs a **`human_needed` battery-% sanity check** on hardware. `[ASSUMED]` `getBatteryLevel()` on AXP192 reads sensibly across the charge range.

### `src/xfer.h`
| Line | Legacy | Replacement |
|------|--------|-------------|
| 75 | `#include <M5StickCPlus.h>` | `#include "compat.h"` |
| 115 | `M5.Axp.GetBatVoltage() * 1000` | `compatBatVoltage() * 1000` |
| 116 | `M5.Axp.GetBatCurrent()` | `compatBatCurrent()` |
| 117 | `M5.Axp.GetVBusVoltage() * 1000` | `compatVBusVoltage() * 1000` |

### `src/data.h`
| Line | Legacy | Replacement |
|------|--------|-------------|
| 84-85 | `M5.Rtc.SetTime(&tm); M5.Rtc.SetDate(&dt);` | `compatRtcSet(&tm, &dt);` (single combined call) |

### `src/buddy.h` / `src/character.h`
Remove `class TFT_eSPI;` (lines 11 / 27 respectively); add `#include "compat.h"`.

### `src/buddy.cpp`, `src/character.cpp`, `src/buddies/*.cpp` ×20
Swap `#include <M5StickCPlus.h>` → `#include "compat.h"` (buddies use `"../compat.h"`). No other changes — they touch only the sprite/drawing API.

---

## Don't Hand-Roll

| Problem | Don't build | Use instead | Why |
|---------|-------------|-------------|-----|
| AXP192 rail cut for idle sleep | Raw `Wire`/`In_I2C` writes to reg 0x12 | `M5.Power.Axp192.setLDO2(0)/setLDO3(0)` | M5Unified already implements the exact `bitOff(0x12,…)` (verified); raw I2C risks bus contention with the library |
| IMU sleep | Raw `Wire1` 0x68 reg-0x6B write (current `imuSleep`) | `M5.Imu.sleep()` | One call, board-agnostic (MPU6886 + BMI270), no manual bus handling |
| Battery % | Re-implementing a coulomb integrator | `M5.Power.getBatteryLevel()` | No coulomb HW exposed; the PMIC gauge is the supported path |
| UI tone | A custom LEDC tone state machine | `M5.Speaker.tone(freq,dur)` | Async playback handled by the library; no per-loop `update()` |
| Battery/VBus float-volts shape | Inline `/1000` at every call site | the `compat*` wrappers | Keeps the existing call sites (which `*1000` to recover mV) numerically correct (Pitfall 3) |

---

## Common Pitfalls

### Pitfall 1: `class TFT_eSPI;` vs the `using` alias — covered above (remove decls + include compat.h).

### Pitfall 2: M5Canvas color depth on `createSprite` (runtime, MEDIUM)
`spr.createSprite(W,H)` (`main.cpp:1240`) for a 135×240 surface is ~63KB at 16bpp. LovyanGFX's `M5Canvas` may default to a different depth/arena than the old `TFT_eSprite`. **Add `spr.setColorDepth(16);` before `createSprite`** (PSRAM is available on StickS3 via `-DBOARD_HAS_PSRAM`). Symptom if wrong: garbled colors / black screen / null sprite. Not a compile blocker; flag for the on-device smoke test.

### Pitfall 3: float-volts vs int-mV (covered) — the `compat*` wrappers restore the float-volts shape the call sites `*1000`.

### Pitfall 4: `M5.Power.Axp192` in the wrong branch → StickS3 build breaks
`Axp192` is **not a member on S3 builds**. Keep every `M5.Power.Axp192.*` strictly inside `compat.h`'s `#else` (non-S3) branch. Symptom: `'class m5::Power_Class' has no member named 'Axp192'` on the `m5stack-sticks3` env.

### Pitfall 5: leftover raw `Wire1` after `imuSleep` rewrite
Once `imuSleep()` becomes `M5.Imu.sleep()`, no source includes/uses `Wire1` — fine. Don't leave a half-edited `Wire1.beginTransmission(...)`; replace the whole function body.

### Pitfall 6: deleting `M5.Beep.update()` but leaving a tone half-played
Old code pumped `M5.Beep.update()` each loop. `M5.Speaker` is fire-and-forget. Just delete the pump; do **not** add a busy-wait. Beeps are short (≤200ms) and overlap harmlessly.

---

## Build / Verify reality

- **PlatformIO on Windows:** invoke via `~/.platformio/penv/Scripts/platformio.exe` (or `pio` if on PATH). Long clean builds (M5Unified + M5GFX + AnimatedGIF compile is multi-minute on first build per env).
- **Order:** build `m5stickc-plus` **first** — it is the env that exercises the `#else` (AXP192) compat branch and the `M5.Power.Axp192` calls; it has *never* fully compiled on this toolchain, so expect the first real surfacing of any classic-ESP32-only gaps. Then build `m5stack-sticks3`.
  ```
  pio run -e m5stickc-plus
  pio run -e m5stack-sticks3
  ```
  Iterate: fix → rebuild the failing env → rebuild the other (a fix for one can regress the other, e.g. an `M5.Power.Axp192` call escaping the `#else` branch).
- **Anticipate beyond the mapped errors:** `MC_DATUM`/`TL_DATUM` (add compat defines if undeclared), StickS3 button-pin values, and any incidental `M5StickCPlus`-only name the grep-by-symbol below already covers. The symbol inventory (`M5.Axp|M5.Rtc|M5.Beep|M5.Imu|ledc|BUTTON_*`) was grepped exhaustively — no other legacy names remain in `src/`.
- **PORT-03 acceptance = both `pio run` commands exit 0.**

### Hardware verification (D-08 — `human_needed`, StickC Plus)
| Behavior | Why it needs a device | How to check |
|----------|----------------------|--------------|
| Battery % (coulomb→`getBatteryLevel` change) | gauge source changed | INFO/DEVICE page shows sane % vs charger state |
| Idle power draw | rail-cut + IMU-sleep ported | leave on battery, screen off, idle past sleep timeout; compare drain to pre-port |
| Display restore after idle sleep | `compatRailWake` LDO voltage `[ASSUMED 3000mV]` | press a button after idle sleep; screen must come back clean |
| Clock / RTC | `compatRtc*` already from Phase 2 | sync time from host, confirm clock face |
| LED on attention | `compatLedSet` | trigger an attention state, watch GPIO10 LED pulse |
| UI beep | `compatBeep`/`M5.Speaker` + D-09 speaker init | button presses + settings `sound` on → audible beep |

StickS3: if no device available, **clean compile/link is the acceptance** (D-08); if a device is available, smoke-test display + speaker beep + battery %.

---

## State of the Art

| Old | Current | Impact |
|-----|---------|--------|
| `M5.Axp` (board lib) | `M5.Power` (+ `M5.Power.Axp192` on classic ESP32) | one board-agnostic power API; the `#else` branch nearly vanishes |
| `M5.Beep` | `M5.Speaker.tone` | async; no `update()` pump |
| `M5.Imu.Init()` / raw `Wire1` IMU sleep | `M5.Imu.begin()` / `M5.Imu.sleep()` | board-agnostic IMU |
| `ledcSetup` + `ledcAttachPin` (core ≤2.x) | `ledcAttach(pin,freq,res)` + `ledcWrite(pin,duty)` (core **3.3.9**) | **compile blocker** if not ported |
| `M5.Lcd` (board lib type) | `M5.Lcd` alias of `M5.Display` (`M5GFX`) | call sites unchanged |

## Assumptions Log

| # | Claim | Section | Risk if wrong |
|---|-------|---------|---------------|
| A1 | `compatRailWake` restore voltage = **3000mV** for LDO2/LDO3 | RF-03/RF-04 | Medium — wrong value = dark/garbled screen after first idle cycle; caught by D-08 hardware check |
| A2 | StickS3 `BUTTON_A_PIN=37` / `BUTTON_B_PIN=39` | Blocker 2 | Low for compile; Medium for actual StickS3 button-wake — confirm from PROJECT.md (O1) |
| A3 | `M5.Power.getBatteryLevel()` on AXP192 reads sensibly across the range | stats.h note | Low — cosmetic %; voltage fallback exists |
| A4 | M5GFX provides `MC_DATUM`/`TL_DATUM` legacy names | RF-05 | Low — trivial 2-line compat `#define` if not |
| A5 | M5Canvas needs explicit `setColorDepth(16)` for the 135×240 sprite | Pitfall 2 | Medium — runtime visual bug only, not a compile blocker |
| A6 | Deleting `M5.Beep.update()` (async speaker) causes no missed/cut beeps | D-10 | Low — beeps are short and fire-and-forget |

## Open Questions

1. **O1 — StickS3 BtnA/BtnB GPIOs for light-sleep wake.** PROJECT.md documents StickS3 I/O (ESP32-S3-PICO-1); the exact button GPIOs were not in the lines read. The light-sleep GPIO-wake path is a StickC-Plus power feature, so on StickS3 it must compile but isn't power-critical for this milestone. **Recommendation:** use the verified StickC-Plus values as the `#else`, put `[ASSUMED]` S3 values behind `#if BOARD_STICKS3`, and have the planner confirm the S3 pins from PROJECT.md / the M5 board pin map before relying on StickS3 button-wake.
2. **O2 — Keep or drop the StickC-Plus coulomb gauge?** Dropping it (recommended) is the minimal both-board path but changes StickC-Plus battery-% behavior. If the user wants to preserve coulomb accuracy on the StickC Plus, it would require direct AXP192 reg reads via `M5.In_I2C` (more code, board-conditional) — out of scope unless D-08 hardware testing shows the `getBatteryLevel()` reading is unacceptable.

## Environment Availability

| Dependency | Required by | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| PlatformIO + espressif32 | build | ✓ | platform 55.03.39 / core 3.3.9 | — |
| M5Unified / M5GFX | all | ✓ | 0.2.x (in `.pio/libdeps` for both envs) | — |
| `-DBOARD_STICKS3` flag | board conditionals | ✓ | set only in `[env:m5stack-sticks3]` | — |
| StickC Plus device | D-08 smoke test | `human` | — | none (hardware check is part of "done") |
| StickS3 device | D-08 (optional) | `human` | — | clean compile/link = acceptance |

**Blocking with no fallback:** none for the build. The only hard requirement is the dual-env compile, which is fully specified above.

## Security Domain

`security_enforcement: true`, ASVS L1. This phase is an internal API port — **no new** network, auth, input-parsing, or crypto surface. The BLE/JSON inbound path (`data.h::_applyJson`, the bonded NUS characteristic) is unchanged; the only edit there routes `M5.Rtc.SetTime/SetDate` → `compatRtcSet` (Phase-2 helper). The host-`{"time":…}` trust boundary is identical to before (already an encrypted-and-bonded peer). No ASVS category newly applies. `nyquist_validation: false` → Validation Architecture section intentionally omitted (no test framework in this embedded project; acceptance is dual-env compile + hardware smoke).

## Sources

### Primary (HIGH — read from installed libraries / toolchain in this session)
- `.pio/libdeps/m5stickc-plus/M5Unified/src/utility/Power_Class.hpp` — `M5.Power` API + the `Axp192` member's per-target visibility
- `.pio/libdeps/.../power/AXP192_Class.hpp` + `AXP192_Class.cpp` — `setLDO2/3`, `_set_LDO`→`bitOff(0x12,…)` rail semantics
- `.pio/libdeps/.../IMU_Class.hpp` — `getAccelData`, `sleep()`, `begin()`
- `.pio/libdeps/.../Speaker_Class.hpp` — `tone(float,uint32_t,…)`, `setVolume`, `begin`
- `.pio/libdeps/.../M5Unified.hpp` — `config_t` defaults, `M5GFX &Lcd = Display`, member instances; `M5Unified.cpp` — StickCPlus button GPIO setup + `spk_cfg.buzzer`
- `~/.platformio/.../esp32-hal-ledc.h` + `platform.json` — core 3.3.9 LEDC API
- Repo `src/` (main.cpp, buddy.{h,cpp}, character.{h,cpp}, stats.h, xfer.h, data.h, compat.h, buddies/doge.cpp) + exhaustive symbol grep — the call-site inventory
- `.planning/phases/02-compatibility-shim/02-RESEARCH.md` — the compat.h base + mapping + Pitfall 1

### Secondary (MEDIUM)
- M5GFX `LGFXBase.hpp` — `textdatum_t` / `setTextDatum(uint8_t)` overload (basis for the MC_DATUM/TL_DATUM MEDIUM confidence)

## Metadata

**Confidence breakdown:**
- RF-04 power collapse + RF-05 alias + D-09 cfg + D-10 beep: **HIGH** — read from installed headers.
- LEDC core-3.x blocker + button-pin blocker: **HIGH** — verified against installed toolchain/grep.
- RF-03 rail-sleep mechanism: **HIGH**; restore voltage: **MEDIUM** (A1, hardware-confirm).
- Runtime visuals (sprite depth, datum names): **MEDIUM** — not exercised by the compile bar.

**Research date:** 2026-06-28
**Valid until:** ~2026-07-28 (stable; revisit if M5Unified or the espressif32 core pin moves)
