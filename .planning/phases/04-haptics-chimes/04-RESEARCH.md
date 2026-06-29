# Phase 4: Haptics → Chimes - Research

**Researched:** 2026-06-29
**Domain:** Embedded C++ (Arduino/PlatformIO) — board-conditional ES8311 chime output via M5.Speaker on the StickS3, preserving the StickC Plus LEDC motor path
**Confidence:** HIGH (all M5.Speaker/Speaker_Class claims read from the installed library source at `.pio/libdeps/m5stack-sticks3/M5Unified/src/utility/Speaker_Class.hpp` and `Speaker_Class.cpp`)

## Summary

Phase 4 makes the five haptic events (approve / deny / attention / celebrate / connect) audible on the StickS3 via `M5.Speaker` (ES8311 I2S), while the StickC Plus LEDC motor path stays byte-for-byte unchanged. The existing non-blocking pattern state machine (`vibrateTick` / `vibratePatternAmp*`) is kept as the sequencer — only its output backend forks per board via `#if defined(BOARD_STICKS3)`. Each event's existing timing pattern (pulse durations and rest gaps) becomes the rhythm skeleton; per-ON-step frequency arrays carry the chiptune pitches.

The most important finding from reading the installed library source: **M5.Speaker's default `tone()` uses a sine wave, not a square wave.** The "Retro / chiptune" character requires the 6-argument overload of `tone()` with a custom square-wave buffer. The 4-argument `compatBeep` shim already in `compat.h` produces sine-wave beeps, which is fine for UI feedback; the chime path must pass its own waveform data.

M5.Speaker queues playback asynchronously on virtual channels (0–7). There is no per-loop pump needed (Phase 3 already confirmed this). A dedicated channel (`CHIME_CH = 0`) for event chimes and auto-select (`channel = -1`) for the UI beep allows the D-06 precedence rule to be implemented purely in the `beep()` helper — suppress beep when `_vibPat != nullptr` on the StickS3 — without any speaker-level channel management.

