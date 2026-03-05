# GDMS:Pocket
**Goblinoid Dungeon Mastering System** — microcontroller-based handheld random table roller for tabletop RPGs.

---

## Concept

A dungeon master's companion designed to stay out of the way until you need it. Pull it out, roll a name, an encounter, a room, or whatever your table demands, then put it back in your pocket. GDMS:Pocket is designed to be as ignorable as it is useful — no app, no screen glare, no dead battery notification from your phone mid-session.

Content lives on a microSD card as plain CSV files and JSON recipe files. Adding, removing, or editing tables requires nothing more than a computer and a text editor.

---

## Current Firmware: Alpha 9

### Implemented Features

- Navigate categories corresponding to folders on the SD card
- Browse and select CSV tables or JSON recipe generators within each category
- Roll a random result with a single button press; re-roll instantly with the same button
- Scroll long results that exceed the screen height
- Hold UP/DOWN for fast scrolling through lists
- LED breathing animation indicates power-on state
- LED "pop" flash on every button press
- Passive buzzer feedback — distinct tones per button
- Full options menu (A+B combo from any screen):
  - Buzzer on/off and volume (loud/soft)
  - LED on/off, pop on/off, brightness (low/med/high)
  - Breath speed (fast/med/slow)
  - Sleep timeout (5 min / 10 min / 30 min / never)
- Settings persist to SD card (`/DATA/settings.cfg`) and reload on boot
- Sleep mode: display blanks after inactivity timeout; any button wakes it
- Live battery voltage indicator in the header — 3-bar icon with critical flash
- Header titles truncated with `...` if they would overlap the battery icon
- Startup splash screen (2 seconds on boot)
- About screen with control reference

### JSON Recipe System (V1)

JSON files in a category folder act as multi-part generators. A recipe chains together rolls from multiple CSV tables and assembles them into a single composed output. Features include optional parts (probability `p`), repeat counts, cross-category table references, and a `format` string for custom output layouts. See **Content & Customisation** below and the recipe convention document in the repo for full details.

### CSV Weighted Tables

If a CSV file uses two columns — `weight,entry` — the first column is treated as an integer weight and results are sampled proportionally. Single-column CSVs treat all entries as equally likely. Lines beginning with `#` or `//` are treated as comments and skipped.

---

## Hardware

### Bill of Materials

| Component | Description |
|-----------|-------------|
| Adafruit Feather RP2040 Adalogger | MCU + built-in microSD slot |
| 1.5" SH1107 OLED (SPI, 128×128) | Display |
| 4× momentary pushbutton | Navigation (active-low) |
| Passive buzzer module (3-pin) | Audio feedback |
| LED + current-limiting resistor | Status indicator |
| 3.7V LiPo battery (JST connector) | Power |
| 2× 100kΩ resistor | Battery voltage divider |

### Pin Mapping

#### OLED Display (SH1107, Software SPI)

| OLED Pin | Feather Pin |
|----------|-------------|
| VCC | 3V3 |
| GND | GND |
| SCL (CLK) | D13 |
| SDA (DATA) | D12 |
| DC | D11 |
| CS | D10 |
| RST | D9 |

> **Library patch required:** The U8g2 SH1107 128×128 driver sets `default_x_offset = 96` by default, which causes a 32px horizontal wrap on some panels. Set both `default_x_offset` and `flipmode_x_offset` to `0` in `u8x8_d_sh1107.c`. This must be re-applied after any U8g2 library update. See Log 8 for full details.

#### Buttons (Active-Low, Internal Pull-Up)

| Button | Feather Pin |
|--------|-------------|
| UP | A1 |
| DOWN | A2 |
| A (select / roll) | A3 |
| B (back) | 24 |

Buttons are wired pin → GND. No external pull-up resistors needed.

#### Buzzer (Passive, PWM)

| Signal | Feather Pin |
|--------|-------------|
| SIG | D6 |
| VCC | 3V3 |
| GND | GND |

> A series resistor (~1kΩ) on the signal line is recommended to reduce volume to a comfortable level for table use.

#### LED

