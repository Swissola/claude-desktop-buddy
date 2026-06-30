#pragma once
// Hardware compatibility shim.
//
// Re-creates the legacy M5StickCPlus API names (TFT_eSprite, TFT_eSPI,
// RTC_*TypeDef) and compat* helper functions on top of M5Unified + M5GFX so
// the fork's UI / render / power / RTC code compiles unchanged for BOTH the
// M5StickC Plus (AXP192 + coin-cell RTC) and the M5StickS3 (ESP32-S3, no AXP,
// no RTC chip). Every board-specific helper is gated on `#if defined(BOARD_STICKS3)`
// so the StickC Plus keeps calling its native AXP192/RTC; all `M5.Axp.*`
// references live ONLY in the non-StickS3 `#else` branch, so `M5.Axp` is never
// parsed in the StickS3 translation unit.
#include <M5Unified.h>
#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

// --- Display surfaces ---
using TFT_eSprite = M5Canvas;
using TFT_eSPI    = lgfx::LGFXBase;

// --- Legacy bare color names ---
// These are the ONLY two TFT color macros this fork uses (grep-confirmed).
#ifndef GREEN
#define GREEN 0x07E0
#endif
#ifndef RED
#define RED 0xF800
#endif

// --- Software RTC ---
// StickS3 has no RTC chip; back the wall clock with the ESP32 system clock
// (settimeofday/time/gmtime_r). The clock free-runs from boot (D-02) and is
// seeded from the bonded host's {"time":[epoch,tz]} message (D-01) via
// compatRtcSet. UTC both directions (timegm/gmtime_r) so the host-localized
// value is never offset twice.
struct RTC_TimeTypeDef { uint8_t Hours, Minutes, Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay, Month, Date; uint16_t Year; };

// Portable UTC broken-down-time -> epoch (newlib lacks timegm).
static inline time_t _compatTimegm(const struct tm* t) {
  static const int mdays[] = { 0,31,59,90,120,151,181,212,243,273,304,334 };
  // Clamp month to [0,11]: a stray tm_mon (e.g. host Month==0 -> -1) must never
  // index mdays out of bounds (negative C-modulo would be UB). WR-01.
  int mon = t->tm_mon < 0 ? 0 : (t->tm_mon > 11 ? 11 : t->tm_mon);
  long y = t->tm_year + 1900;
  long days = (y-1970)*365 + (y-1969)/4 - (y-1901)/100 + (y-1601)/400;
  days += mdays[mon];
  if (mon > 1 && ((y%4==0 && y%100!=0) || y%400==0)) days += 1;
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

// --- USB / charging sense ---
static inline bool compatOnUsb() {
  int v = M5.Power.getVBUSVoltage();
  if (v > 0) return v > 4000;
  return (int)M5.Power.isCharging() != 0;
}

// --- Onboard LED ---
// StickC Plus: red LED on GPIO10, active-low. StickS3 has no user LED (GPIO10
// is Grove Port-A there), so the LED calls compile to no-ops (RF-02 / D-06).
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

// --- Chip temperature (D-05) ---
// ESP32-S3 internal sensor on StickS3; temperatureRead() also links on classic
// ESP32 (StickC Plus) though it is less accurate there. Accuracy is out of scope.
static inline int compatChipTempC() { return (int)temperatureRead(); }

// --- Power / battery / brightness (RF-04) ---
// Phase 1 removed M5StickCPlus; M5.Axp no longer exists on EITHER board.
// M5.Power wraps the AXP192 (StickC Plus) and the S3 gauge, so these helpers
// are board-AGNOSTIC. Only rail-cut sleep + the onboard LED stay board-specific.
// Pitfall 3: M5.Power.getBatteryVoltage()/getVBUSVoltage() return int mV; the
// legacy call sites multiply by 1000 to recover mV — so divide by 1000.0f here.
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

// --- CPU-frequency throttle (idle power saving) — board-specific ---
// On the StickS3 (ESP32-S3) dropping the CPU clock to 40MHz while the BLE
// controller is ACTIVE starves the APB clock the radio depends on and RESETS
// the chip. Symptom: a ~15s reboot loop on battery — the SCREEN_OFF_MS idle
// throttle (main.cpp) fires only off-USB (keepAwakeOnUsb=_onUsb is true on USB,
// so the throttle is skipped there → stable on USB, reboots on battery). The
// reboot loop also keeps the device at full clock + display + radio the whole
// time → it runs warm. Lowering frequency cannot cause a brownout (it reduces
// current), so this is the APB-starvation reset, not a power dropout.
// The deep-idle power win comes from screen-off backlight + esp_light_sleep_start
// (which halts the CPU entirely, making the pre-sleep clock irrelevant), NOT from
// this throttle — so skipping it on the S3 costs almost nothing and removes the
// reset. The chip simply stays at its 240MHz boot frequency. StickC Plus (classic
// ESP32, AXP, field-proven) keeps the native throttle unchanged.
// See debug/sticks3-battery-reboot.
#if defined(BOARD_STICKS3)
static inline void compatSetCpuMhz(uint32_t) {}
#else
static inline void compatSetCpuMhz(uint32_t mhz) { setCpuFrequencyMhz(mhz); }
#endif

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
  M5.Power.Axp192.setLDO3(3000);  // restore panel logic ~3.0V [ASSUMED A1 — confirm on hardware]
  M5.Power.Axp192.setLDO2(3000);  // restore backlight rail; caller re-applies brightness [ASSUMED A1 — confirm on hardware]
}
#endif

