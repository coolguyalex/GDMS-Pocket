# GDMS Pocket – Dev Log  
## SH1107 1.5" OLED Bring-Up, Pin Mapping, Hardware Validation, JSON recipe functions, _file.ext ignore behavior.

### Date
January 2026

---

## Goal of This Session
Bring up a newly received **1.5" SH1107 OLED (SPI)** on a **Feather RP2040 Adalogger**, resolve display alignment issues, finalize a clean pin map for all peripherals (OLED, buttons, LED, buzzer), and validate everything with a single hardware test sketch.

---

## Hardware Used
- Adafruit Feather RP2040 Adalogger
- 1.5" OLED, controller marked **SH1107**, SPI interface (AliExpress module)
- 4 momentary buttons (active-low)
- Passive buzzer (3-pin module)
- Single indicator LED + resistor

---

## OLED Bring-Up and Debugging

### Initial Issues
- Screen powered but showed:
  - shifted / wrapped content
  - apparent ~32px horizontal offset
- U8g2 rotation flags and page buffer settings did **not** resolve the issue.

### Root Cause Identified
The issue was traced to the **U8g2 SH1107 128×128 driver** (`u8x8_d_sh1107.c`).

In the `u8x8_sh1107_128x128_display_info` struct, the library defines:

```c
default_x_offset = 96
flipmode_x_offset = 96
```

This causes horizontal **wraparound addressing** on some SH1107 panels (columns 0–31 appear at the far right of the display).

### Final Fix (Driver-Level)
The OLED panel used in this project expects a **true column 0 origin**.

**Library patch applied:**
```diff
- default_x_offset = 96
- flipmode_x_offset = 96
+ default_x_offset = 0
+ flipmode_x_offset = 0
```

After this change:
- `(0,0)` maps correctly to the top-left of the glass
- `drawFrame(0,0,128,128)` renders correctly
- No application-level coordinate hacks required

⚠️ **Important:** This change lives inside the U8g2 library.  
Documented in repo so it can be re-applied if the library is updated.

---

## OLED Configuration (Final, Working)

- Library: **U8g2**
- Constructor used:
```cpp
U8G2_SH1107_128X128_1_4W_SW_SPI
```

- Software SPI (intentionally avoids SPI conflicts with SD card)

### OLED Pin Mapping
| OLED | Feather RP2040 |
|----|----|
| VCC | 3V3 |
| GND | GND |
| SCL (CLK) | D13 |
| SDA (DATA) | D12 |
| DC | D11 |
| CS | D10 |
| RST | D9 |

---

## Final Pin Map (All Components)

### Buttons (Active-Low, INPUT_PULLUP)
Buttons are wired **pin → GND**.

| Button | Pin |
|------|----|
| Up | A2 |
| Down | A3 |
| A | A1 |
| B | A0 |

### Buzzer
- Passive buzzer
- PWM/tone capable

| Signal | Pin |
|------|----|
| SIG | D5 |
| VCC | 3V3 |
| GND | GND |

### LED
| Connection | Pin |
|-----------|----|
| Anode (via resistor) | D24 |
| Cathode | GND |

⚠️ Note: `PIN_LED` is defined as a macro in the RP2040 core.  
Do **not** use `PIN_LED` as a variable name in sketches.

---

## Validation Sketch
A single minimal test sketch was written and verified that:

- OLED initializes and renders text correctly
- All four buttons register reliably
- LED turns on when any button is pressed
- Buzzer emits distinct tones per button

This sketch serves as:
- a wiring validation tool
- a future regression test for hardware changes

---

## Current Known-Good State
- OLED mapping is correct and stable
- All pins are finalized and non-conflicting
- SD card SPI remains unused and unaffected
- Ready to:
  - switch OLED to `_F_` buffer mode (full framebuffer)
  - integrate OLED + input handling back into `POC_V04`
  - formalize pin definitions in a shared `pins.h`

---

