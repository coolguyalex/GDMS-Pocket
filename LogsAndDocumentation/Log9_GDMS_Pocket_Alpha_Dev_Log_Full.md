# GDMS Pocket – Dev Log
## Alpha Development Arc: Logs 1–9
### Word Wrapping, Hold-Scroll, Looping Menus, LED Breathe & Pop, Options Menu, Sleep/Wake, Scrollbars, Battery Indicator

**Period:** January–March 2026  
**Author:** Alexander Sousa  
**Hardware:** Adafruit Feather RP2040 Adalogger + 1.5" SH1107 128×128 OLED (SPI)  
**Firmware at close of log:** GDMSpAlpha9.ino

---

## Overview

This log documents the full Alpha development arc of GDMS:Pocket, starting from the first working hardware bring-up (Alpha 1, covered in Log 8) through the battery voltage indicator and icon geometry work completed in Alpha 9. Each Alpha version is documented in the order it was developed, covering the problem that motivated the work, the approach taken, specific implementation decisions, issues encountered, and how they were resolved.

The codebase at the end of this arc is a stable, feature-complete handheld UI with word-aware text wrapping, hold-to-repeat navigation, looping menus, a full LED animation system, a settings menu with persistence, sleep/wake, scrollbars on all list views, and a live battery voltage indicator in the header.

---

## Alpha 1 — Hardware Bring-Up and SH1107 Display Fix

*(Documented in full in Log 8. Summarised here for continuity.)*

The first Alpha brought up all hardware peripherals on the Feather RP2040 Adalogger: the SH1107 OLED via software SPI, four momentary buttons, a passive buzzer, and an external LED. A full hardware validation sketch confirmed each peripheral was functional in isolation before integration.

The OLED required a library-level patch. The U8g2 SH1107 128×128 driver (`u8x8_d_sh1107.c`) sets `default_x_offset = 96` and `flipmode_x_offset = 96`, which causes a 32px horizontal wrap on the specific AliExpress panel used in this project. Content intended for the left edge appeared instead at the far right of the glass. The fix was to patch both offset values to `0` directly in the library source. This must be re-applied any time U8g2 is updated.

Final pin map established in this session and carried forward unchanged through Alpha 9:

| Peripheral | Pin(s) |
|------------|--------|
| OLED CLK | D13 |
| OLED DATA | D12 |
| OLED DC | D11 |
| OLED CS | D10 |
| OLED RST | D9 |
| Button UP | A1 |
| Button DOWN | A2 |
| Button A | A3 |
| Button B | 24 |
| Buzzer SIG | D6 |
| LED Anode | D25 |
| Battery divider tap | A0 |

---

## Alpha 2 — Word-Aware Line Wrapping

### Problem

When a rolled CSV result was displayed on screen, long entries were being cut at a fixed character count per line with no awareness of word boundaries. This produced mid-word breaks that were jarring to read — a result like "The innkeeper's daugh-\nter has a secret" rather than breaking cleanly between words.

A naive fix of just increasing the line length was not viable — the display is only 128px wide using a 6×12 font, giving approximately 20 usable characters per line before overlapping the scrollbar strip reserved on the right edge.

### Implementation

A custom word-aware line wrapper was written from scratch in Arduino-compatible C++ — no templates, no lambdas, no `std::string`. The constraints were tight: the RP2040 has 264kB of SRAM, and the wrapper needs to handle entries up to several hundred characters long without heap allocation in the render path.

The approach uses a statically allocated array of `WrapLine` structs (`wrapLines[MAX_WRAP_LINES]`, 64 entries max), each holding a null-terminated char buffer of `WRAP_CHARS + 1` bytes (21 bytes). This entire structure lives in static RAM, not on the call stack, and is reused on every redraw.

The wrapping algorithm (`buildWrappedLines()`) walks the source string a word at a time. It tracks the current line's accumulated length and a `lineStart` index into the source string. When a word would overflow the current line, the current line is emitted as a `WrapLine` by recording a `memcpy` slice into the buffer, and the word is re-evaluated on the fresh line.

Three edge cases required specific handling:

**1. Leading spaces at the start of a new line.** When a word causes a wrap and the next character is the inter-word space, that space would become the first character of the new line, producing an unintended indent. The algorithm eats this space explicitly before re-evaluating the next word.

