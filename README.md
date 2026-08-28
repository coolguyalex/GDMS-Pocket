# GDMS:Pocket
**Goblinoid Dungeon Mastering System** — microcontroller-based handheld random table roller for tabletop RPGs.

---

## Concept

A dungeon master's companion designed to stay out of the way until you need it. Pull it out, roll a name, an encounter, a room, or whatever your table demands, then put it back in your pocket. GDMS:Pocket is designed to be as ignorable as it is useful — no app, no screen glare, no dead battery notification from your phone mid-session.

Content lives on a microSD card as plain CSV files and JSON recipe files. Adding, removing, or editing tables requires nothing more than a computer and a text editor.

---

## Current Firmware: Alpha 10 

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
  - Data Edit Mode — exposes the SD card as a USB mass storage drive
- Settings persist to SD card (`/DATA/settings.cfg`) and reload on boot
- Sleep mode: display blanks after inactivity timeout; any button wakes it (suspended while in Data Edit Mode)
- Live battery voltage indicator in the header — 3-bar icon with critical flash
- Header titles truncated with `...` if they would overlap the battery icon
- Startup splash screen (2 seconds on boot)
- About screen with control reference

### Data Edit Mode

Selecting "Data Edit" from the options menu puts the device into USB mass storage mode: the SD card mounts on the connected computer as a normal removable drive, so content can be added, edited, or removed with a regular file explorer — no card removal required. The device still charges normally over the same USB-C connection while in this mode.

- Entered from the options menu (A → cycles into the mode)
- Screen displays "DATA EDIT MODE" / "press A + B to exit." while active
- Exit via A+B (held) — the same combo used elsewhere to open the options menu is repurposed here to exit, since the options menu isn't reachable while the SD card is handed off to the host
- On exit, the SD card is unmounted from the host, remounted for the firmware's own file access, and categories are rescanned so any added/removed/edited content shows up immediately
- Implemented via Adafruit_TinyUSB's mass storage (MSC) class, using the same SdFat `SD` object already used for file-level access elsewhere in the firmware — read/write/flush callbacks operate at the raw sector level rather than the file level
- Sleep timeout is disabled while in this mode so the screen doesn't blank mid-transfer

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


