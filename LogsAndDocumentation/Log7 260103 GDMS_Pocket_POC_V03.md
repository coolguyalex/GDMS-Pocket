# GDMS-Pocket POC Dev Log (POC_V03)

Date: 2026-01-04  
Target: Adafruit RP2040 Adalogger + 0.96" SSD1306 OLED + 4 buttons + LED + passive buzzer + SD card

## Goal of this POC
Implement the core “random table rolodex” loop described in the README:

- Browse category folders under `/DATA`
- Browse `.csv` tables inside a category
- Pick a table and “roll” a random entry from it (re-rollable)  
【119:3†README.md†L31-L66】

This session focused on building that end-to-end loop (folders → files → random row) plus a set of UI refinements needed for the specific OLED you’re using.

---

## Hardware + Wiring (as used in code)
- OLED: SSD1306 128×64, I²C (0x3C), `Wire` at 400kHz
- Buttons (active LOW w/ `INPUT_PULLUP`)
  - Up: D9
  - Down: D10
  - A: D6 (Open / Select)
  - B: D5 (Back)
- LED: D11 (PWM dim blink on press)
- Passive buzzer: D12 (short tones on press)

(Buttons were explicitly remapped so **A = open/select** and **B = back** to match the intended UX.)

---

## SD data layout (POC assumptions)
- SD has `/DATA` at the root.
- `/DATA/<CATEGORY>/` contains one-level-deep category folders only (no recursion).
- Each category folder contains `.csv` files representing random tables.  
【119:3†README.md†L36-L52】

---

## What’s working now (end-to-end behavior)

### 1) Category browser (MODE_CATS)
- On boot, the system scans `/DATA` and builds an in-memory list of category folder names.
- UI shows a scrollable list with a cursor (">").
- Header uses a smaller font and is positioned so list text stays out of the OLED’s yellow top band.
- Footer/tooltips removed (controls are intuitive).

### 2) File browser (MODE_FILES)
- When a category is opened, the system scans `/DATA/<selectedCat>/` and lists `.csv` files only.
- UI shows scrollable list with cursor.
- Filenames are displayed **without the `.csv` extension** (via `stripCsvExt()`).
- Long names are truncated with ellipses (`ellipsize()`), preventing wrap into a second line.

### 3) Table view + rolling (MODE_TABLE)
- Selecting a file constructs `fullPath = "/DATA/" + selectedCat + "/" + selectedFile`.
- `pickRandomCsvLine()` selects a random non-empty line from the file using **reservoir sampling**.
- Table view displays the selected entry using a simple fixed-width wrap.
- A reroll:
  - Re-runs `pickRandomCsvLine()`
  - Increments `rollCount`
- B goes back to the file list.

### 4) Table scrolling (Up/Down in MODE_TABLE)
- Long generated entries can be scrolled using Up/Down.
- Scroll operates in “wrapped line” units:
  - `countWrappedLines()` estimates wrapped line count using a fixed chars-per-line assumption.
  - `tableScrollLine` is clamped to valid range.
- **Important reset point:** whenever entering MODE_TABLE (selecting a file) we reset:
  - `rollCount = 0`
  - `currentEntry = ""`
  - `tableScrollLine = 0` (so each new table starts at top)

---

## UI changes made for your specific OLED
### Yellow top band avoidance
Your OLED has a yellow pixel band at the top. We adjusted the layout so *all list/table text starts below that band*:

- Reduced header font size (general change: headers are now small font)
- Adjusted header divider and list start `y0` values:
  - You found `y0 = 16` works best on your specific module (keeps content in the blue region).

### List layout / removing overlap
- Previously the bottom row could collide with the footer/tooltips.
- We removed the footer entirely and sized lists so the last list row stays clear of the bottom edge.

### Filename and entry wrapping fixes
- `.csv` extension hidden in file list display (reduces line length)
- Long list items ellipsized to a safe character count
- Table entries wrapped and scrollable

---

## Key implementation notes (so a dev can continue quickly)

### Core state machine
`UiMode`:
- `MODE_CATS`: category list
- `MODE_FILES`: csv list for selected category
- `MODE_TABLE`: currentEntry + reroll + scroll

State variables (high level):
- Category: `categories[]`, `catCount`, `cursor`, `scrollTop`
- Files: `files[]`, `fileCount`, `fileCursor`, `fileScrollTop`
- Table: `selectedCat`, `selectedFile`, `currentEntry`, `rollCount`, `tableScrollLine`

### Random CSV selection
`pickRandomCsvLine(fullPath, outLine)`:
- Reads file line-by-line (fixed buffer)
- Trims CR/LF
- Skips empty lines and comment-ish lines (`#`, `//`)
- Reservoir sampling chooses 1 random line from unknown-length stream (no need to load file into RAM)

### Input + feedback
- Buttons use edge detection + debounce.
- On press:
  - PWM LED blink at a very low brightness
  - Buzzer tone for 30ms (frequency varies per button)

---

## Known issues / TODOs
1) **Header wrap on long table names**
   - Example: `motivations.csv` sometimes renders as `motivation` then `s` (wrap), which can overlap content.
   - Fix: header should always be ellipsized to fit one line (or rendered with text bounds and clipped).

2) **Category screen not using full vertical space (list height)**
   - You noticed categories don’t extend to the bottom as expected.
   - Fix: revisit `rows`, `listY0`, and the available height calculation to ensure consistent use of 64px height on all screens.

3) **CSV “weighted two-column” support not implemented yet**
   - README calls for interpreting 2-column CSV weights. This is not in the current POC loop.  
   【119:3†README.md†L7-L13】

4) **JSON “recipes” chaining not implemented yet**
   - Out of scope for POC_V03, but in the README “additional functionality” list.  
   【119:3†README.md†L7-L13】

---

## Suggested next step (minimal + high value)
**Header ellipsize + single-line guarantee**
- Create `String headerSafe(const String& s)` that:
  - strips `.csv`
  - ellipsizes to a max character count for the header font
- Apply to:
  - file list header (selectedCat)
  - table header (selectedFile)

Then, unify layout constants:
- `HEADER_H`, `CONTENT_Y0`, `ROWS_PER_LIST`
so the category screen and file screen stay consistent.

---