## Next Logical Steps
1. Create `pins.h` reflecting the finalized pin map
2. Convert OLED constructor to `_F_` buffer on RP2040
3. Merge U8g2 rendering into GDMS UI code
4. Restore SD card functionality and test coexistence

---

**End of log**


## Log 8 Addendum – JSON Recipes, UI Refinements, and Audio Tuning  
**Date:** 2026  
**Author:** Alexander Sousa  

This addendum documents work completed after the initial Log 8 entry, focused on restoring and extending functionality following a partial code recovery, as well as refining UI and hardware feedback for the 128×128 OLED build.

---

### 1. JSON Recipe System (V1 Re-Integration)

The JSON “recipe” system was re-implemented after recovering a near-latest branch. The intent of this system is to allow higher-level generators (e.g. NPCs, rooms) to chain together multiple random table rolls into a single output.

**Key features implemented:**
- `.json` files are now treated as selectable generators alongside `.csv` tables.
- Files prefixed with `_` are hidden from the UI but remain accessible to recipes (e.g. `_icon.bin`, `_internal.csv`).
- JSON recipes define a `parts[]` array, each part specifying:
  - `roll`: a CSV path to sample from
  - `label` (optional): prepended to the result
  - `p` (optional): probability (0.0–1.0) that the part is included
- Recipes may reference:
  - Local files (relative to the recipe’s folder)
  - Cross-category files using `/DATA`-rooted paths (e.g. `/items/treasure.csv`)

**Runtime behavior:**
- Selecting a `.json` file runs the recipe and displays the composed output in the same table view as CSVs.
- Rerolling (`A` in table view) correctly re-executes the recipe rather than sampling raw JSON lines.

This establishes a stable **V1 recipe system** suitable for NPC, room, and encounter generators.

---

### 2. Input Handling Fixes (CSV vs JSON)

A critical fix was made to ensure consistent behavior between:
- Initial selection of a file (`MODE_FILES`)
- Rerolling while viewing results (`MODE_TABLE`)

Both code paths now branch explicitly based on file extension:
- `.csv` → `pickRandomCsvLine()`
- `.json` → `runJsonRecipeV1()`

This prevents erroneous behavior where rerolls on JSON generators would previously display raw JSON keys.

---

### 3. UI Layout Updates for 128×128 OLED

With the transition from a 0.96″ display to a 1.5″ 128×128 OLED:

- **Header text** was upgraded to a larger font for stronger visual identity.
- **Body text font and wrapping were intentionally left unchanged** to preserve density and readability.
- Vertical spacing was adjusted so content starts closer to the header divider, reclaiming usable screen space without risking overlap.
- Category list, file list, and table view now share consistent vertical layout assumptions.

These changes improve readability while keeping rendering logic stable.

---

### 4. Startup Splash Screen

A dedicated splash screen was added to display on boot for ~2 seconds before SD initialization and UI startup.

**Displayed text:**
GOBLINOID
DUNGEON
MASTERING
SYSTEM

-Alexander Sousa
2026


This establishes device identity and provides a clean, intentional startup experience.

---

### 5. Buzzer Volume Mitigation

The passive buzzer was identified as overly loud when driven directly via `tone()`.

**Mitigations discussed and partially implemented:**
- Reduced beep duration for UI feedback.
- Recommended hardware fix: series resistor (≈1 kΩ) on the buzzer signal line.
- Optional RC filtering discussed for further softening if desired.

This brings audio feedback more in line with a “quiet handheld tool” rather than an alarm-like device.

---

### 6. Future Considerations (Not Yet Implemented)

- Per-category icons loaded from SD (e.g. `_icon.bin` in each folder), rendered as 1-bit bitmaps.
- JSON recipes invoking other JSON recipes (nested generators).
- User-editable settings and saveable outputs.
- Optional migration from `StaticJsonDocument` to `JsonDocument` to align with newer ArduinoJson APIs.

---

**Status:**  
The recovered codebase is now functionally equivalent (and in some areas improved) relative to the original pre-loss state, with JSON recipes, UI scaling, and startup identity fully restored.