### BOM with cost estimates
|Component                                |Price     |Source                                |
|-----------------------------------------|----------|--------------------------------------|
|Adafruit Feather RP2040 Adalogger        |$14.95    |Adafruit / DigiKey, retail single unit|
|1.5” SH1107 128×128 monochrome OLED (SPI)|$4.50     |AliExpress, bulk 10-pack pricing      |
|Momentary pushbuttons ×4                 |$0.60     |AliExpress, bulk 100pc pack           |
|Passive buzzer module                    |$0.35     |AliExpress, bulk pack                 |
|LED + resistor                           |$0.10     |AliExpress, bulk pack                 |
|3.7V LiPo battery w/ JST                 |$2.50     |AliExpress, bulk pack                 |
|100kΩ resistors ×2                       |$0.02     |AliExpress, bulk reel                 |
|Slide switch                             |$0.30     |AliExpress, bulk pack                 |
|Custom PCB, bare board (10-unit batch)   |$1.50     |PCBWay                                |
|3D printed PETG enclosure                |$3.00     |Self-sourced filament                 |
|Misc wire/headers/hardware               |$1.00     |AliExpress, bulk pack                 |
|**Total**                                |**$28.82**|                                      |

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
>
> This has recurred at least once in practice after installing an unrelated library (Adafruit TinyUSB) via Library Manager, which appears to have triggered a U8g2 reinstall/update as a side effect. Worth checking `default_x_offset`/`flipmode_x_offset` any time the display shows an unexplained horizontal shift, not just after deliberate U8g2 updates. As of U8g2's source (checked Feb 2025), the maintainer's own comment on this section confirms `0` is the correct value for panels like this one — see [olikraus/u8g2#2581](https://github.com/olikraus/u8g2/issues/2581).

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
| Adafruit TinyUSB | USB mass storage (Data Edit Mode) — requires Tools > USB Stack > Adafruit TinyUSB in Arduino IDE |

### Controls Reference

| Button | Category / File list | Table / Result view | Options menu | Data Edit Mode |
|--------|---------------------|---------------------|--------------|-----------------|
| UP | Scroll up | Scroll up | Previous option | — |
| DOWN | Scroll down | Scroll down | Next option | — |
| A | Select / enter | Re-roll | Cycle setting value | — |
| B | — (no-op at root) | Back to file list | Back | — |
| A + B (held) | Open options | Open options | — | Exit Data Edit Mode |

### Architecture Notes

- State machine with six modes: `MODE_CATS`, `MODE_FILES`, `MODE_TABLE`, `MODE_OPTIONS`, `MODE_ABOUT`, `MODE_DATA_EDIT`
- Page-buffer OLED rendering (`_1_` constructor) — redraws on input events only, with a periodic background refresh for animation (LED breath, battery icon flash)
- Non-blocking LED and buzzer using `millis()` timers throughout
- Battery sampled every 5 seconds via a non-blocking timer; 8-sample average
- All user settings persisted to `/DATA/settings.cfg` as key=value text immediately on change
- SD card is accessed two ways depending on mode: file-level (`SD.open()`, `FsFile`) for normal category/table browsing, and raw block/sector-level (`SD.card()->readSector()`/`writeSector()`) for Data Edit Mode's USB mass storage exposure — same underlying SdFat object, different API surface

---

## Planned / Future Features

- Auto-mode: a dearf-fortressian ambient atory generatir where randomyl generated entires drive a strange atory which unfolds before the reader. 
- Per-category icons (`_icon.bin`, 32×32 1-bit bitmap) rendered in the header
- Saved results — up to 10 entries written to SD, browsable on a Saved page
- Nested JSON recipes (recipes invoking other recipes)
- Ambient mode — auto-rolls at a set interval for generative table atmosphere
- Neopixel indicator
- Battery voltage shown in the About screen
- Migration from `StaticJsonDocument` to `JsonDocument` (ArduinoJson v7 alignment)
- **GDMS-pocket Companion App** — desktop tool for content authoring, generation, and firmware flashing (see below)

---

## GDMS-pocket Companion App (Planned)

A desktop companion tool for authoring and managing GDMS:Pocket content, shipped alongside the device rather than as a separate purchase. Not yet implemented — this section documents the intended design and open questions.

### Concept

**Data Edit Mode is now implemented on-device** (see Current Firmware above) — the SD card can be exposed as a plain USB mass storage drive from the options menu, no companion app required. The companion app remains planned as a richer, GDMS-aware layer on top of this: same name family, same visual language, same spirit of staying out of the way until needed, but with content-authoring tools (Markov-chain generation, structured CSV/JSON editing, firmware flashing) that a generic file explorer can't offer.

### Aesthetic

Black-on-green CLI-style interface, matching the terminal look of the original [GDMS](https://github.com/coolguyalex/GDMS) desktop tool. Navigation is entirely text-based:
- Keyboard shortcuts for every applet/action
- Clickable text labels as the mouse-driven alternative — text glows on hover and blinks briefly when clicked, echoing a terminal cursor rather than a conventional button

### Device Detection & Interaction

Still an open design question — rough intent:
- When the app is running and a GDMS:Pocket is connected via USB, the device should detect the companion app's presence (likely via a simple serial handshake) and unlock companion-aware functionality on-device, going beyond the generic mass-storage exposure Data Edit Mode already provides.
- Once connected, the app could:
  - Browse and directly edit category folders, CSV tables, and JSON recipes on the SD card
  - Generate new content — e.g. Markov-chain-based text generators trained on existing tables, to bootstrap new CSV entries from a sample of existing ones
  - Flash new firmware to the device (bundling firmware updates with the same tool used for content, instead of requiring separate Arduino IDE / bootloader steps)
  - **Chiptune mode** (low priority / just for fun) — compose or import chiptune-style songs and play them back through the device's passive buzzer, in the spirit of [goblinoidDungeonSynthSystem](https://github.com/coolguyalex/goblinoidDungeonSynthSystem)

### Open Questions

- ~~Whether "Data Edit Mode" on the device becomes a distinct on-device mode of its own, or stays implicit~~ — resolved: it's a distinct on-device mode, entered/exited explicitly from the options menu, and works as plain mass storage independent of any companion app
- Whether the companion app's smarter feature set (content generation, firmware flashing) talks to the device over serial while Data Edit Mode's plain mass-storage exposure remains the fallback for basic editing
- How much of the companion-aware logic should live in GDMS:Pocket firmware itself (e.g. a lightweight serial command protocol) vs. entirely in the companion app

---

## Changelog

| Version | Summary |
|---------|---------|
| Alpha 10| Data Edit Mode (USB mass storage for SD card content editing) |
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
