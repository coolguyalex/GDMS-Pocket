# GDMS:Pocket -- Incremental SD + OLED Development Report 251216

This document summarizes the step-by-step development process used to
implement a low-RAM, microcontroller-based Dungeon Master table roller
using an SSD1306 OLED display, an SPI SD card module, and physical
buttons. The goal of this report is to allow future-you or another
developer to reproduce the same working system and understand the
reasoning behind each technical decision.

## Target Platform

- Arduino-class MCU (Uno / Nano / Nano Every)
- Severe RAM constraints (~2 KB SRAM)
- SPI SD card reader module (with onboard regulator)
- SSD1306 OLED via I²C
- Momentary buttons wired to GND (INPUT_PULLUP)

## Key Constraints Identified Early

1. RAM is the primary limiting factor.
2. SD card access causes brief current spikes.
3. Many SD modules do NOT operate correctly when powered from 3.3V
despite claims.
4. Arduino SD.h uses FAT 8.3 short filenames only.
5. Directory iteration requires explicit rewindDirectory() calls.

## Verified Hardware Findings

- SSD1306 OLED works reliably at both 3.3V and 5V.
- SD module reliably initializes only when powered from 5V.
- SD activity can cause visible LED flicker due to rail sag.
- SD card access must be minimized during runtime.

## File System Rules Adopted

- All folders live at SD root.
- Folder and file names must be FAT 8.3 compliant.
- Example structure:

        /DICE
            /D20.TXT
            /D100.TXT

## Development Phases

### Phase 1: OLED Bring-Up

- Tested OLED independently.
- Confirmed rotation using U8G2_R2.
- Chose U8g2 page buffer (_1_) to conserve RAM.

### Phase 2: SD Initialization

- Verified SD.begin() stability.
- Identified 5V requirement for SD module.
- Avoided SPI reconfiguration once stable.

### Phase 3: Root Directory Enumeration

- Successfully listed root folders.
- Observed 8.3 short-name behavior (ENCOUN~1, etc.).
- Learned that renaming folders does not always remove aliasing.

### Phase 4: Folder Traversal

- Opened /DICE folder directly.
- Discovered openNextFile() returns no entries without
rewindDirectory().
- Confirmed correct file count after rewind.

### Phase 5: File Reading

- Implemented line-by-line file reading.
- Avoided String objects entirely.
- Read only first line initially to validate path correctness.

### Phase 6: Random Line Selection

- Implemented reservoir sampling for uniform randomness.
- Required only constant memory regardless of file size.
- Proven reliable with large tables.

### Phase 7: Button Integration

- X button (D4) triggers a reroll.
- Implemented debounce and edge detection.
- SD access occurs only on button press.

## Final System Behavior

- Device boots to "Ready" state.
- Pressing X opens a table file and rolls a random entry.
- Result is displayed on OLED.
- System remains stable despite low SRAM headroom.

## Design Principles Established

- Always test one subsystem at a time.
- Reduce RAM usage before adding features.
- Stream data; never load full files.
- Trust empirical hardware behavior over datasheets.

## Next Logical Extensions

- Add A/B buttons for file selection.
- Cache filenames per folder.
- Generalize folder selection (pages).
- Optional: migrate to RP2040 or ESP32 for more RAM.
