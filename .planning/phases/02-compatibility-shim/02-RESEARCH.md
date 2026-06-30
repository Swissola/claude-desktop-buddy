# Phase 2: Compatibility Shim - Research

**Researched:** 2026-06-28
**Domain:** Embedded C++ (Arduino/PlatformIO) — porting an M5StickCPlus-library firmware to M5Unified + M5GFX behind a `src/compat.h` shim
**Confidence:** HIGH

## Summary

Phase 2 produces `src/compat.h` — a header that re-creates the legacy M5StickCPlus
API names (`TFT_eSprite`, `TFT_eSPI`, `RTC_*TypeDef`) on top of M5Unified/M5GFX, plus
`compat*` helper functions for the peripherals whose APIs changed (power, RTC, LED, chip
temp). The phase's narrow acceptance bar (success criterion 3) is that **a translation
unit including only `compat.h` (+ M5Unified/M5GFX) compiles with zero errors** under
`pio run -e m5stack-sticks3`. Actually editing `main.cpp`/`stats.h`/`xfer.h`/`data.h` to
call the shim is **Phase 3**.

I retrieved the **verbatim PR #48 `compat.h`** (from the head fork `yiduo/claude-desktop-buddy`
at the PR head SHA `2cb901e`) — it is reproduced in full below and is the D-07 base. It is
~120 lines and already provides: the two graphics `using` aliases, `GREEN`/`RED` macros, the
software RTC (`RTC_*TypeDef` + `compatRtcSet`/`compatRtcGetTime`/`compatRtcGetDate` backed by
the ESP32 system clock via `settimeofday`/`gmtime_r` + a portable `timegm`), `compatOnUsb()`,
board-conditional `compatLedInit`/`compatLedSet` (no-op on StickS3), and `compatChipTempC()`.

Two research flags resolved with HIGH confidence:
- **RF-01 (host time): HOST TIME IS ALREADY AVAILABLE.** `src/data.h::_applyJson` already
  parses `{"time":[epoch_sec, tz_offset_sec]}` off the inbound NUS/USB stream and seeds the
  RTC. D-01 is realizable with **zero host-side changes** — the protocol already carries time;
  the shim just needs to route `SetTime/SetDate` into the software clock. D-02's free-running
  fallback is automatically satisfied (the system clock free-runs from boot until first sync).
- **RF-02 (StickS3 LED): NO usable user LED → `compatLed` is a no-op on StickS3.** PR #48's
  reference compat.h states it explicitly ("StickS3 has no user LED; GPIO10 is Grove Port-A"),
  matching PROJECT.md. Adopt the no-op stub.

**Primary recommendation:** Take PR #48's `compat.h` verbatim as the base, then **extend it
with the additional `compat*` power/sleep/battery helpers this fork's larger `main.cpp`/`stats.h`
needs** (battery voltage/current, screen brightness, backlight rail, power-button, power-off,
coulomb/sleep stubs). Wire every new helper board-conditionally (`#if defined(BOARD_STICKS3)`)
so the StickC Plus keeps its native AXP192/RTC paths. Do **not** build a fake `M5.Axp`/`M5.Rtc`
object — replicate PR #48's technique of helper functions that Phase 3 swaps call sites onto.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Graphics typedefs (`TFT_eSprite`/`TFT_eSPI`) | compat.h header (compile-time alias) | M5GFX runtime | Pure type aliases; M5Canvas/LGFXBase already provide the drawing API 1:1 |
| Software RTC (wall clock) | compat.h + ESP32 system clock | BLE host (`{"time":...}` seed) | No RTC chip on StickS3; `time()`/`settimeofday()` is the clock, host BLE supplies truth |
| Battery / charging / USB sense | compat.h helper → `M5.Power` (PMIC) | — | M5Unified's `M5.Power` already abstracts AXP192 (StickC+) and M5PM1 (StickS3) |
| Power button | compat.h helper → `M5.Power.getKeyState()` | — | Power button is on the PMIC, not a GPIO; M5Unified normalizes it |
| Backlight / brightness | compat.h helper → `M5.Display.setBrightness()` | — | Display tier owns backlight; AXP `ScreenBreath`/`SetLDO2` have no StickS3 analog |
| Onboard LED feedback | compat.h helper (GPIO10 on StickC+; no-op on StickS3) | — | Board-specific GPIO; StickS3 has none |
| Coulomb counter / chip temp / rail sleep | compat.h safe-stubs (D-04/D-05) | `M5.Power.getBatteryLevel()` fallback | AXP192-only features; no clean StickS3 analog |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `m5stack/M5Unified` | `^0.2.0` (Phase-1 locked) | Board-agnostic HAL: `M5.Display`, `M5.Power`, `M5.Imu`, `M5.Btn*`, `M5.Speaker` | Auto-detects StickC Plus vs StickS3 at runtime; the project-wide replacement for the board-specific `M5StickCPlus` lib |
| `m5stack/M5GFX` | `^0.2.0` (Phase-1 locked) | Graphics: `M5Canvas` (sprite), `lgfx::LGFXBase` (common surface base) | LovyanGFX-based; provides the exact `TFT_eSprite`/`TFT_eSPI` drawing API under different class names |

No new libraries are introduced in this phase. M5Unified + M5GFX were added in Phase 1
(BUILD-02). compat.h is a single header — there is nothing to `npm`/`pip`/`cargo` install.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Helper functions (`compatBatVoltage()`…) that Phase 3 swaps onto | A fake `struct { } Axp;` global named `M5.Axp` to keep call sites byte-identical | Rejected: PR #48 (the D-07 base) uses helper functions and edits call sites; a fake `M5.Axp` would collide with M5Unified's `M5` struct and fight the library. Replicate the reference technique. |
| `lgfx::LGFXBase` for `TFT_eSPI` | `M5GFX` (the concrete LCD class) | `LGFXBase` is the common base of BOTH `M5Canvas` (sprite) and `M5GFX` (live LCD), so a `TFT_eSPI*` can point at either — exactly what `buddyRenderTo`/`characterRenderTo` need (they receive `&spr` OR `&M5.Lcd`). |