**2. Words longer than the line width.** A word that cannot fit on any line regardless of wrapping (e.g. a very long URL or a run of characters with no spaces) is handled by a hyphenated hard-cut path. The word is broken into chunks of `WRAP_CHARS - 1` characters, each emitted with a trailing `-` character. The final chunk is emitted without a hyphen and left open for continuation by the following word.

**3. Explicit newlines in the source string.** Recipe outputs from the JSON system use `\n` to separate labelled parts (e.g. `"Name: Aldric\nJob: Gravedigger"`). The wrapper detects `\n` in the character scan and emits the current line immediately, then resets for the next line. This means multi-part recipe output wraps correctly at both the explicit newlines and within each part's own text.

The result is stored in `wrapLines[]` and `wrapLineCount`. The table draw function (`drawTable()`) iterates over this array to render visible lines, using `tableScrollLine` as the offset into the array.

---

## Alpha 3 — Hold-to-Repeat Scrolling

### Problem

With the word-aware wrapper producing multi-line results, navigating long entries required many individual UP/DOWN button presses. Holding a button did nothing — each press was treated as a single edge event. This made scrolling through a recipe output with six or eight wrapped lines tedious.

Additionally, scrolling through long category or file lists had the same problem — getting from the top to the bottom of a list with twenty entries required twenty individual presses.

### Implementation

The button handling loop in `loop()` was extended with a hold-to-repeat system. The key design constraint was that this had to work without blocking the main loop — no `delay()`, no polling loops. Everything is driven by `millis()` comparisons.

When a button transitions from not-pressed to pressed (falling edge), the existing debounce and edge-detection logic fires `onButtonPressed()` immediately as before. Two new timestamps are recorded: `holdStartMs[i]` (when the press was first confirmed) and `lastRepeatMs[i]` (when the last repeat event fired).

On every subsequent loop tick while the button remains held, the code checks:
- Has the button been held for longer than `HOLD_DELAY_MS` (500ms)? This is the intentional pause before repeat begins, preventing accidental holds from triggering repeats.
- Has `HOLD_REPEAT_MS` (100ms) elapsed since the last repeat fire?

If both conditions are true, `onButtonPressed()` is called again synthetically, exactly as if the button had been pressed again, and `lastRepeatMs[i]` is updated.

Critically, hold-to-repeat is **only enabled for BTN_UP and BTN_DOWN**. BTN_A and BTN_B are edge-only. This is intentional — holding A should not fire multiple rolls or multiple confirms, and holding B should not navigate back multiple screens. The `isRepeatBtn` flag gates the hold-repeat path specifically to the navigation buttons.

The parameters chosen:
- `HOLD_DELAY_MS = 500` — half a second of intentional hold before repeat starts. Feels deliberate without feeling sluggish.
- `HOLD_REPEAT_MS = 100` — ten repeat events per second. Fast enough to scroll a long list quickly, slow enough to maintain control at one-item-at-a-time granularity.

---

## Alpha 4 — Looping Menus

### Problem

Reaching the bottom of a category list or file list and pressing DOWN did nothing — the cursor stopped at the last item. Similarly, pressing UP at the top item stopped at item 0. This was the simplest possible boundary behaviour but it felt like hitting a wall. On a short list you'd frequently need to wrap — for example, to get from item 1 back to the last item quickly.

### Implementation

The cursor arithmetic in `onButtonPressed()` was changed from clamped to wrapping for both list modes.

For `MODE_CATS`:
```cpp
// Before (clamped):
if (b == BTN_UP && cursor > 0) cursor--;

// After (looping):
cursor = (cursor <= 0) ? (int16_t)(catCount - 1) : cursor - 1;
```

The same pattern applies to DOWN and to the file list in `MODE_FILES`. The `fileCursor` wraps analogously using `fileCount`.

The scroll window clamping (`scrollTop` tracking) was left unchanged — it continues to keep the selected item visible by sliding the viewport. Wrapping from item 0 to the last item causes `scrollTop` to jump to `catCount - rows`, correctly showing the end of the list. This required no special handling because the scroll clamping logic (`if (cursor < scrollTop) scrollTop = cursor`) already handles the case where the cursor moves above or below the current viewport.