| Connection | Feather Pin |
|------------|-------------|
| Anode (via resistor) | D25 |
| Cathode | GND |

> Do not use `PIN_LED` as a variable name — it is defined as a macro in the RP2040 Arduino core.

#### Battery Voltage Divider

The Feather RP2040 Adalogger does not have a built-in battery monitor circuit. An external voltage divider must be wired as follows:

```
VBAT ──[ 100kΩ ]──┬──[ 100kΩ ]── GND
                  │
                  └── A0
```

| Signal | Feather Pin |
|--------|-------------|
| Divider center tap | A0 |

The firmware reads A0, multiplies by 2, and displays a 3-bar battery icon in the header. Voltage thresholds: FULL ≥4.0V, OK ≥3.7V, LOW ≥3.5V, CRIT <3.5V (flashing).

### SD Card

The Adalogger's built-in microSD slot is used via SdFat (Adafruit fork) on SPI1. Format the card as FAT32.

---

## SD Card Data Structure

```
/DATA/
  /category-name/
    table.csv
    generator.json
    _hidden-ingredient.csv
  /another-category/
    ...
  settings.cfg          ← written automatically by firmware
```

- `/DATA` is the root content directory. It must exist.
- Each immediate subfolder of `/DATA` becomes a navigable category in the UI.
- Folders nested deeper than one level are ignored.
- `.csv` and `.json` files within a category folder appear as selectable items.
- Files whose names begin with `_` are hidden from the UI but accessible to JSON recipes.
- `settings.cfg` is written automatically and should not be edited manually.

---

## Software

### Language & Framework

Arduino (C++) targeting the Adafruit Feather RP2040 Adalogger via the Arduino-Pico core.

### Library Dependencies

| Library | Purpose |
|---------|---------|
| U8g2 (olikraus) | OLED driver and graphics |
| SdFat – Adafruit Fork | microSD access |
| ArduinoJson | JSON recipe parsing |

### Controls Reference

| Button | Category / File list | Table / Result view | Options menu |
|--------|---------------------|---------------------|--------------|
| UP | Scroll up | Scroll up | Previous option |
| DOWN | Scroll down | Scroll down | Next option |
| A | Select / enter | Re-roll | Cycle setting value |
| B | — (no-op at root) | Back to file list | Back |
| A + B (held) | Open options | Open options | — |

### Architecture Notes

- State machine with five modes: `MODE_CATS`, `MODE_FILES`, `MODE_TABLE`, `MODE_OPTIONS`, `MODE_ABOUT`
- Page-buffer OLED rendering (`_1_` constructor) — redraws on input events only, with a periodic background refresh for animation (LED breath, battery icon flash)
- Non-blocking LED and buzzer using `millis()` timers throughout
- Battery sampled every 5 seconds via a non-blocking timer; 8-sample average
- All user settings persisted to `/DATA/settings.cfg` as key=value text immediately on change

---

## Planned / Future Features

- Per-category icons (`_icon.bin`, 32×32 1-bit bitmap) rendered in the header
- Saved results — up to 10 entries written to SD, browsable on a Saved page
- Nested JSON recipes (recipes invoking other recipes)
- Ambient mode — auto-rolls at a set interval for generative table atmosphere
- Neopixel indicator
- Battery voltage shown in the About screen
- Migration from `StaticJsonDocument` to `JsonDocument` (ArduinoJson v7 alignment)

---

## Changelog

| Version | Summary |
|---------|---------|
| Alpha 9 | Battery voltage icon in header, header truncation, stale pin comment fixed |
| Alpha 8 | Scrollbar on category list, additional LED options |
| Alpha 7 | Sleep/wake, scrollbar in table view, About screen in options |
| Alpha 6 | A+B combo options menu |
| Alpha 5 | LED breathing and pop animations |
| Alpha 4 | List scrolling |
| Alpha 3 | Hold-to-scroll on UP/DOWN |
| Alpha 2 | Word-aware line wrapping |
| Alpha 1 | Initial SH1107 bring-up, pin map, full hardware test |

---

*GDMS:Pocket by Alexander Sousa, 2026*