**Primary recommendation:** Add a 16-sample square-wave buffer and per-event frequency tables to `src/main.cpp`; gate the `vibrateTick` output backend with `#if defined(BOARD_STICKS3)` (tone path) vs. `#else` (unchanged ledcWrite path); add one check in `beep()` and one board-conditional in `settingsItems[]`. No new files, no new libraries, no changes to `compat.h`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Chime output is StickS3-only. The StickC Plus keeps its vibration motor; no chime/theme UI appears on the StickC Plus.
- **D-02:** Ship one chime voice this phase — the Retro / chiptune set (square-wave tones, playful pixel-buddy/Tamagotchi energy). Chosen over warm, minimal, and bright as the starting voice.
- **D-03:** All five events must be audibly distinct and recognisable, and approve must differ clearly from deny (today they're the same motor blip). Intended shapes: approve = bright rising cue; deny = lower/descending cue; attention = soft-soft-accent rising alert; celebrate = upbeat multi-note flourish; connect = clean two-note "linked up". Keep them short and non-annoying for a desk companion.
- **D-04:** Write the chime engine theme-ready — structure the tone tables so additional voices can be added later without re-architecting — even though only the Retro set ships now.

### Claude's Discretion (user delegated — flagged for review, overridable)
- **D-05 (pattern reuse):** Reuse each event's existing rhythm/shape from the current vibration patterns (approve/deny blip, attention low-low-HIGH, celebrate HIGH-low-HIGH-low siren, connect two-pulse) as the structural skeleton, rendered as chiptune tones with distinct per-event pitches — i.e. preserve the event "rhythm" identity, author tonal character on top. Not a blind buzz-to-beep amplitude translation.
- **D-06 (beep vs chime overlap, HAPT-03):** Keep `settings().sound` (UI button beep) and the event-chime feedback as separate toggles, but give event chimes precedence — when an event chime plays, suppress the simultaneous UI beep so the two cues don't stack/clash. UI beep still fires for plain button feedback when no event chime is active.
- **D-07 (settings label, HAPT-03):** Board-conditional label — the feedback on/off entry reads "chime" on the StickS3 (it chimes) and stays "vibrate" on the StickC Plus (it really vibrates). No theme-picker settings entry this phase (single voice).

### Deferred Ideas (OUT OF SCOPE)
- Selectable chime-theme picker (StickS3 settings entry to choose the voice) — user wants it eventually; deferred until there's more than one voice. Build the engine theme-ready (D-04) so it slots in cleanly.
- Additional chime voices: Warm & musical, Minimal & subtle, Bright & arcade-y — the other three personalities from discussion. Record and add after the Retro set ships.
- BUZZ-01 (Grove Vibrator Unit, Port.A G9) — v2-deferred; do NOT add a configurability seam for it in this phase.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| HAPT-01 | On the StickS3 (`#if defined(BOARD_STICKS3)`), the haptic pattern engine (`vibratePatternAmp*`) drives `M5.Speaker` tones; each event (approve/deny/attention/celebrate/connect) has a distinct, recognisable chime | Verified M5.Speaker tone API; concrete per-event frequency tables and square-wave buffer provided below |
| HAPT-02 | On the StickC Plus, the existing LEDC vibration-motor path is preserved unchanged | Board-conditional seam: the `#else` blocks in `vibrateTick` / setup contain the original `ledcWrite` calls byte-for-byte |
| HAPT-03 | The "vibrate" settings entry is relabelled to "chime"/"haptics", and the sound-vs-chime overlap (UI beep vs event tone) is resolved so cues don't clash | Settings label: board-conditional array element; overlap: `_vibPat` guard in `beep()` on StickS3 |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Event chime output (StickS3) | `main.cpp` → `M5.Speaker.tone()` via I2S/ES8311 | — | ES8311 I2S path; no LEDC involvement; already initialized at M5.begin |
| Vibration motor output (StickC Plus) | `main.cpp` → `ledcWrite(VIBRATE_PIN, duty)` | — | Unchanged LEDC path; core 3.x pin-based; no channel numbers |
| Sequencer / timing | `vibrateTick` pattern state machine in `main.cpp` | — | Non-blocking millis-based tick; kept as-is; only output backend forks |
| Chime vs beep precedence | `beep()` in `main.cpp` (D-06 guard) | — | Both drive M5.Speaker on StickS3; guard in the caller, not the speaker |
| Settings UI label | `settingsItems[]` in `main.cpp` (board-conditional) | — | Index 2 reads "chime" on StickS3 / "vibrate" on StickC Plus |
| Speaker init | `M5.begin(cfg)` in setup() (D-09, Phase 3) | — | Already done; no additional init for Phase 4 |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `m5stack/M5Unified` | 0.2.17 (installed) | `M5.Speaker` tone API | Already on both envs; provides I2S/ES8311 playback for StickS3 and buzzer path for StickC Plus |

No new libraries. Phase 4 installs nothing; `M5Unified 0.2.17` is already locked in `platformio.ini`.

## Package Legitimacy Audit

Not applicable — Phase 4 installs **no** external packages. All dependencies were vetted and locked in Phase 1.

---

## M5.Speaker API (VERIFIED from installed Speaker_Class.hpp / .cpp)

### Key signatures

```cpp
// [VERIFIED: Speaker_Class.hpp:165]
// 4-arg overload — uses the DEFAULT SINE WAVE tone.
bool tone(float frequency, uint32_t duration = UINT32_MAX, int channel = -1, bool stop_current_sound = true);

// [VERIFIED: Speaker_Class.hpp:156]
// 6-arg overload — uses CUSTOM WAVEFORM raw_data (unsigned 8-bit).
// This is the overload needed for chiptune square-wave character.
bool tone(float frequency, uint32_t duration, int channel, bool stop_current_sound,
          const uint8_t* raw_data, size_t array_len, bool stereo = false);

// [VERIFIED: Speaker_Class.hpp:121]
void setVolume(uint8_t master_volume);   // 0..255; default = 64
uint8_t getVolume(void) const;

// [VERIFIED: Speaker_Class.hpp:108]
bool isPlaying(void) const volatile;                         // any channel?
size_t isPlaying(uint8_t channel) const volatile;           // 0=idle, 1=playing (room), 2=full queue

// [VERIFIED: Speaker_Class.hpp:142/146]
void stop(void);               // stop all channels
void stop(uint8_t channel);    // stop one channel

// [VERIFIED: Speaker_Class.hpp:238]
static constexpr const size_t sound_channel_max = 8;   // 8 virtual channels
```

### Default tone waveform is SINE — critical finding

```cpp
// [VERIFIED: Speaker_Class.cpp:73]
const uint8_t Speaker_Class::_default_tone_wav[16] =
  { 177, 219, 246, 255, 246, 219, 177, 128, 79, 37, 10, 1, 10, 37, 79, 128 }; // sin wave data
```

**The 4-arg `tone()` produces a SINE wave, not a square wave.** `compatBeep` (already using `M5.Speaker.tone(freq, dur)`) produces sine-wave UI beeps — correct for a soft button feedback sound. For the chiptune square-wave character required by D-02, the chime path MUST use the 6-arg overload with a custom square-wave buffer.

### 6-arg tone internal math
```cpp
// duration != UINT32_MAX case:
// sample_rate = frequency * array_len
// repeat_count = (uint32_t)(duration_ms * frequency / 1000)
// Example: freq=659Hz, array_len=16, dur=60ms
//   sample_rate = 659 * 16 = 10544 Hz
//   repeat_count = 60 * 659 / 1000 = 39 periods ≈ 59ms  (≈ target)
```

The math rounds down slightly, which is acceptable; the discrepancy for any of these short tones (60–160ms) is well under 16ms.

### Channel model
- 8 independent virtual channels (0–7). Multiple channels mix in the I2S output stream.
- `channel = -1` auto-selects an idle channel.
- `stop_current_sound = true` (default) cuts any in-progress sound on the chosen channel before starting the new one.
- Each channel has a two-slot wavinfo flip buffer: one playing, one queued. `isPlaying(ch)` returns 2 when the queue slot is also occupied.

### Volume default
Default `_master_volume = 64` (~25%). The ES8311 on the StickS3 will be noticeably quiet at this level for a desk companion. Calling `M5.Speaker.setVolume(128)` inside `#if defined(BOARD_STICKS3)` in `setup()` is recommended (see Open Questions). This is not a new user-visible setting — just a sensible hardware default.

---

## Chime Engine Design (HAPT-01)

### Square-wave buffer (chiptune character)

```cpp
// 16-sample 50%-duty square wave, unsigned 8-bit.
// Use with the 6-arg tone() for hard square-wave character.
// The default _default_tone_wav is a sine; this replaces it for the Retro voice.
// [VERIFIED: sine default confirmed at Speaker_Class.cpp:73; this buffer is the
//  explicit square-wave substitute for chiptune output]
static const uint8_t CHIME_WAV[] = {
    0,   0,   0,   0,   0,   0,   0,   0,
  255, 255, 255, 255, 255, 255, 255, 255
};

// Dedicated speaker channel for event chimes (0 of 0–7).
// UI beep uses channel = -1 (auto-select), landing on channels 1–7.
// Separation prevents the beep from cutting off an in-progress chime.
static constexpr uint8_t CHIME_CH = 0;
```

### Retro / chiptune frequency tables (D-02 / D-03 / D-04)

These are the Retro voice values. Theme-ready layout: a future voice adds parallel constants/arrays; a theme-picker (deferred D-04) selects which set is active.

```cpp
// ---- Retro / chiptune chime voice (Phase 4 — the only voice) ----
// Frequencies in Hz. Musical justification in the pitfall notes.

// approve — single blip, bright/positive (E5 = 659 Hz)
// deny    — single blip, dark/low (A3 = 220 Hz)
// These MUST be audibly distinct (D-03). E5 vs A3 is ~3 octaves apart.
static const float CHIME_APPROVE_FREQ  = 659.0f;   // E5  – bright, "yes"
static const float CHIME_DENY_FREQ     = 220.0f;   // A3  – dark low, "no"

// attention — soft-soft-accent rising (E4, E4, G5) — 3 ON steps
// Mirrors the existing low-low-HIGH amplitude shape (PAT_ATTENTION_AMP={115,115,255}).
// Same pitch for steps 0–1 keeps it "soft"; G5 is a ~2.4× frequency jump = accent.
static const float CHIME_ATTN_FREQS[]  = { 330.0f, 330.0f, 784.0f };  // E4, E4, G5

// celebrate — ascending C major arpeggio (C5→E5→G5→C6) — 4 ON steps
// Reuses the 4-pulse HIGH-low siren rhythm; replaces alternating amp with
// ascending pitch. C5→C6 is universally readable as "win/level-up".
static const float CHIME_CELEB_FREQS[] = { 523.0f, 659.0f, 784.0f, 1047.0f }; // C5, E5, G5, C6

// connect — rising perfect fourth G4→C5 — 2 ON steps (90ms + 160ms)
// "Ding-dong" interval. Longer second note (160ms) settles = "linked up" feel.
static const float CHIME_CONN_FREQS[]  = { 392.0f, 523.0f };  // G4, C5
```

**Why these frequencies work together (D-03 distinction):**
- approve (E5 = 659 Hz) vs deny (A3 = 220 Hz): ~3 octaves, unmistakably different, no listener can confuse them
- attention (E4-E4-G5): low-low-HIGH mirrors the motor AMP pattern; the HIGH G5 = 784 Hz is ~2× the E4 330 Hz — dramatic but short
- celebrate (C5-E5-G5-C6): the C major ascending arpeggio is one of the most universally recognized "win" cues in games; unmistakable
- connect (G4-C5): the perfect fourth is a classic doorbell/notification interval; two-note = brief, not intrusive

### Per-event frequency dispatch (StickS3 only)

The five call sites that trigger haptics (`vibratePatternAmp(PAT_APPROVE, ...)`, etc.) stay UNCHANGED (per CONTEXT: call sites stay the same). The frequency data is wired inside the two `vibratePattern*` functions via a pattern-pointer-keyed helper:

```cpp
#if defined(BOARD_STICKS3)
static float        _vibFreq    = 440.0f;       // flat frequency (approve, deny)
static const float* _vibFreqArr = nullptr;       // per-ON-step freq array (attn, celeb, conn)

// Map each pattern pointer → its chime frequency data.
// Pointer equality is reliable: PAT_* are file-scope static arrays (unique addresses).
static void _chimeSetFreqs(const uint16_t* pat) {
  _vibFreqArr = nullptr;
  if      (pat == PAT_APPROVE)    { _vibFreq = CHIME_APPROVE_FREQ; }
  else if (pat == PAT_DENY)       { _vibFreq = CHIME_DENY_FREQ; }
  else if (pat == PAT_CONNECT)    { _vibFreqArr = CHIME_CONN_FREQS; }
  else if (pat == PAT_ATTENTION)  { _vibFreqArr = CHIME_ATTN_FREQS; }
  else if (pat == PAT_CELEBRATE)  { _vibFreqArr = CHIME_CELEB_FREQS; }
  else                            { _vibFreq = 440.0f; }   // fallback
}
#endif
```

### Modified vibratePatternAmp / vibratePatternAmpArr (HAPT-01 / HAPT-02)

Add one `#if BOARD_STICKS3` call at the end of each, before `return`:

```cpp
static void vibratePatternAmp(const uint16_t* pat, uint8_t amp) {
  if (!settings().vibrate) return;
  if (_vibPat) return;
  _vibPat    = pat;
  _vibAmpArr = nullptr;
  _vibStep   = 0;
  _vibAmp    = amp;
  _vibNext   = millis();
#if defined(BOARD_STICKS3)
  _chimeSetFreqs(pat);
#endif
}

static void vibratePatternAmpArr(const uint16_t* pat, const uint8_t* ampArr) {
  if (!settings().vibrate) return;
  if (_vibPat) return;
  _vibPat    = pat;
  _vibAmpArr = ampArr;
  _vibStep   = 0;
  _vibAmp    = VIB_FULL;
  _vibNext   = millis();
#if defined(BOARD_STICKS3)
  _chimeSetFreqs(pat);
#endif
}
```

The StickC Plus compiled output is byte-for-byte the original (the `#if` block is stripped).

### Modified vibrateTick (the minimal board-conditional seam)

```cpp
static void vibrateTick(uint32_t now) {
  if (!_vibPat) return;
  if ((int32_t)(now - _vibNext) < 0) return;

  if (_vibPat[_vibStep] == 0) {
    // Pattern end: stop output, clear state.
#if defined(BOARD_STICKS3)
    M5.Speaker.stop(CHIME_CH);
#else
    ledcWrite(VIBRATE_PIN, 0);
#endif
    _vibPat    = nullptr;
    _vibAmpArr = nullptr;
    return;
  }

  if (_vibStep % 2 == 0) {   // ON step
#if defined(BOARD_STICKS3)
    float freq = _vibFreqArr ? _vibFreqArr[_vibStep / 2] : _vibFreq;
    M5.Speaker.tone(freq, _vibPat[_vibStep], CHIME_CH, true, CHIME_WAV, sizeof(CHIME_WAV));
#else
    uint8_t amp = _vibAmpArr ? _vibAmpArr[_vibStep / 2] : _vibAmp;
    ledcWrite(VIBRATE_PIN, amp);
#endif
  } else {                    // OFF step
#if defined(BOARD_STICKS3)
    // Tone from the previous ON step has already expired (its duration
    // matched the step time and _vibNext gated us). Stop for explicit silence.
    M5.Speaker.stop(CHIME_CH);
#else
    ledcWrite(VIBRATE_PIN, 0);
#endif
  }

  _vibNext = now + _vibPat[_vibStep++];
}
```

**Why this is safe:**
- `tone(freq, dur_ms, CHIME_CH, true, CHIME_WAV, 16)` — the `stop_current_sound = true` flag cuts any lingering previous tone on CHIME_CH before starting the new one
- `dur_ms = _vibPat[_vibStep]` — tone duration equals the ON-step duration; when `vibrateTick` next fires (after `_vibNext` elapses), the tone has already expired
- `M5.Speaker.stop(CHIME_CH)` on OFF steps makes silence explicit; no timing dependency
- StickC Plus `#else` path = original `ledcWrite` calls, unchanged

---

## D-06 UI-Beep vs Chime Coexistence (HAPT-03)

Both the UI beep and event chimes drive `M5.Speaker` on the StickS3. To enforce event-chime precedence (D-06), add a single check in the `beep()` helper:

```cpp
static void beep(uint16_t freq, uint16_t dur) {
  if (settings().sound) {
#if defined(BOARD_STICKS3)
    if (!_vibPat) compatBeep(freq, dur);   // chime active → suppress UI beep
#else
    compatBeep(freq, dur);
#endif
  }
}
```

**Why this works:**
- `_vibPat != nullptr` means the chime sequencer is active (between the first ON step and the terminal 0 step)
- `_vibPat` is cleared to `nullptr` immediately when the pattern ends (`_vibPat[_vibStep] == 0` branch in `vibrateTick`)
- After the chime finishes, `_vibPat == nullptr` and the next button press fires the UI beep normally
- The check is O(1), pure pointer comparison, zero overhead

**Channel coexistence:** `compatBeep` uses `channel = -1` (auto-select), which M5.Speaker resolves to any idle channel 1–7. `CHIME_CH = 0` is dedicated to chimes. They are physically separate virtual channels; M5.Speaker will MIX them in the I2S output if both were playing simultaneously. The `!_vibPat` guard in `beep()` prevents this mixing scenario from occurring in practice, satisfying D-06.

---

## D-07 Settings Label Relabel (HAPT-03)

Current declaration (main.cpp ~line 319):
```cpp
const char* settingsItems[] = { "brightness", "sound", "vibrate", "bluetooth", ... };
```

Board-conditional element at index 2:
```cpp
const char* settingsItems[] = {
  "brightness", "sound",
#if defined(BOARD_STICKS3)
  "chime",
#else
  "vibrate",
#endif
  "bluetooth", "wifi", "led", "transcript", "clock rot", "wrist", "12hr", "sleep", "pet", "reset", "back"
};
```

The `applySetting` switch `case 2: s.vibrate = !s.vibrate;` stays identical. The `settings().vibrate` field name is internal; only the displayed label changes. `SETTINGS_N = 14` and all indices remain the same on both boards.

---

## LEDC Initialization in setup() (HAPT-02)

The current `setup()` code (main.cpp ~line 1232):
```cpp
ledcAttach(VIBRATE_PIN, 500, 8);  // core-3.x pin-based API
ledcWrite(VIBRATE_PIN, 0);        // off
```

On the StickS3, GPIO26 has no motor connected. Attaching LEDC to an unconnected pin is harmless, but it is unnecessary work. Guarding it is optional but clean:

```cpp
#if !defined(BOARD_STICKS3)
  ledcAttach(VIBRATE_PIN, 500, 8);
  ledcWrite(VIBRATE_PIN, 0);
#endif
```

The StickC Plus path is unchanged inside the `#if !BOARD_STICKS3` block. HAPT-02 compliance is the same either way; the guard is a hygiene improvement. Planner's choice.

---

## Architecture Patterns

### System Architecture Diagram

```
Loop tick (16ms)
    │
    ▼
vibrateTick(now)
    │
    ├─── _vibPat == nullptr? → return (idle)
    ├─── time not yet? → return (waiting)
    ├─── pat[step] == 0? → ─────────────────────────────────────────┐
    │                                                                ▼
    │                                              BOARD_STICKS3: M5.Speaker.stop(CH0)
    │                                              else: ledcWrite(GPIO26, 0)
    │                                              _vibPat = nullptr
    │
    ├─── step even (ON step)?
    │       │
    │       ├── BOARD_STICKS3: freq = freqArr[step/2] or _vibFreq
    │       │                  M5.Speaker.tone(freq, dur_ms, CH0, true, CHIME_WAV, 16)
    │       │                  → I2S DMA → ES8311 DAC → speaker
    │       │
    │       └── else: amp = ampArr[step/2] or _vibAmp
    │                 ledcWrite(GPIO26, amp)
    │                 → LEDC PWM → ERM motor
    │
    └─── step odd (OFF step)?
            ├── BOARD_STICKS3: M5.Speaker.stop(CH0)
            └── else: ledcWrite(GPIO26, 0)
```

```
Event triggers (5 call sites, UNCHANGED):
  vibratePatternAmp(PAT_APPROVE, VIB_APPROVE_AMP)    → _chimeSetFreqs(PAT_APPROVE) [S3 only]
  vibratePatternAmp(PAT_DENY, VIB_DENY_AMP)          → _chimeSetFreqs(PAT_DENY)    [S3 only]
  vibratePatternAmpArr(PAT_ATTENTION, PAT_ATTN_AMP)  → _chimeSetFreqs(PAT_ATTENTION) [S3 only]
  vibratePatternAmpArr(PAT_CELEBRATE, PAT_CELEB_AMP) → _chimeSetFreqs(PAT_CELEBRATE) [S3 only]
  vibratePatternAmp(PAT_CONNECT, 150)                → _chimeSetFreqs(PAT_CONNECT)   [S3 only]
```

```
beep() path (UI buttons):
  settings().sound = true?
    BOARD_STICKS3: _vibPat == nullptr? → M5.Speaker.tone(freq, dur) [sine, ch=-1]
                   _vibPat != nullptr? → (suppressed, D-06)
    else: M5.Speaker.tone(freq, dur) [sine, ch=-1] — unchanged
```

### Recommended Code Organization

All changes are confined to `src/main.cpp`. Suggested block order within the haptic section (lines ~31–112):

```
[line ~31] const int VIBRATE_PIN = 26;  — unchanged
[line ~34] const int VIBRATE_CH  = 2;   — unchanged (unused, harmless)

[NEW] #if defined(BOARD_STICKS3)
[NEW] // ---- Chime constants (StickS3 / ES8311) ----------------------------
[NEW] static const uint8_t CHIME_WAV[] = {0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255};
[NEW] static constexpr uint8_t CHIME_CH = 0;
[NEW] // ---- Retro voice frequency tables -----------------------------------
[NEW] static const float CHIME_APPROVE_FREQ  = 659.0f;
[NEW] static const float CHIME_DENY_FREQ     = 220.0f;
[NEW] static const float CHIME_ATTN_FREQS[]  = {330.0f, 330.0f, 784.0f};
[NEW] static const float CHIME_CELEB_FREQS[] = {523.0f, 659.0f, 784.0f, 1047.0f};
[NEW] static const float CHIME_CONN_FREQS[]  = {392.0f, 523.0f};
[NEW] #endif

[lines ~47-53] PAT_* arrays — unchanged

[lines ~55-67] _vibPat / _vibAmpArr / _vibStep / _vibNext / _vibAmp state — unchanged

[NEW] #if defined(BOARD_STICKS3)
[NEW] static float        _vibFreq    = 440.0f;
[NEW] static const float* _vibFreqArr = nullptr;
[NEW] #endif

[lines ~65-67] VIB_FULL / VIB_APPROVE_AMP / VIB_DENY_AMP — unchanged

[lines ~74-94] vibratePatternAmp / vibratePatternAmpArr — add _chimeSetFreqs call

[NEW] #if defined(BOARD_STICKS3)
[NEW] static void _chimeSetFreqs(const uint16_t* pat) { ... }
[NEW] #endif

[lines ~97-112] vibrateTick — board-conditional output
```

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Chiptune square wave | Custom I2S driver / DAC manipulation | `M5.Speaker.tone(freq, dur, ch, stop, CHIME_WAV, 16)` — the 6-arg overload | The library handles I2S, DMA, ES8311 codec programming, resampling from tone-freq → 48kHz I2S; verified working for stickS3 |
| Sequential tone scheduling | Per-note delay loops or FreeRTOS task | The existing `vibrateTick` pattern state machine | Already handles multi-step sequences non-blocking; tested in production |
| Chime channel management | Manual I2S channel setup | `CHIME_CH = 0` as a fixed virtual channel; auto-channel (-1) for beep | M5.Speaker's 8-channel mixer handles mixing/muting transparently |
| UI beep suppression during chimes | Complex interrupt/callback system | `if (!_vibPat) compatBeep(...)` in `beep()` | `_vibPat` is the authoritative "chime active" flag; zero overhead |
| Square-wave frequency synthesis | Manually computing sample buffers per frequency | Pass constant `CHIME_WAV[16]` with the desired frequency as `tone(freq, ...)` | The library scales sample rate to produce the correct frequency via `sample_rate = freq * array_len` |

---

## Common Pitfalls

### Pitfall 1: Using `tone(freq, dur)` for chimes — produces sine wave, not square wave
**What goes wrong:** `M5.Speaker.tone(freq, dur)` (4-arg) uses `_default_tone_wav` which is a **sine wave** (`{177, 219, 246, 255, ...}` confirmed in Speaker_Class.cpp:73). The chime sounds smooth/soft, not chiptune/square.
**Why it happens:** The overload name `tone` implies a simple beeper but the implementation interpolates any waveform through I2S; the default happens to be sine.
**How to avoid:** Always use the **6-arg overload** for chimes: `M5.Speaker.tone(freq, dur, CHIME_CH, true, CHIME_WAV, sizeof(CHIME_WAV))`. Reserve the 4-arg version for `compatBeep` (UI beep, fine as sine).
**Warning sign:** Chimes sound "warm" or "electronic" without the hard-edged digital bite; check which overload was used.

### Pitfall 2: Placing `_vibFreq` / `_vibFreqArr` in the wrong scope
**What goes wrong:** If the new StickS3 state variables are declared inside `#if defined(BOARD_STICKS3)` but accessed from `vibrateTick` (which is shared-source), the StickC Plus build sees undeclared identifiers.
**Why it happens:** The StickS3-gated declaration is invisible to the StickC Plus compilation unit.
**How to avoid:** The `_vibFreq` / `_vibFreqArr` accesses in `vibrateTick` must also be inside `#if defined(BOARD_STICKS3)` guards. The `_chimeSetFreqs` helper is `#if BOARD_STICKS3` only. Verify the StickC Plus build after adding the new variables.

### Pitfall 3: `CHIME_WAV` / `CHIME_CH` declared inside `#if BOARD_STICKS3` but referenced in a non-guarded function
**What goes wrong:** If `CHIME_WAV` is in a `#if BOARD_STICKS3` block but a `vibrateTick` `else` branch references it without a guard, the StickC Plus build fails with undeclared identifier.
**Why it happens:** The `else` (StickC Plus) path of `vibrateTick` must never reference BOARD_STICKS3-only symbols.
**How to avoid:** All CHIME_WAV / CHIME_CH / _vibFreq* references appear ONLY inside `#if defined(BOARD_STICKS3)` blocks in `vibrateTick`.

### Pitfall 4: Tone duration rounding discards a step in short chimes
**What goes wrong:** `repeat_count = (uint32_t)(dur_ms * freq / 1000)` truncates. For approve (dur=60ms, freq=659Hz): 60*659/1000 = 39.54 → 39 repeats = 59.2ms. For very short tones (20ms or less) this could round to 0 repeats and play nothing.
**Why it happens:** Integer truncation in the internal math.
**How to avoid:** The minimum ON-step duration in the existing patterns is 60ms. Even at low frequencies (A3=220Hz, deny): 60*220/1000 = 13.2 → 13 repeats = 59ms. All five events are safe. Do NOT add shorter steps without rechecking the math.

### Pitfall 5: LEDC on GPIO26 is left attached on StickS3 — potential PWM noise on startup
**What goes wrong:** `ledcAttach(VIBRATE_PIN=26, 500, 8)` without a board guard runs on the StickS3. GPIO26 is unconnected to a motor but is attached to LEDC. If GPIO26 is floating or connected to something unexpected, the PWM signal may cause noise.
**Why it happens:** The setup() LEDC init is currently unconditional.
**How to avoid:** Wrap with `#if !defined(BOARD_STICKS3)`. Not a blocking issue (GPIO26 is not routed to the ES8311 or any speaker path), but cleaner. On the ESP32-S3, GPIO26 is a regular GPIO; LEDC attachment is harmless but wasteful.

### Pitfall 6: Chime fires during `M5.Speaker.isPlaying()` check assumptions
**What goes wrong:** Checking `M5.Speaker.isPlaying(CHIME_CH)` to decide whether to suppress a UI beep — but `isPlaying` can return 0 during the OFF step (silent gap) between notes of a multi-note chime, allowing a UI beep to slip through.
**Why it happens:** The chime has explicit silent gaps (OFF steps). During those gaps the speaker channel IS idle even though the chime pattern is still "in progress."
**How to avoid:** Use `_vibPat != nullptr` (the pattern sequencer state) rather than `M5.Speaker.isPlaying(CHIME_CH)` as the chime-active test. `_vibPat` stays set throughout the entire event (including rest gaps) until the terminal zero step clears it.

### Pitfall 7: `stop_current_sound = true` in tone() cuts a UI beep mid-word
**What goes wrong:** If event chimes were on the same channel as the UI beep AND `stop_current_sound = true`, a chime would abruptly cut off an in-progress beep.
**Why it happens:** The speaker-level `stop_current_sound` applies to the targeted channel.
**How to avoid:** The design separates channels: chimes on `CHIME_CH = 0`, UI beeps on auto-select (1–7). The `!_vibPat` guard in `beep()` prevents overlap at the sequencer level, so the channel separation is a belt-and-suspenders measure.

---

## Code Examples

### Complete vibrateTick with board-conditional seam
```cpp
// Source: derived from existing vibrateTick (main.cpp:97-112) + BOARD_STICKS3 fork
static void vibrateTick(uint32_t now) {
  if (!_vibPat) return;
  if ((int32_t)(now - _vibNext) < 0) return;
  if (_vibPat[_vibStep] == 0) {
#if defined(BOARD_STICKS3)
    M5.Speaker.stop(CHIME_CH);
#else
    ledcWrite(VIBRATE_PIN, 0);
#endif
    _vibPat    = nullptr;
    _vibAmpArr = nullptr;
    return;
  }
  if (_vibStep % 2 == 0) {   // ON step
#if defined(BOARD_STICKS3)
    float freq = _vibFreqArr ? _vibFreqArr[_vibStep / 2] : _vibFreq;
    M5.Speaker.tone(freq, _vibPat[_vibStep], CHIME_CH, true, CHIME_WAV, sizeof(CHIME_WAV));
#else
    uint8_t amp = _vibAmpArr ? _vibAmpArr[_vibStep / 2] : _vibAmp;
    ledcWrite(VIBRATE_PIN, amp);
#endif
  } else {                    // OFF step / rest
#if defined(BOARD_STICKS3)
    M5.Speaker.stop(CHIME_CH);
#else
    ledcWrite(VIBRATE_PIN, 0);
#endif
  }
  _vibNext = now + _vibPat[_vibStep++];
}
```

### D-06 beep guard
```cpp
// Source: from existing beep() (main.cpp:288-290) + D-06 guard
static void beep(uint16_t freq, uint16_t dur) {
  if (settings().sound) {
#if defined(BOARD_STICKS3)
    if (!_vibPat) compatBeep(freq, dur);   // suppress when chime is active
#else
    compatBeep(freq, dur);
#endif
  }
}
```

### D-07 settingsItems board-conditional label
```cpp
// Source: from existing settingsItems[] (main.cpp:319) + board-conditional index 2
const char* settingsItems[] = {
  "brightness", "sound",
#if defined(BOARD_STICKS3)
  "chime",
#else
  "vibrate",
#endif
  "bluetooth", "wifi", "led", "transcript", "clock rot", "wrist", "12hr", "sleep", "pet", "reset", "back"
};
// SETTINGS_N stays 14; all indices stay the same on both boards.
// applySetting case 2 (s.vibrate = !s.vibrate) is UNCHANGED.
```

### Optional volume init in setup() (recommended — see Open Questions)
```cpp
// In setup(), after M5.begin(cfg) — BOARD_STICKS3 only.
// Default _master_volume = 64 (~25%). 128 is more audible for a desk companion.
#if defined(BOARD_STICKS3)
  M5.Speaker.setVolume(128);
#endif
```

### Optional LEDC guard in setup()
```cpp
// Guard the motor LEDC init to avoid attaching GPIO26 unnecessarily on StickS3.
#if !defined(BOARD_STICKS3)
  ledcAttach(VIBRATE_PIN, 500, 8);
  ledcWrite(VIBRATE_PIN, 0);
#endif
```

---

## State of the Art

| Old (motor era) | New (chime era, StickS3) | Impact |
|------|---------|--------|
| `ledcWrite(VIBRATE_PIN, amp)` — PWM amplitude drives ERM motor | `M5.Speaker.tone(freq, dur, ch, stop, wav, len)` — frequency drives I2S/ES8311 | Approve vs deny now audibly distinct (D-03); frequency carries event identity that motor amplitude cannot |
| `vibratePatternAmpArr` — per-ON-step amplitude = motor strength | `_vibFreqArr` — per-ON-step frequency = pitch | Same data structure; different semantic (amplitude vs pitch); theme-ready parallel |
| Default `M5.Speaker.tone(freq, dur)` sine wave | Square-wave via 6-arg `tone(freq, dur, ch, stop, CHIME_WAV, 16)` | Required for chiptune/Retro D-02 character; default 4-arg tone is sine |

**Not changed / still current:**
- `compatBeep`: continues to use 4-arg `M5.Speaker.tone()` (sine wave is fine for UI button feedback)
- Pattern timing (`vibrateTick` state machine): unchanged logic; only output backend forks
- StickC Plus motor path: byte-for-byte in compiled output

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Proposed frequencies ({659, 220, 330/784, 523/659/784/1047, 392/523} Hz) are musically distinguishable and non-annoying on the ES8311/speaker combination | Chime design | Low — the major/octave intervals are very safe choices; if a tone sounds wrong on hardware, swap the frequency constant and rebuild. No code restructuring needed |
| A2 | `M5.Speaker.setVolume(128)` in setup() produces comfortable desk-companion volume on the StickS3 | Open Questions | Low — if too loud/quiet, change the constant. Not a settings UI change |
| A3 | Pattern pointer equality (`pat == PAT_APPROVE` etc.) is reliable for dispatch in `_chimeSetFreqs` | Chime dispatch | LOW risk — all PAT_* are file-scope `static const` arrays with unique link-time addresses; pointer equality works within one translation unit. If future refactoring moves them, the dispatch must be updated |

**If this table is empty:** — it is not. Three low-risk assumptions are listed above.

---

## Open Questions

1. **Default volume: is `setVolume(128)` appropriate?**
   - What we know: default `_master_volume = 64` (~25%); confirmed in Speaker_Class.hpp:285
   - What's unclear: how loud 128/255 sounds through the StickS3 speaker at desk distance
   - Recommendation: add `M5.Speaker.setVolume(128)` in setup() under `#if BOARD_STICKS3`; user adjusts the constant after hardware testing. No scope creep — this is a hardware init default, not a settings entry

2. **Should OFF steps call `M5.Speaker.stop(CHIME_CH)` explicitly?**
   - What we know: `tone(freq, dur_ms)` expires after exactly `dur_ms` ms; `vibrateTick` advances to the OFF step only after `_vibNext` elapses (≥ that `dur_ms`)
   - What's unclear: whether a late `vibrateTick` call could arrive marginally before the tone expires
   - Recommendation: yes, call `stop(CHIME_CH)` on OFF steps (already in the code example above). It is at most a few microseconds early and ensures clean silence during rests

3. **Are the proposed chime pitches distinguishable through the physical speaker in a noisy desk environment?**
   - What we know: E5 (659 Hz) vs A3 (220 Hz) for approve vs deny is ~3 octaves; major arpeggio for celebrate is standard "win" cue
   - What's unclear: the physical StickS3 speaker frequency response — small speakers roll off at low frequencies; A3 (220 Hz) for deny may be attenuated
   - Recommendation: if deny is inaudible at 220 Hz, raise it to A4 (440 Hz) and keep approve at E5 (659 Hz) — still clearly distinct. Hardware test is the acceptance gate (Success Criterion 1)

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| M5Unified `M5.Speaker` | HAPT-01 chimes | ✓ | 0.2.17 (in `.pio/libdeps/m5stack-sticks3/`) | — |
| `BOARD_STICKS3` build flag | board-conditional seam | ✓ | set in `[env:m5stack-sticks3]` build_flags | — |
| PlatformIO + espressif32 55.03.39 | dual-env build | ✓ | core 3.3.9 (verified Phase 3) | — |
| StickS3 hardware device | HAPT-01 hardware smoke test | `human` | — | compile + no-crash boot is minimum |
| StickC Plus hardware device | HAPT-02 motor regression check | `human` (optional) | — | compile acceptance (device was repurposed per Phase 3 notes) |

**Missing dependencies with no fallback:** none for the build. Hardware testing is the only gate that requires a physical device.

---

## Security Domain

`security_enforcement: true`, ASVS L1. Phase 4 is internal embedded C++ audio feedback — no new network surface, no new auth path, no new inbound data parsing, no new cryptography. The only change to code that handles external input is the `beep()` suppression guard, which is a pure boolean check on a flag that is already trustworthy (set internally by `vibratePattern*`, never by inbound data). No ASVS category newly applies. The BLE inbound path (`data.h`, NimBLE-gated) is unchanged. `nyquist_validation: false` → Validation Architecture section intentionally omitted.

---

## Sources

### Primary (HIGH confidence — read from installed library source in this session)
- `.pio/libdeps/m5stack-sticks3/M5Unified/src/utility/Speaker_Class.hpp` — full API: `tone()` overloads (lines 156, 165), `setVolume` (121), `getVolume` (125), `isPlaying` (108, 113), `stop()` (142, 146), `sound_channel_max = 8` (238), `_master_volume = 64` (285)
- `.pio/libdeps/m5stack-sticks3/M5Unified/src/utility/Speaker_Class.cpp` — `_default_tone_wav[16]` = sine wave (line 73); 6-arg `tone` internal math (duration → repeat_count)
- `src/main.cpp` — full haptic section read (lines 31–112, 288–290, 319, 336–357, 1222–1244, 1298–1381, 1456–1498); confirmed PAT_* arrays, vibratePatternAmp/Arr signatures, vibrateTick logic, beep() body, settingsItems[], applySetting cases
- `src/compat.h` — `compatBeep` uses 4-arg `M5.Speaker.tone(freq, dur)` (line 150); confirmed board-conditional pattern
- `.planning/phases/03-api-port/03-RESEARCH.md` — Speaker_Class.hpp:165 confirmed HIGH in Phase 3; D-09 speaker init; core 3.x LEDC pin-based (no channel-number conflicts); M5Unified M5GFX architecture details
- `.planning/phases/04-haptics-chimes/04-CONTEXT.md` — all locked decisions D-01 through D-07; deferred items; call-site inventory

### Secondary (MEDIUM confidence)
- `.planning/debug/resolved/sticks3-bootloop.md` — hardware realities: PSRAM disabled (CONFIG_SPIRAM=n), HWCDC, ES8311 I2S works correctly post-Phase-3 fixes; speaker path verified operational

---

## Metadata

**Confidence breakdown:**
- M5.Speaker API (tone signatures, channel model, volume, default waveform): **HIGH** — read from installed headers and .cpp
- Chime frequency values (Hz choices per event): **MEDIUM** — standard musical intervals, pending hardware verification
- Board-conditional seam (vibrateTick / beep / settingsItems): **HIGH** — derived from reading the actual code to be modified
- LEDC non-collision on core 3.x: **HIGH** — confirmed in Phase 3 research (pin-based, no channel numbers)

**Research date:** 2026-06-29
**Valid until:** ~2026-07-29 (stable; revisit if M5Unified upgrades past 0.2.17 or the espressif32 platform version changes)