The options menu cursor (`optCursor`) uses the same looping pattern, added when that menu was implemented in Alpha 6.

---

## Alpha 5 — LED Breathing and Pop

### Problem

The external LED on D25 was not doing anything useful. The original notes listed two desired behaviours: a slow breathing animation to indicate power-on state, and a brief bright flash ("pop") on each button press to give physical feedback. Both needed to run without blocking the main loop — a breathing animation driven by `delay()` would freeze the display and input handling.

### Implementation

**Triangle-wave breathing.** A sine wave would be more natural but requires floating-point math in every loop iteration, which is expensive on a microcontroller. A triangle wave — linear ramp up then linear ramp down — is computed from `millis()` with integer arithmetic only and produces a smooth enough fade for an LED.

```cpp
uint32_t t = (now + ledBreathOffset) % period;
if (t < period / 2) {
    val = (uint8_t)((uint32_t)breathMax * 2 * t / period);
} else {
    val = (uint8_t)((uint32_t)breathMax * 2 * (period - t) / period);
}
analogWrite(LED_PIN, val);
```

`period` is a runtime value pulled from `BREATH_PERIOD_TBL[]` (indexed by `breathSpeed` enum), so the breathing speed can be changed at runtime by the options menu without touching the animation code. `breathMax` similarly comes from `LED_BREATH_MAX_TBL[]` indexed by `ledBrightness`.

**Pop interrupt.** When a button is pressed, `ledPop()` is called. It captures the current phase of the breath cycle into `ledBreathOffset`, sets `ledPopActive = true`, writes full (or brightness-scaled) brightness immediately via `analogWrite`, and sets `ledPopEndMs` 100ms in the future.

In `ledUpdate()`, when `ledPopActive` is true, the breath calculation is skipped entirely — the LED holds at pop brightness until `ledPopEndMs` is reached. When the pop expires, `ledPopActive` is cleared and the animation resumes from the captured phase offset. This means the LED doesn't jerk back to wherever the breath wave happened to be — it picks up from the same point the cycle was at when the pop fired, producing a seamless transition back into breathing.

**Master switches.** `ledEnabled` is a boolean master switch that turns the entire LED off (breath + pop). When false, `ledUpdate()` immediately writes 0 to the LED and clears `ledPopActive`. `ledPopEnabled` independently controls whether pops fire while breathing continues — useful for someone who finds the flash distracting but wants the ambient breathing. Both are exposed in the options menu.

---

## Alpha 6 — A+B Combo Options Menu

### Problem

The device needed a settings menu but had only four buttons, all needed for navigation. Dedicating a button to "open menu" would sacrifice one of the four navigation inputs. The solution was to use a simultaneous button chord — pressing A and B together — as a meta-input to open the options screen from any mode.

### Implementation

**Chord detection.** The A+B combo is detected in `loop()` independently of the normal button edge-detection path. Both `BTN_A` and `BTN_B` GPIO states are read directly each tick. If both are LOW, a timer (`abComboArmMs`) starts. If both remain LOW for `AB_COMBO_MS` (80ms), the combo fires. This 80ms window requires both buttons to be held simultaneously for long enough to be intentional, while short enough to feel responsive.