## Package Legitimacy Audit

Not applicable — this phase installs no external packages. The only dependencies
(`m5stack/M5Unified ^0.2.0`, `m5stack/M5GFX ^0.2.0`) were vetted and locked in Phase 1's
`platformio.ini`. slopcheck was unavailable at research time, but no package decisions are
made here.

## The PR #48 Reference `compat.h` (D-07 verbatim base)

**Source:** `yiduo/claude-desktop-buddy` @ `feat/m5sticks3-port`, head SHA
`2cb901e2d15d1d121689d0f65cb7075d0bb04a28`, path `src/compat.h`
(fetched via authenticated `gh api` — PR #48 originates from this fork, not the upstream repo).
`[VERIFIED: gh api repos/yiduo/claude-desktop-buddy/contents/src/compat.h]`

```cpp
#pragma once
// Hardware compatibility shim. (header comment trimmed for brevity)
#include <M5Unified.h>
#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

// --- Display surfaces ---
using TFT_eSprite = M5Canvas;
using TFT_eSPI    = lgfx::LGFXBase;

// --- Legacy bare color names ---
#ifndef GREEN
#define GREEN 0x07E0
#endif
#ifndef RED
#define RED 0xF800
#endif

// --- Software RTC ---
struct RTC_TimeTypeDef { uint8_t Hours, Minutes, Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay, Month, Date; uint16_t Year; };

// Portable UTC broken-down-time -> epoch (newlib lacks timegm).
static inline time_t _compatTimegm(const struct tm* t) {
  static const int mdays[] = { 0,31,59,90,120,151,181,212,243,273,304,334 };
  long y = t->tm_year + 1900;
  long days = (y-1970)*365 + (y-1969)/4 - (y-1901)/100 + (y-1601)/400;
  days += mdays[t->tm_mon % 12];
  if (t->tm_mon > 1 && ((y%4==0 && y%100!=0) || y%400==0)) days += 1;
  days += t->tm_mday - 1;
  return ((time_t)days * 24 + t->tm_hour) * 3600 + t->tm_min * 60 + t->tm_sec;
}

static inline void compatRtcSet(const RTC_TimeTypeDef* tm, const RTC_DateTypeDef* dt) {
  struct tm t = {};
  t.tm_hour = tm->Hours; t.tm_min = tm->Minutes; t.tm_sec = tm->Seconds;
  t.tm_mday = dt->Date;  t.tm_mon = (int)dt->Month - 1;
  t.tm_year = (int)dt->Year - 1900;
  time_t epoch = _compatTimegm(&t);
  struct timeval now = { epoch, 0 };
  settimeofday(&now, nullptr);
}
static inline void _compatRtcNow(struct tm* out) {
  time_t now = time(nullptr); gmtime_r(&now, out);
}
static inline void compatRtcGetTime(RTC_TimeTypeDef* tm) {
  struct tm t; _compatRtcNow(&t);
  tm->Hours = t.tm_hour; tm->Minutes = t.tm_min; tm->Seconds = t.tm_sec;
}
static inline void compatRtcGetDate(RTC_DateTypeDef* dt) {
  struct tm t; _compatRtcNow(&t);
  dt->WeekDay = t.tm_wday; dt->Month = t.tm_mon + 1;
  dt->Date = t.tm_mday;   dt->Year = t.tm_year + 1900;
}

// --- Power / USB ---
static inline bool compatOnUsb() {
  int v = M5.Power.getVBUSVoltage();
  if (v > 0) return v > 4000;
  return (int)M5.Power.isCharging() != 0;
}

// --- Onboard LED ---
#if defined(BOARD_STICKS3)
static inline void compatLedInit() {}
static inline void compatLedSet(bool) {}
#else
static const int COMPAT_LED_PIN = 10;
static inline void compatLedInit() {
  pinMode(COMPAT_LED_PIN, OUTPUT);
  digitalWrite(COMPAT_LED_PIN, HIGH);   // active-low: HIGH = off
}
static inline void compatLedSet(bool on) {
  digitalWrite(COMPAT_LED_PIN, on ? LOW : HIGH);
}
#endif

// --- Chip temperature ---
static inline int compatChipTempC() { return (int)temperatureRead(); }
```

### What the base already covers vs. what this fork additionally needs

| Need in this fork | In PR #48 base? | Action |
|-------------------|-----------------|--------|
| `TFT_eSprite`/`TFT_eSPI` aliases | ✅ | Keep verbatim |
| `GREEN`/`RED` macros | ✅ | Keep — **and these are the ONLY two TFT color macros this fork uses** (grep confirms: 6×`GREEN`, 1×`RED`, no other `TFT_*`/`WHITE`/`BLACK`/etc.). No extension needed. |
| `RTC_*TypeDef` + Get/Set | ✅ | Keep verbatim — struct layout matches this fork exactly (see Legacy Surface) |
| `compatOnUsb()` | ✅ | Keep verbatim |
| `compatLed*` (no-op on StickS3) | ✅ | Keep verbatim (RF-02 confirms no-op) |
| `compatChipTempC()` | ✅ | Keep verbatim (satisfies D-05) |
| Battery voltage/current/VBus accessors | ❌ | **ADD** `compatBatVoltage()`, `compatBatCurrent()`, `compatVBusVoltage()` |
| Battery % (coulomb→fallback) | ❌ | **ADD** `compatBatteryPct()` → `M5.Power.getBatteryLevel()` on StickS3 |
| Screen brightness (`ScreenBreath`) | ❌ | **ADD** `compatScreenBreath(int)` → `M5.Display.setBrightness()` |
| Backlight rail (`SetLDO2`) | ❌ | **ADD** `compatBacklight(bool)` |
| Power button (`GetBtnPress`) | ❌ | **ADD** `compatPowerBtnShort()` → `M5.Power.getKeyState()==2` |
| Power off (`PowerOff`) | ❌ | **ADD** `compatPowerOff()` → `M5.Power.powerOff()` |
| Coulomb counter / rail-sleep stubs (D-04) | ❌ | **ADD** no-op stubs on StickS3 |

> PR #48 confirms the fork files `buddy.h`, `buddy.cpp`, `character.h`, `character.cpp`,
> `data.h`, `xfer.h`, `main.cpp` were all edited in that PR — i.e. the call-site porting is
> real Phase-3 work there too, not done by the header alone.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SHIM-01 | `src/compat.h` re-creates legacy names on M5Unified/M5GFX (`TFT_eSprite`→`M5Canvas`, `TFT_eSPI`→`lgfx::LGFXBase`, software RTC, `compatOnUsb`/`compatLed`/`compatChipTempC`) so UI/render code is untouched | PR #48 base reproduced verbatim above + the exact legacy-surface inventory and `compat*` extension set below. Acceptance = compat.h-only TU compiles under `pio run -e m5stack-sticks3` (criterion 3). |
</phase_requirements>

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Software RTC syncs from the BLE host time as primary source; free-run from `millis()` between syncs.
- **D-02 (fallback, mandatory):** If host BLE carries no usable time, fall back to a free-running software RTC seeded at first connect/boot (PR #48 style). Shim must compile and behave sanely with NO host time. Do NOT introduce host-side protocol changes.
- compat.h must provide `RTC_TimeTypeDef`/`RTC_DateTypeDef` and `M5.Rtc.GetTime/SetTime/GetDate/SetDate` equivalents (4 call sites + 2 type uses today).
- **D-03:** Map AXP calls with a real M5Unified equivalent onto `M5.Power` (battery V/level/current; VBus/USB/charging → `compatOnUsb`; power button → M5Unified power-button API; `ScreenBreath` → `M5.Display.setBrightness`; `PowerOff` → `M5.Power.powerOff`).
- **D-04:** Stub AXP-only calls with no clean StickS3 analog to safe no-ops/zero returns: `SetLDO`, `EnableCoulombcounter`/`GetCoulombData`, `GetTempInAXP`, `SetSleep`, `WakeUpDisplayAfterLightSleep`. Battery-% from coulomb counter falls back to a voltage-based estimate where the UI needs a number. All power shims board-conditional; StickC Plus keeps real AXP192.
- **D-06:** Map `compatLed` to a StickS3 LED if hardware exposes one (RF-02); else no-op stub. Compile must not depend on a StickS3 LED existing.
- **D-07:** Build compat.h by starting from PR #48's `compat.h` verbatim, then extending only as this fork's compile errors demand. Iterate compile → add missing name → recompile. Do NOT cherry-pick PR #48 commits; replicate the header content/technique.

### Claude's Discretion
- Graphics typedefs are pre-decided: `TFT_eSprite` → `M5Canvas`, `TFT_eSPI` → `lgfx::LGFXBase`.
- Exact board-conditional structure (`#if defined(BOARD_STICKS3)` blocks vs paired headers) and how `compatOnUsb` is wired are Claude's call.
- **D-05:** Chip-temp source — StickC Plus reads AXP192 temp; on StickS3 use ESP32-S3 internal temp sensor if trivial, else sentinel/0. Low priority; does not block the compile bar.
- Any voltage→percentage curve for the battery fallback.

### Deferred Ideas (OUT OF SCOPE)
- **Host-side time broadcast:** adding a timestamp to the companion's BLE protocol — future, host-side. (NOTE: RF-01 finds host time is ALREADY present, so D-01 needs nothing host-side anyway.)
- **StickS3 chip-temperature fidelity** beyond the D-05 minimum.
- API porting of `main.cpp` + buddy/character sources to actually adopt the unified API → **Phase 3**. Haptics → chimes → **Phase 4**.
</user_constraints>

## Legacy-API Surface (the exact names the shim must satisfy)

Enumerated from the repo source (`[VERIFIED: Grep over src/]`). "Phase" column notes whether
the name must be **resolved by the header for the standalone compile bar (P2)** or is a
**call-site port done in Phase 3 (P3)** that the header's helpers enable. The compat.h-only
TU (criterion 3) needs only the typedefs + helper declarations to compile; the `M5.Axp.*` /
`M5.Rtc.*` / `M5.Beep` call sites belong to Phase 3.

### Graphics (P2 — types; P3 — forward-decl fixes)
| Legacy name | Where | Replacement | Notes |
|-------------|-------|-------------|-------|
| `TFT_eSprite` (type, ~25 uses incl. `extern TFT_eSprite spr`) | main.cpp:10; all `src/buddies/*.cpp:6` | `using TFT_eSprite = M5Canvas;` | Constructor `TFT_eSprite(&M5.Lcd)` → `M5Canvas(&M5.Lcd)` OK (`M5.Lcd` is an `M5GFX` ⊂ `LGFXBase`) |
| `TFT_eSPI` (type, pointer params) | buddy.h:11,12; buddy.cpp:33,36,170,175; character.h:27,28; character.cpp:46,47,250,253,255 | `using TFT_eSPI = lgfx::LGFXBase;` | **LANDMINE:** `buddy.h:11` and `character.h:27` contain `class TFT_eSPI;` forward declarations. A `using`-alias + a later `class TFT_eSPI;` is a **conflicting redeclaration** → compile error. Those forward decls must be replaced by `#include "compat.h"` (Phase 3 edit; PR #48 edited both headers). |
| Drawing methods on `spr`/`M5.Lcd`/`TFT_eSPI*` | throughout | identical on M5Canvas/LGFXBase | `fillSprite, createSprite, setPivot, pushSprite, pushRotated, setRotation, fillScreen, fillRect, fillRoundRect, drawRoundRect, drawFastHLine, fillTriangle, fillCircle, drawCircle, drawLine, drawRect, setTextDatum, setTextSize, setTextColor, setCursor, print, printf, drawString` — all present in M5GFX/LovyanGFX. `pushSprite/pushRotated/setPivot/createSprite` are called on the `M5Canvas` object directly (not via `TFT_eSPI*`), so no base-class gap. |
| `MC_DATUM`/`TL_DATUM`, `GREEN`, `RED` | main.cpp, buddies | M5GFX provides the datums; compat.h defines `GREEN`/`RED` | Only `GREEN`/`RED` color macros are used (grep-confirmed) — base already covers both. |

### RTC (P2 — types + helpers; P3 — call-site swaps)
| Legacy call | Where | Replacement helper | Signature the caller expects |
|-------------|-------|--------------------|------------------------------|
| `RTC_TimeTypeDef` (type) | main.cpp:621; data.h:81 | `struct { uint8_t Hours,Minutes,Seconds; }` | Field-init order in data.h:81 is `{Hours,Minutes,Seconds}` ✓; reads `.Hours/.Minutes/.Seconds` (main.cpp:684-727) |
| `RTC_DateTypeDef` (type) | main.cpp:622; data.h:82 | `struct { uint8_t WeekDay,Month,Date; uint16_t Year; }` | Field-init in data.h:82 is `{WeekDay,Month,Date,Year}` ✓; reads `.WeekDay/.Month/.Date` (main.cpp:680,692-693,726) |
| `M5.Rtc.GetTime(&t)` | main.cpp:629 | `compatRtcGetTime(&t)` | `void(RTC_TimeTypeDef*)` |
| `M5.Rtc.GetDate(&d)` | main.cpp:630 | `compatRtcGetDate(&d)` | `void(RTC_DateTypeDef*)` |
| `M5.Rtc.SetTime(&t)` + `M5.Rtc.SetDate(&d)` | data.h:84-85 | `compatRtcSet(&t, &d)` (single combined call) | PR #48 folds the two setters into one `compatRtcSet`; Phase 3 edits data.h to call it once. |

### Power / AXP192 (P3 call-site swaps; helpers added to compat.h in P2)
Every distinct `M5.Axp.*` call, its expected signature, and the mapping. `[VERIFIED: M5Unified
Power_Class.hpp + m5-docs]` for the `M5.Power` targets.

| Legacy call | Returns (caller expects) | Where | StickS3 mapping (D-03/D-04) | Suggested helper |
|-------------|--------------------------|-------|-----------------------------|------------------|
| `M5.Axp.GetBatVoltage()` | `float` **volts** | main.cpp:879; stats.h:325,332; xfer.h:115 | `M5.Power.getBatteryVoltage()` → `int16_t` **mV**; divide by 1000 | `float compatBatVoltage()` → `M5.Power.getBatteryVoltage()/1000.0f` |
| `M5.Axp.GetBatCurrent()` | `float` **mA** | main.cpp:880; stats.h:326; xfer.h:116 | `M5.Power.getBatteryCurrent()` → `int32_t` **mA** (+charge/−discharge) | `float compatBatCurrent()` |
| `M5.Axp.GetVBusVoltage()` | `float` **volts** | main.cpp:628,881; stats.h:324; xfer.h:117 | `M5.Power.getVBUSVoltage()` → `int16_t` **mV** (−1 if unsupported) | `float compatVBusVoltage()` (and prefer `compatOnUsb()` for the boolean checks) |
| `M5.Axp.ScreenBreath(int)` | `void`; arg range used 8..100 | main.cpp:192,1625 | `M5.Display.setBrightness(uint8_t 0..255)` | `void compatScreenBreath(int v)` → `setBrightness(map(constrain(v,0,100),0,100,0,255))` |
| `M5.Axp.SetLDO2(bool)` | `void` (backlight rail on/off) | main.cpp:208,1435,1642 | No AXP rail on StickS3 → emulate via brightness: `setBrightness(on?restore:0)` | `void compatBacklight(bool on)`; on StickC Plus keep `M5.Axp.SetLDO2` (board-conditional) |
| `M5.Axp.PowerOff()` | `void` | main.cpp:529 | `M5.Power.powerOff()` | `void compatPowerOff()` |
| `M5.Axp.GetBtnPress()` | `uint8_t`; `==0x02` = short press | main.cpp:605,1290,1677 | `M5.Power.getKeyState()` → `0=none,1=long,2=short,3=both`; short = `==2`. (`M5.BtnPWR.wasClicked()` also works but is edge-latched; `getKeyState()` is the closer 1:1 to the consumed-on-read AXP call.) | `bool compatPowerBtnShort()` → `M5.Power.getKeyState()==2` |
| `M5.Axp.GetTempInAXP192()` | `float`/`int` °C | main.cpp:912 | ESP32-S3 internal sensor | `compatChipTempC()` (already in base) → `temperatureRead()` |
| `M5.Axp.EnableCoulombcounter()` | `void` | stats.h:314 | **D-04 stub** no-op on StickS3 | `void compatEnableCoulomb()` → `{}` |
| `M5.Axp.GetCoulombData()` | `float` mAh | stats.h:341,357 | **D-04**: no coulomb HW. Route battery % away from this path (see below) | n/a — see `compatBatteryPct()` |
| `M5.Axp.SetSleep()` | `void` (cuts LDO2/LDO3 rails) | main.cpp:263 | **D-04 stub** no-op on StickS3 (no AXP rails); backlight already handled by `compatBacklight` | `void compatRailSleep()` → `{}` |
| `M5.Axp.WakeUpDisplayAfterLightSleep()` | `void` | main.cpp:273 | **D-04 stub** no-op on StickS3 | `void compatRailWake()` → `{}` |

**Battery-% fallback (D-04):** the coulomb-counter gauge in `stats.h` (`batteryInit`/`batteryTick`/
`batteryPct`/`batteryPctVoltage`, `BATT_CAPACITY_MAH=120`) is AXP192-specific. On StickS3,
have `batteryPct()` (Phase-3 board-conditional edit) return `M5.Power.getBatteryLevel()`
(0..100, M5Unified's own ADC/PMIC gauge) and `#if`-out the coulomb path. If a manual
voltage curve is preferred (Claude's discretion), the existing `batteryPctVoltage()` formula
`(mV-3200)/10` clamped 0..100 is a reasonable LiPo approximation but reads high off the
charger; `M5.Power.getBatteryLevel()` is the better default.

### LED (P2 helpers; P3 call-site swaps) — RF-02 resolved
| Legacy name | Where | Replacement | Verdict |
|-------------|-------|-------------|---------|
| `LED_PIN` (=10), `pinMode`, `digitalWrite(LED_PIN, HIGH/LOW)` | main.cpp:29,1226-1227,1321,1323 | `compatLedInit()` / `compatLedSet(bool)` | **StickS3: no-op** (RF-02). StickC Plus: GPIO10 active-low, exactly as base. |

### Out-of-Phase-2 names that Phase 3/4 must still resolve (flagged, not built here)
| Legacy name | Where | Belongs to | Note |
|-------------|-------|-----------|------|
| `M5.Beep.tone/begin/update` | main.cpp:294,1224,1297 | Phase 3/4 | Not in M5Unified → `M5.Speaker`. The "sound" UI-beep path. |
| `M5.Imu.Init()` | main.cpp:1223,274 | Phase 3 | M5Unified uses `M5.Imu.begin()`; `getAccelData(&ax,&ay,&az)` exists. |
| `imuSleep()` raw `Wire1` 0x68 reg writes | main.cpp:243-248 | Phase 3 | MPU6886-specific (StickC Plus). StickS3 IMU (BMI270) differs → must be board-conditional. |
| `ledcSetup`/`ledcAttachPin`/`ledcWrite` | main.cpp:103,112,1228-1230 | Phase 4 | Vibration path. ESP32-S3 Arduino-core 3.x changed the LEDC API (`ledcAttach`). Board-conditional in Phase 4. |

## RF-01 Verdict — BLE host time IS available (D-01 fully realizable, no host changes)

`[VERIFIED: src/data.h:70-91 + src/ble_bridge.cpp:61-66]`

The inbound NUS stream is drained in `dataPoll` (data.h:147-184) → fed line-by-line into
`_applyJson` (data.h:70). That function **already parses a time message**:

```cpp
// src/data.h:76-91 (verbatim)
JsonArray t = doc["time"];
if (!t.isNull() && t.size() == 2) {
  time_t local = (time_t)t[0].as<uint32_t>() + (int32_t)t[1];  // epoch + tz_offset
  struct tm lt; gmtime_r(&local, &lt);
  RTC_TimeTypeDef tm = { (uint8_t)lt.tm_hour, (uint8_t)lt.tm_min, (uint8_t)lt.tm_sec };
  RTC_DateTypeDef dt = { (uint8_t)lt.tm_wday, (uint8_t)(lt.tm_mon + 1),
                         (uint8_t)lt.tm_mday, (uint16_t)(lt.tm_year + 1900) };
  M5.Rtc.SetTime(&tm);
  M5.Rtc.SetDate(&dt);
  extern uint32_t _clkLastRead; _clkLastRead = 0;  // force re-read
  _rtcValid = true; ...
}
```

- The host (Claude Desktop/Code companion) sends `{"time":[epoch_sec, tz_offset_sec]}` over
  the **same** NUS RX characteristic (`6e400002-…`, `RxCallbacks::onWrite` → ring buffer →
  `bleRead`) and USB serial. There is **no separate characteristic and no protocol change
  needed** — the time arrives in the normal JSON stream.
- **Verdict (a): host time IS available now.** Wire D-01 by routing the existing
  `M5.Rtc.SetTime/SetDate` pair (data.h:84-85) onto `compatRtcSet(&tm,&dt)` (Phase-3 edit).
  No host-side work.
- **D-02 free-running fallback is automatic.** The software clock is the ESP32 system clock
  (`time()` / `settimeofday()`). Before the first `{"time":…}` message it free-runs from an
  arbitrary base (seconds-since-boot from epoch 0); `_rtcValid`/`dataRtcValid()` gates whether
  the UI treats the clock as trustworthy. The shim compiles and runs with zero host time.
- **No new characteristic, no new parsing** belongs in Phase 2 — the seam already exists.

## RF-02 Verdict — StickS3 has no usable user LED → `compatLed` no-op

`[CITED: PR #48 src/compat.h] [CITED: .planning/PROJECT.md StickS3 I/O]`

The PR #48 reference compat.h (the D-07 base) states it directly: *"StickC Plus: red LED on
GPIO10, active-low. StickS3 has no user LED (GPIO10 is Grove Port-A there), so the LED calls
compile to no-ops."* PROJECT.md likewise documents no StickS3 LED (GPIO10 = Grove Port.A G10).
Web search for an M5StickS3 onboard LED returned only generic ESP32-S3 dev-board WS2812 results,
not a documented M5StickS3 user LED. `[ASSUMED→low-risk]` that no addressable LED exists; even
if one did, D-06 permits the no-op, and the base already ships it.

**Verdict:** Adopt PR #48's board-conditional `compatLedInit`/`compatLedSet` verbatim — no-op
on StickS3, GPIO10 active-low on StickC Plus.

## Software RTC Pattern (no RTC chip on ESP32-S3)

The proven pattern (PR #48) is **back the clock with the ESP32 system clock, not `M5.Rtc`**:
- `compatRtcSet` converts the legacy broken-down `RTC_*TypeDef` (which the host already
  delivers as **already-localized** components) to epoch via a portable `timegm`
  (`_compatTimegm`, since newlib lacks `timegm`), then `settimeofday()`.
- `compatRtcGetTime/GetDate` read `time(nullptr)` and `gmtime_r` back to components.
- Using `timegm`↔`gmtime_r` (UTC both directions) means **no timezone double-application** —
  the host already added its offset, so the device must treat the value as wall-clock UTC.
- This free-runs from `millis()`/RTC peripheral underneath `time()` between syncs — exactly
  D-02. It also benefits the StickC Plus (works without its coin-cell RTC).
- **M5Unified `M5.Rtc` note:** on a board with no RTC chip, `M5.Rtc` is effectively a no-op /
  reports unsupported. Do **not** route the clock through `M5.Rtc` — use the system-clock
  helpers above. (This is why the shim provides `compatRtc*` functions, not an `M5.Rtc` alias.)

## Architecture Patterns

### System / include flow
```
                 ┌──────────────────────────────────────────────┐
  host BLE NUS   │  ble_bridge.cpp RxCallbacks::onWrite          │
  {"time":[..]}  │     → rxPush ring → bleRead()                 │
  ───────────────┼──► data.h dataPoll → _applyJson               │
                 │        ├─ {"time":...} → compatRtcSet()  ◄── seeds software clock (D-01)
                 │        └─ session JSON → TamaState             │
                 └──────────────────────────────────────────────┘
                                    │
   main.cpp / stats.h / xfer.h ─────┤  call compat* helpers + typedefs
                                    ▼
        ┌──────────────────── src/compat.h ────────────────────┐
        │  using TFT_eSprite=M5Canvas;  using TFT_eSPI=LGFXBase │
        │  RTC_*TypeDef + compatRtc{Set,GetTime,GetDate}        │
        │  #if BOARD_STICKS3 ─ M5.Power / M5.Display / no-op LED│
        │  #else            ─ M5.Axp / GPIO10 LED (native)      │
        └───────────────┬──────────────────────┬───────────────┘
                        ▼                      ▼
                  M5Unified (M5.Power,    M5GFX (M5Canvas,
                  M5.Display, M5.Imu)     lgfx::LGFXBase)
```

### Pattern: board-conditional helper (the whole-project idiom)
```cpp
// Source: replicated from PR #48 compat.h technique
#if defined(BOARD_STICKS3)
static inline void compatScreenBreath(int v) {
  M5.Display.setBrightness((uint8_t)map(constrain(v,0,100),0,100,0,255));
}
static inline bool compatPowerBtnShort() { return M5.Power.getKeyState() == 2; }
static inline void compatRailSleep() {}            // D-04 no-op
static inline void compatRailWake()  {}            // D-04 no-op
#else
static inline void compatScreenBreath(int v) { M5.Axp.ScreenBreath(v); }
static inline bool compatPowerBtnShort() { return M5.Axp.GetBtnPress() == 0x02; }
static inline void compatRailSleep() { M5.Axp.SetSleep(); }
static inline void compatRailWake()  { M5.Axp.WakeUpDisplayAfterLightSleep(); }
#endif
```
> Note: putting `M5.Axp.*` inside the `#else` keeps those names out of the StickS3 TU
> entirely, so `M5.Axp` (which does not exist in M5Unified) is never even parsed for StickS3.

### Anti-Patterns to Avoid
- **Fake `M5.Axp`/`M5.Rtc` global objects.** Collides with M5Unified's `M5` instance and
  fights the library. Use `compat*` helper functions (PR #48 technique, D-07).
- **A `using TFT_eSPI = …` alongside surviving `class TFT_eSPI;` forward declarations.**
  Conflicting redeclaration → hard error. Remove the forward decls (Phase 3).
- **Routing the clock through `M5.Rtc`.** No-op on a board without an RTC chip; use the
  system-clock helpers.
- **Applying the host timezone offset twice.** Host already localizes; convert UTC↔UTC
  (`timegm`/`gmtime_r`), never `localtime`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Battery percentage on StickS3 | A bespoke coulomb integrator / custom LiPo curve | `M5.Power.getBatteryLevel()` | M5Unified already implements per-PMIC gauging (M5PM1 on StickS3, AXP192 on StickC+) |
| USB/charge detection | Polling raw ADC | `compatOnUsb()` (VBus + `isCharging()` fallback) | Already in PR #48 base; handles the −1/unsupported VBus case |
| Power-button decode | Raw I2C reads of the PMIC | `M5.Power.getKeyState()` | Normalizes AXP192 vs M5PM1 key state to `0/1/2/3` |
| Wall clock | Custom millis→date math | `settimeofday`/`time`/`gmtime_r` (system clock) + `_compatTimegm` | Standard libc; PR #48 already wrote the one missing piece (`timegm`) |
| Graphics surface abstraction | A custom blit layer | `M5Canvas` / `lgfx::LGFXBase` | Drawing API is already 1:1 with `TFT_eSprite`/`TFT_eSPI` |

**Key insight:** This whole phase is *re-mapping*, not *building*. The only genuinely
hand-written code is the portable `_compatTimegm` (newlib gap) — and PR #48 already wrote it.

## Common Pitfalls

### Pitfall 1: `class TFT_eSPI;` forward-decl vs the `using` alias
**What goes wrong:** `buddy.h:11` and `character.h:27` declare `class TFT_eSPI;`. With
`using TFT_eSPI = lgfx::LGFXBase;` in compat.h, the compiler sees the same name declared as
both a class and a type-alias → "conflicting declaration" / "redeclared as different kind of
symbol."
**Why:** A forward `class` declaration and a type alias are incompatible redeclarations.
**How to avoid:** Phase 3 replaces those forward decls with `#include "compat.h"` (PR #48 did
exactly this — both headers are in its changed-files list). For the **Phase-2 standalone TU**
this does not arise (compat.h alone has no such conflict), but the planner must schedule the
fix in Phase 3 or the full build (PORT-03) breaks.
**Warning sign:** error mentioning `TFT_eSPI` "previous declaration" in `buddy.h`/`character.h`.

### Pitfall 2: M5Canvas color depth / PSRAM on `createSprite`
**What goes wrong:** `spr.createSprite(135,240)` for a 16-bpp sprite = ~63 KB. LovyanGFX
sprites may default to a different color depth or allocation arena than the old `TFT_eSprite`.
**Why:** M5Canvas defaults differ from TFT_eSPI; without an explicit depth the sprite can come
up at the wrong bpp or fail to allocate in internal RAM.
**How to avoid:** In Phase 3, call `spr.setColorDepth(16)` (and rely on `BOARD_HAS_PSRAM`,
already set in Phase 1) before `createSprite`. Out of scope for the Phase-2 compile bar but
flag for the planner. **Warning sign:** garbled colors or a black screen at runtime, or
`createSprite` returning null.
**Confidence:** MEDIUM (runtime behavior — not exercised by the compile-only bar).

### Pitfall 3: Float vs int units in the power accessors
**What goes wrong:** `M5.Axp.GetBatVoltage()` returns **volts (float)**; `M5.Power.getBattery
Voltage()` returns **mV (int16_t)**. A naive 1:1 swap makes the UI read ~1000× wrong.
**How to avoid:** Wrap in `compatBatVoltage()`/`compatBatCurrent()`/`compatVBusVoltage()` that
restore the float-volts shape the call sites expect (they multiply by 1000 to get mV).
**Warning sign:** battery shows `4000%` or `0.00V`.

### Pitfall 4: `temperatureRead()` availability
**What goes wrong:** `compatChipTempC()` calls `temperatureRead()`. It exists on both ESP32 and
ESP32-S3 Arduino cores, so it compiles, but on classic ESP32 (StickC Plus) it is famously
inaccurate. D-05 keeps it discretionary and non-blocking.
**How to avoid:** Accept the base implementation; it satisfies the compile bar. Optionally
board-condition StickC Plus back to `M5.Axp.GetTempInAXP192()`.
**Confidence:** HIGH (compiles); accuracy is explicitly out of scope (D-05).

### Pitfall 5: `BOARD_STICKS3` reliability
**What goes wrong:** Board-conditional blocks silently take the wrong branch if `-DBOARD_STICKS3`
isn't defined for the StickS3 env.
**Why:** It is set only in `[env:m5stack-sticks3]` build_flags (verified present in
`platformio.ini:30`). The StickC Plus env must NOT define it (verified — it doesn't).
**How to avoid:** Nothing to do (Phase 1 set it correctly); just rely on `#if defined(BOARD_STICKS3)`.
A quick `#if !defined(BOARD_STICKS3) && !<known M5StickCPlus marker>` static-assert is optional
insurance but unnecessary.
**Warning sign:** StickS3 build pulls in `M5.Axp.*` and fails with "M5.Axp has no member".

### Pitfall 6: Include ordering / `M5Unified.h` before Arduino types
**What goes wrong:** compat.h uses `M5Canvas`, `lgfx::LGFXBase`, `temperatureRead`, `map`,
`constrain`, `pinMode`, `digitalWrite`.
**How to avoid:** compat.h already `#include <M5Unified.h>` and `#include <Arduino.h>` (and
`<sys/time.h>`/`<time.h>`) at the top — keep that. The standalone-TU bar is satisfied because
the header is self-contained. **Warning sign:** "M5Canvas not declared" → a consumer included
compat.h without M5Unified first; the header's own includes prevent this.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Board-specific `M5StickCPlus` library (`M5.Axp`, `M5.Rtc`, `M5.Lcd`, `TFT_eSPI`) | `M5Unified` (`M5.Power`, `M5.Display`, `M5.Imu`) + `M5GFX` (`M5Canvas`, `lgfx::LGFXBase`), runtime board auto-detect | M5Unified GA (well established by 2024) | One source tree builds for StickC Plus + StickS3; the shim is the only adapter needed |
| `M5.Beep` PWM tone | `M5.Speaker` (I2S/ES8311 on StickS3) | M5Unified | Phase 4 concern; eliminates the LEDC ch0 collision noted in main.cpp:31-35 |
| `ledcSetup`/`ledcAttachPin` (core ≤2.x) | `ledcAttach` (ESP32 Arduino core 3.x) | core 3.0 | Phase 4 vibration path; board/core-conditional |

**Deprecated/outdated:** Routing time through `M5.Rtc` on a chip-less board (no-op); using
`localtime`/timezone math on host-localized values (double offset).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | M5StickS3 exposes no addressable/controllable user LED | RF-02 | Low — D-06 permits no-op regardless; PR #48 + PROJECT.md corroborate |
| A2 | `M5.Power.getBatteryLevel()` returns a usable 0..100 on StickS3 (M5PM1) | Power mapping | Low — falls back to `batteryPctVoltage()`; cosmetic only |
| A3 | M5Canvas default color depth needs explicit `setColorDepth(16)` for the 135×240 sprite | Pitfall 2 | Medium — runtime visual bug if wrong; not a Phase-2 compile blocker |
| A4 | `temperatureRead()` is linkable on the espressif32 core version pinned in Phase 1 | Pitfall 4 / D-05 | Low — present on ESP32-S3; D-05 makes it non-blocking |

## Open Questions (RESOLVED)

1. **Should compat.h provide the extension power helpers in Phase 2, or only the PR #48 base?**
   - What we know: Criterion 3 only requires the compat.h-only TU to compile. The base alone
     compiles. The fork's `M5.Axp.*` call sites are Phase-3 ports.
   - What's unclear: Whether the planner wants the full `compat*` helper set written now (so
     Phase 3 is pure call-site swaps) or grown error-by-error in Phase 3.
   - **RESOLVED: Write the full helper set in Phase 2** (the mapping is fully specified
     above and the helpers compile standalone), leaving Phase 3 as mechanical call-site edits.
     This honors D-07's "extend as compile errors demand" because we already know the demands
     from the source inventory. Adopted by plan 02-01 Task 1.

2. **`compatBacklight(bool)` for `SetLDO2` — brightness-0 vs `M5.Display.sleep()`?**
   - What we know: `SetLDO2(false)` cuts the backlight rail; wake calls `applyBrightness()`.
   - **RESOLVED: `setBrightness(0)` for off** (simplest, reversible by the existing
     `applyBrightness()`); avoid `M5.Display.sleep()` which also blanks the panel state.
     Adopted by plan 02-01 Task 1.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| PlatformIO + espressif32 platform | Build | ✓ (Phase 1) | per `platformio.ini` | — |
| `m5stack/M5Unified` | compat.h | ✓ | `^0.2.0` | — |
| `m5stack/M5GFX` | compat.h | ✓ | `^0.2.0` | — |
| `-DBOARD_STICKS3` flag | board conditionals | ✓ | set in `[env:m5stack-sticks3]` | — |

No missing dependencies. This is a header-only, compile-bar phase; everything needed was put
in place by Phase 1.

## Security Domain

`security_enforcement: true`, ASVS level 1. This phase produces a single C++ compatibility
header with no network, auth, input-parsing, or crypto surface of its own.

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V5 Input Validation | no (this phase) | The BLE/JSON inbound path (`_applyJson`) is pre-existing and unchanged here; `t[0].as<uint32_t>()` bounds the time field. Any hardening is out of Phase-2 scope. |
| V6 Cryptography | no | BLE LE Secure Connections bonding lives in `ble_bridge.cpp`, untouched by the shim. |
| Others (V2/V3/V4) | no | No auth/session/access-control logic in a compat header. |

**Note:** the time-sync seam (data.h) trusts the bonded host's `{"time":…}` value. Since NUS
characteristics are encrypted-and-bonded only (`ESP_GATT_PERM_*_ENCRYPTED`), the time source is
already an authenticated peer — no new trust boundary is introduced by Phase 2.

## Sources

### Primary (HIGH confidence)
- `gh api repos/yiduo/claude-desktop-buddy/contents/src/compat.h?ref=2cb901e…` — the verbatim
  PR #48 reference compat.h (D-07 base), fetched authenticated.
- Repo source (this fork): `src/main.cpp`, `src/data.h`, `src/stats.h`, `src/xfer.h`,
  `src/buddy.{h,cpp}`, `src/character.{h,cpp}`, `src/ble_bridge.{cpp,h}`, `platformio.ini` —
  Grep/Read for the exact legacy-name inventory and the RF-01 time-sync seam.
- M5Unified `Power_Class.hpp` (github.com/m5stack/M5Unified) — `getBatteryVoltage`/`Current`/
  `getVBUSVoltage`/`isCharging`/`getBatteryLevel`/`setLed`/`powerOff`/sleep signatures.
- m5-docs M5Unified Power Class + migration notes — `getKeyState()` (0/1/2/3) and `M5.BtnPWR`
  semantics for the AXP192 power button.

### Secondary (MEDIUM confidence)
- m5-docs / community threads — `M5.BtnPWR.wasClicked()`/`wasHold()` AXP192 limitation
  (cross-checked with migration doc).
- M5Stack store + CNX Software M5StickS3 overview — ESP32-S3-PICO-1-N8R8, 1.14" display.

### Tertiary (LOW confidence)
- General ESP32-S3 onboard-LED search (no M5StickS3-specific user LED found) — corroborates
  RF-02 no-op verdict (treated as confirmation, not the basis).

## Metadata

**Confidence breakdown:**
- PR #48 compat.h base: HIGH — fetched verbatim from the PR head SHA.
- Legacy-API surface inventory: HIGH — grepped from the actual repo, with file:line.
- `M5.Power`/`M5.Display` mapping: HIGH — verified against M5Unified header + docs.
- RF-01 (host time): HIGH — the parsing code exists in `data.h`.
- RF-02 (no LED): HIGH — PR #48 reference + PROJECT.md agree.
- Runtime details (sprite color depth, temp accuracy): MEDIUM — not exercised by the
  compile-only acceptance bar; flagged for Phase 3.

**Research date:** 2026-06-28
**Valid until:** ~2026-07-28 (M5Unified 0.2.x is stable; revisit if the version pin moves)