# GDMS-pocket JSON Recipe Conventions (V1)

## Purpose
A **recipe** is a `.json` file that generates a multi-line output by chaining together one or more random rolls from CSV tables.

Recipes appear in the UI alongside CSV files and can be selected like any other generator.

---

## File placement and visibility

### Category folders
All content lives under:

```
/DATA/<CATEGORY>/
```

A category folder can contain:
- visible CSV tables (`names.csv`)
- visible recipes (`npc.json`)
- **hidden support tables and assets** prefixed with `_` (`_traits.csv`, `_icon.bin`)

### Hidden files
Any file beginning with `_` is:
- **not shown in the UI file list**
- still accessible to recipes via `"roll"` paths

This allows “ingredient tables” and internal assets to be hidden from the user-facing interface.

---

## Recipe JSON structure

### Required structure
A V1 recipe is a JSON object containing:

- `parts`: an array of roll steps

```json
{
  "parts": [
    { "label": "Name", "roll": "names.csv" },
    { "label": "Goal", "roll": "_goals.csv", "p": 0.8 }
  ]
}
```

---

### `parts[]` entries

Each part is an object with the following fields:

#### `roll` (required)
Path to a CSV file that the recipe will roll on.

- **Relative path** (local to the recipe folder):
  ```json
  "roll": "names.csv"
  ```

- **Cross-category path** (rooted under `/DATA`):
  ```json
  "roll": "/items/treasure.csv"
  ```

---

#### `label` (optional)
Text prefix shown before the rolled result.

If present:
```
Label: <rolled line>
```

If omitted, the rolled line is printed alone.

---

#### `p` (optional)
Probability that this part appears in the output.

- Range: `0.0` to `1.0`
- Default: `1.0` (always included)

Examples:
- `1.0` → always included
- `0.25` → included ~25% of the time
- `0.0` → never included (useful for testing)

---

## Output format (V1)

Recipe output is a **multi-line string**.
Each included part contributes exactly one line, in order.

Example output:

```
Name: Joryn Kettle
Job: Dockhand
Trait: Always humming
```

Notes:
- Parts are processed top to bottom.
- Failed rolls are silently skipped.
- If no parts produce output, the UI displays:
  ```
  No selectable output.
  ```

---

## CSV expectations (V1)

Recipes roll from CSV files using simple random line selection.

- Empty lines are skipped
- Lines beginning with `#` or `//` are treated as comments
- No weighting or column parsing is performed in V1

---

## Naming conventions

Recommended patterns:

### Visible generators
- `npc.json`
- `room.json`
- `encounter.json`

### Hidden ingredient tables
- `_egress.csv`
- `_rumors.csv`
- `_gods.csv`

### Icons (future / optional)
- `_icon.bin` (32×32 monochrome bitmap)

---

## Example: Cross-folder room generator

If stored at `/DATA/room/room.json`:

```json
{
  "parts": [
    { "label": "Size", "roll": "size.csv" },
    { "label": "Shape", "roll": "shape.csv" },
    { "label": "Graffiti", "roll": "graffiti.csv", "p": 0.35 },
    { "label": "Trap", "roll": "/traps/traps.csv", "p": 0.20 },
    { "label": "Treasure", "roll": "/items/treasure.csv", "p": 0.25 },
    { "label": "Junk", "roll": "/items/junk.csv", "p": 0.40 }
  ]
}
```

---

## Reserved keys (forward compatibility)

These keys are reserved for future versions and may be included safely:

- `title` (string): display name override
- `type` (string): generator category (e.g. `npc`, `room`)
- `version` (number): recipe schema version

Example:

```json
{
  "title": "NPC Generator",
  "type": "npc",
  "version": 1,
  "parts": [
    { "label": "Name", "roll": "names.csv" }
  ]
}
```

---

## Planned V2 Extensions

- Recipes that invoke other recipes
- Weighted CSV support
- Formatting templates (single-line summaries)
- Grouped / conditional parts