`abComboFired` prevents the options screen from being opened repeatedly while the chord is held. It resets only when one or both buttons are released. The combo is also suppressed if already in `MODE_OPTIONS` (you can't open options from inside options).

**prevMode.** When the combo fires, the current `mode` is saved to `prevMode` before switching to `MODE_OPTIONS`. Pressing B inside the options menu restores `mode = prevMode`, returning to wherever the user was — categories, files, or result view — without losing their position.

**Options menu contents (Alpha 6 initial):**
- Buzzer on/off
- Buzzer volume (loud/soft)
- LED on/off
- LED pop on/off

Each option is displayed as `label    <value>` with the value right-aligned. Pressing A cycles the selected setting's value forward through its possible states. Settings took effect immediately on change; persistence to SD was added in Alpha 7.

**Buzzer volume as a software approximation.** The passive buzzer module used has no hardware volume control. The only software-accessible parameter is tone duration — shorter beeps are perceived as quieter because less energy is delivered to the buzzer membrane. `BUZ_VOL_SOFT` halves the requested duration before calling `tone()`. This is an imperfect approximation; the recommendation remains to add a ~1kΩ series resistor on the signal line for a true hardware volume reduction, but the software approach is a usable interim solution.

---

## Alpha 7 — Sleep/Wake, Scrollbars, and About Screen

### Sleep and Wake

The device needed a display-off idle timeout to preserve battery when left on a table between rolls. The implementation follows the same non-blocking `millis()` pattern as the LED and buzzer.

`lastActivityMs` is updated every time `onButtonPressed()` is called — not `lastInputMs`, which tracks the last input for OLED throttle purposes, but a separate timestamp specifically for the sleep system. This distinction matters because the OLED throttle idle timer is much shorter (a few milliseconds) than the sleep timeout (several minutes), and conflating them would cause incorrect behavior.

In `loop()`, every tick checks whether `now - lastActivityMs >= SLEEP_TIMEOUT_TBL[sleepTimeout]`. If the device is not already sleeping and the timeout has elapsed, `deviceSleeping` is set to true and the display is blanked by rendering a page-buffer cycle with no drawing commands — this produces a full black frame rather than leaving the last image static on the OLED.

Wake is handled inside `onButtonPressed()`. The first line after updating timestamps checks `deviceSleeping`. If true, it clears the flag, calls `uiRedraw()` to restore the display, and then continues processing the button that caused the wake. This means the wake button is not consumed by the wake action — it performs its intended function (UP/DOWN/A/B) as well as waking the display. This felt more responsive than requiring an extra press just to wake.

Four timeout values are available via the options menu: 5 minutes, 10 minutes, 30 minutes, and Never. The "Never" case is implemented by storing `0` in `SLEEP_TIMEOUT_TBL[SLEEP_NEVER]` and checking `tmo > 0` before comparing to `lastActivityMs`, so a zero timeout simply never triggers.

### Scrollbars

Long lists — more categories than fit on one screen, more files than fit on one screen, result text longer than the visible area — needed a visual indicator of scroll position and remaining content.

A shared `drawListScrollbar()` function was written to serve the category list, the file list, and the options menu (all of which are indexed, fixed-size lists). A separate but structurally identical scrollbar is drawn inline in `drawTable()` for the result text view (which uses wrapped-line count rather than item index).

**Scrollbar anatomy.** The scrollbar is a 3px wide vertical strip at the right edge of the display (x=125..127):
- A filled triangle at the top pointing up, indicating scroll-up is available. Hollow (outline only) when already at the top.
- A filled triangle at the bottom pointing down. Hollow when at the bottom.
- A thin centre line (1px `drawVLine`) connecting the two triangle bases — the track.
- A filled rectangle (the thumb) positioned on the track proportionally to the current scroll position.

Thumb height is proportional to `visibleRows / totalItems`, clamped to a minimum of 3px so it remains visible even with very long lists. Thumb position is scaled linearly: `thumbY = TRK_TOP + (thumbRange * scrollTop / maxScroll)`.

The scrollbar is only rendered when the list is longer than the visible area — if all items fit on screen, no scrollbar is drawn and that column of pixels is available for content.

**Result text scrollbar.** The table view scrollbar uses the same triangle-track-thumb geometry but parameterised on `wrapLineCount` (total wrapped lines) and `maxLinesOnScreen` (how many lines fit in the text area below the header). This scrollbar is unconditionally positioned at x=125..127, which is why the word wrapper uses `WRAP_CHARS = 20` — reserving the rightmost 6px column for the scrollbar strip even when the content is short.

### About Screen

A fifth UI mode (`MODE_ABOUT`) was added, accessible from the bottom row of the options menu. The About screen displays a brief description of the device, the author name, year, and a compact controls reference (which buttons do what). Pressing B from About returns to the options menu rather than to `prevMode`, since About is a sub-page of Options rather than a top-level destination.

### Settings Added in Alpha 7

The options menu was expanded with three additional entries:
- **LED Brightness:** cycles through Low (breathMax=20, popBrightness=25), Med (80/128), High (180/255). Both breath ceiling and pop peak are read from lookup tables indexed by the `LedBright` enum.
- **Breath Speed:** cycles through Fast (500ms period), Med (3000ms), Slow (10000ms). The period is read from `BREATH_PERIOD_TBL[]` each animation tick, so changing the speed takes effect immediately on the next `ledUpdate()` call.
- **Sleep Timeout:** cycles through 5 min, 10 min, 30 min, Never.

### Settings Persistence

All settings were given persistence to SD in Alpha 7. `saveSettings()` writes a simple `key=value\n` text file to `/DATA/settings.cfg` immediately whenever any setting is changed. `loadSettings()` reads it on boot. Unknown keys are silently ignored for forward compatibility.

The format is intentionally human-readable and simple — no binary, no JSON, no checksum. The file is small enough that the write is instantaneous from the user's perspective.

---

## Alpha 8 — Scrollbar on Category List, BAT Placeholder

Two remaining list views that lacked scrollbars were addressed: the category list (`MODE_CATS`) and the options menu. Both now call `drawListScrollbar()` with their respective item count, visible row count, and scroll offset. This completed scrollbar coverage across all list-type views.

A static `"BAT"` text label was added to the top-right corner of the header as a placeholder, marking the intended position of the future battery indicator. This was explicitly a temporary placeholder — not functional, just a spatial reservation while the battery monitor circuit and firmware were designed.

---

## Alpha 9 — Battery Voltage Monitor and Header Truncation

### Hardware: External Voltage Divider

The Feather RP2040 Adalogger does not include a built-in battery voltage monitor circuit. This is documented by Adafruit and is a deliberate board design decision given the RP2040's limited analog GPIO count. An external resistor divider is required:

```
VBAT ──[ 100kΩ ]──┬──[ 100kΩ ]── GND
                  │
                  └── A0
```

Two equal resistors produce exactly half the battery voltage at the center tap. The RP2040 ADC reference is 3.3V; a fully charged LiPo at 4.2V would exceed this if read directly and potentially damage the pin. Halving gives a maximum of 2.1V at A0, safely within range. The firmware doubles the ADC result to recover the true voltage.

A0 is free for this purpose. A stale comment in the source had incorrectly listed A0 as the B button pin — a holdover from an earlier hardware revision. The actual B button is on pin 24. The comment was corrected in this session.

### ADC Sampling

The RP2040 ADC is functional but noisy. `sampleBattery()` takes 8 readings with a 1ms delay between each and averages them. ADC resolution is explicitly set to 12-bit (0–4095) at the start of each call. The conversion to voltage:

```cpp
float raw = sum / 8.0f;
float vA0 = (raw / 4095.0f) * 3.3f;
battVoltage = vA0 * 2.0f;
```

Sampling occurs on a 5-second non-blocking timer in `loop()`, consistent with the existing non-blocking patterns for LED and buzzer. An initial sample is taken during `setup()` before `uiRedraw()` so the first rendered frame shows a real reading rather than the optimistic default of 4.2V.

### Voltage Thresholds and BattLevel Enum

Four charge tiers were defined. The `BattLevel` enum values are assigned so that the numeric value equals the number of bars to draw — this eliminates any translation step in the drawing code:

| Enum | Value | Bars drawn | Voltage |
|------|-------|-----------|---------|
| `BATT_CRIT` | 0 | 0 (empty, flashing) | < 3.5V |
| `BATT_LOW` | 1 | 1 bar | ≥ 3.5V |
| `BATT_OK` | 2 | 2 bars | ≥ 3.7V |
| `BATT_FULL` | 3 | 3 bars | ≥ 4.0V |

The `fillSegs = (int)lvl` assignment in the drawing function requires no case statement or lookup table.

### Battery Icon Design and Geometry — Iteration 1

The initial icon was designed with 4 fill bars to match the 4 charge tiers, body width `BODY_W=18`, `ICON_X=108`. The bars were 3px wide, 1px gaps, left inner pad of 1px. This fit the interior cleanly:

```
4 bars × 3px + 3 gaps × 1px + 1px left pad + 1px right pad = 16px interior
Body interior = 18 - 2 (outline) = 16px ✓
```

When the design was revised to 3 bars (FULL=3, OK=2, LOW=1, CRIT=0/empty/flash) the bar count was reduced from 4 to 3 without resizing the body, leaving a 4px dead space on the right side of the interior. The rightmost bar appeared to have unequal padding compared to its neighbours.

Additionally, a geometry error in `SEG_X0` meant the rightmost bar's right edge landed exactly at the body's inner right wall (x=124 in an 18px body at x=108), producing zero clearance between the bar and the outline — the bar appeared visually merged with the body wall.

### Battery Icon Design — Iteration 2 (Final)

Both issues were corrected by properly sizing the body for 3 bars with 1px clearance on all four sides of every bar:

```
Required interior = 1px left pad + 3×3px bars + 2×1px gaps + 1px right pad = 13px
BODY_W = 13px interior + 2px outline = 15px
ICON_X = 128 - 15px body - 2px nub - 1px gap from edge = 111
```

Verified bar positions:

| Bar | Left edge | Right edge | Right clearance to inner wall |
|-----|-----------|------------|-------------------------------|
| 0 | x=113 | x=115 | 9px |
| 1 | x=117 | x=119 | 5px |
| 2 | x=121 | x=123 | 1px |

Inner right wall at x=124. Every bar has exactly 1px clearance on all four sides.

The nub sits at x=126..127, flush with the display's right edge.

### Critical Flash Animation

At `BATT_CRIT`, the icon alternates between a normal state (empty body outline, no bars) and an inverted state (white flood-fill of body and nub, with bar positions punched back to black using `setDrawColor(0)`) on a 500ms period derived from `(millis() / 500) % 2`. No persistent blink state variable is needed — the current frame's appearance is computed entirely from the current `millis()` value on each redraw.

### Header Title Truncation

The static `"BAT"` label was replaced by the icon. The `drawHeader()` function was updated to measure the title string's rendered pixel width using `u8g2.getStrWidth()` and compare it to the available space (`MAX_TITLE_W = 109px`, being `ICON_X=111` minus a 2px gap). If the title exceeds this budget, characters are removed one at a time until `title + "..."` fits, then `"..."` is appended.

Pixel measurement via `getStrWidth()` is used rather than a character-count heuristic because `u8g2_font_8x13_tf` is not strictly monospaced — narrower glyphs would allow more characters than a fixed 8px-per-character estimate. Measuring directly is always accurate regardless of font metrics.

---

## Current Known-Good State at Alpha 9

- All four buttons debounced and functional; UP/DOWN have hold-to-repeat; A/B are edge-only
- Looping navigation in all list modes
- Word-aware wrapping with hyphenation for oversized words, explicit newline support
- LED breathing (triangle wave, configurable speed and brightness), pop interrupt on button press
- Passive buzzer feedback, per-button distinct frequencies, configurable volume and master on/off
- A+B chord opens options from any mode; B closes and returns to previous mode
- Options menu: 7 configurable settings + About entry, all persisted to SD immediately on change
- Sleep/wake: configurable timeout, blanked display, any button wakes and processes simultaneously
- Scrollbars on all list views (categories, files, options) and result text view
- Live battery voltage indicator in header: 3-bar icon, 5-second sampling, critical flash
- Header title truncation protects battery icon from overflow

---

## Known Issues and Notes for Next Session

- **USB reads as "full":** When powered via USB-C, the BAT pin reflects the regulated USB supply rather than a LiPo cell. The battery icon will show FULL. No software-reliable USB detection is available on this board without additional hardware. Accepted behaviour.
- **Bar orientation:** Current bars fill left-to-right (leftmost bar is the last one remaining as battery drains). Right-to-left fill (bars drain from left, only right bar remains at LOW) may be more intuitive and is worth evaluating.
- **Battery voltage in About screen:** Displaying the raw measured voltage in the About screen would be useful for field calibration without a serial monitor.
- **Options menu `optScrollTop` is `static` inside `drawOptions()`:** This works correctly but is slightly unusual — it means the options scroll position persists across visits to the menu. Worth making it a proper file-scope variable for clarity.
- **`PIN_LED` macro collision warning:** The RP2040 Arduino core defines `PIN_LED` as a macro. The sketch uses `LED_PIN` to avoid collision, but this should be noted in any future code review.

---

**End of Log — Alpha 9**  
*Alexander Sousa, March 2026*