// --- UI beep (D-10): M5.Beep -> M5.Speaker ---
// M5Unified has no M5.Beep. Speaker must be enabled at M5.begin (D-09).
// Works on both boards (StickC Plus buzzer + StickS3 ES8311/AW8737).
// M5.Speaker.tone signature [VERIFIED: Speaker_Class.hpp:165]:
//   bool tone(float frequency, uint32_t duration = UINT32_MAX, ...)
static inline void compatBeep(uint16_t freq, uint16_t dur) {
  M5.Speaker.tone((float)freq, (uint32_t)dur);
}

// --- Button GPIOs (were M5StickCPlus.h macros; M5Unified does not define them) ---
// Used by main.cpp for light-sleep GPIO wake (main.cpp:279-280).
#if defined(BOARD_STICKS3)
#ifndef BUTTON_A_PIN
#define BUTTON_A_PIN 11   // StickS3 BtnA (GPIO11) [VERIFIED O1-RESOLVED: M5Unified.cpp board_M5StickS3 gpio_in(GPIO_NUM_11)]
#endif
#ifndef BUTTON_B_PIN
#define BUTTON_B_PIN 12   // StickS3 BtnB (GPIO12) [VERIFIED O1-RESOLVED: M5Unified.cpp board_M5StickS3 gpio_in(GPIO_NUM_12)]
#endif
#else
#ifndef BUTTON_A_PIN
#define BUTTON_A_PIN 37   // StickC Plus BtnA (GPIO37) [VERIFIED: M5Unified.cpp GPIO_NUM_37]
#endif
#ifndef BUTTON_B_PIN
#define BUTTON_B_PIN 39   // StickC Plus BtnB (GPIO39) [VERIFIED: M5Unified.cpp GPIO_NUM_39]
#endif
#endif

// --- Text-datum insurance (RF-05) ---
// M5GFX provides these via lgfx compatibility enum [MEDIUM confidence: LGFXBase.hpp].
// Pre-empt undeclared-identifier errors in Wave-3 build (MC_DATUM, TL_DATUM at
// main.cpp:700,710,728,733,1253,1267). These are no-ops if already defined by M5GFX.
#ifndef MC_DATUM
#define MC_DATUM middle_center
#endif
#ifndef TL_DATUM
#define TL_DATUM top_left
#endif
