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

// --- Power / battery / brightness helpers (board-conditional) ---
// D-03: map AXP calls with a real M5Unified equivalent onto M5.Power / M5.Display.
// D-04: stub AXP-only calls (coulomb, rail sleep/wake) to safe no-ops on StickS3.
// Pitfall 3: M5.Power.getBatteryVoltage()/getVBUSVoltage() return int mV; the
// legacy M5.Axp.GetBatVoltage()/GetVBusVoltage() returned float volts, and the
// call sites multiply by 1000 to recover mV — so divide by 1000.0f here.
#if defined(BOARD_STICKS3)
static inline float compatBatVoltage()  { return M5.Power.getBatteryVoltage() / 1000.0f; }
static inline float compatBatCurrent()  { return (float)M5.Power.getBatteryCurrent(); }
static inline float compatVBusVoltage() { return M5.Power.getVBUSVoltage() / 1000.0f; } // -1mV if unsupported -> negative volts
static inline int   compatBatteryPct()  { return M5.Power.getBatteryLevel(); }           // D-04: away from AXP coulomb path
static inline void  compatScreenBreath(int v) {
  M5.Display.setBrightness((uint8_t)map(constrain(v, 0, 100), 0, 100, 0, 255));
}
static inline void  compatBacklight(bool on) {
  // No AXP LDO2 rail on StickS3; emulate via brightness. Off = setBrightness(0)
  // (reversible by the existing applyBrightness path); avoid M5.Display.sleep().
  M5.Display.setBrightness(on ? 255 : 0);
}
static inline bool  compatPowerBtnShort() { return M5.Power.getKeyState() == 2; }         // 0=none,1=long,2=short,3=both
static inline void  compatPowerOff()      { M5.Power.powerOff(); }
// D-04 safe stubs (no coulomb counter / no AXP rails on StickS3)
static inline void  compatEnableCoulomb() {}
static inline void  compatRailSleep()     {}
static inline void  compatRailWake()      {}
#else
static inline float compatBatVoltage()  { return M5.Axp.GetBatVoltage(); }
static inline float compatBatCurrent()  { return M5.Axp.GetBatCurrent(); }
static inline float compatVBusVoltage() { return M5.Axp.GetVBusVoltage(); }
static inline int   compatBatteryPct()  { return M5.Power.getBatteryLevel(); } // keep off the coulomb path here; compiles on both
static inline void  compatScreenBreath(int v) { M5.Axp.ScreenBreath(v); }
static inline void  compatBacklight(bool on)  { M5.Axp.SetLDO2(on); }
static inline bool  compatPowerBtnShort() { return M5.Axp.GetBtnPress() == 0x02; }
static inline void  compatPowerOff()      { M5.Axp.PowerOff(); }
static inline void  compatEnableCoulomb() { M5.Axp.EnableCoulombcounter(); }
static inline void  compatRailSleep()     { M5.Axp.SetSleep(); }
static inline void  compatRailWake()      { M5.Axp.WakeUpDisplayAfterLightSleep(); }
#endif
