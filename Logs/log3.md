# GDMS Pocket -- Iteration Log (AVR / Arduino Nano Every)

## Project Context

This iteration focused on porting the Goblinoid Dungeon Mastering System
(GDMS) to an Arduino-class microcontroller with extremely limited SRAM
(\~2 KB). The goal was a handheld Dungeon Master aid that reads random
table entries from text files on an SD card and displays results on a
small OLED screen, with physical buttons and a status LED.

## Hardware Used

\- Arduino Nano Every (AVR-class SRAM constraints)\
- SSD1306 128x64 OLED (I2C)\
- SPI microSD card module\
- Three momentary buttons (LEFT / MIDDLE / X)\
- Green LED on D6 (PWM) with series resistor

## Features Successfully Implemented

\- Reliable SD card initialization and file reading\
- Reservoir sampling to select random lines from large text files
without loading them into RAM\
- OLED display output using U8g2 page-buffer mode\
- Button debounce logic with edge detection\
- LED status indicator with:\
• low-power breathing animation (\~1 second cycle)\
• short brightness \'pop\' on button presses\
- Clean separation between setup() initialization and loop() runtime
behavior

## Key Problems Encountered

1\. \*\*Severe SRAM pressure\*\*\
- Global memory usage reached \~91%, leaving \~180 bytes free\
- Resulted in reset loops, freezes during SD directory scans, and
unstable behavior\
\
2. \*\*Arduino auto-prototype conflicts\*\*\
- Functions using custom structs (DebounceBtn) and SD File types caused
compiler errors\
- Name collisions (isTxtFile, readLine) triggered \'redeclared as
different kind of symbol\' errors\
\
3. \*\*Directory traversal instability\*\*\
- openNextFile() directory scans during setup() caused hangs and resets
under low SRAM\
- Progress/debug buffers worsened stack usage\
\
4. \*\*Misleading memory reporting\*\*\
- Flash usage (\~80--88%) was not the problem\
- SRAM usage was the real limiting factor

## Solutions and Workarounds

\- Renamed SD helper functions (sdIsTxtFile, sdReadLine) to avoid
auto-prototype collisions\
- Reduced global buffer sizes aggressively\
- Moved toward non-blocking LED and button logic\
- Identified directory scanning at boot as a major instability source\
- Confirmed LED breathing stopped when setup() stalled, helping diagnose
hangs

## Features Explicitly Shelved for This Board

\- User-added tables via arbitrary SD folder browsing\
- Recursive directory traversal\
- Dynamic table discovery at boot\
- Rich OLED layouts beyond basic text

## Recommended AVR-Safe Design (If Revisited)

\- Use U8x8 (text-only) instead of U8g2 to reclaim \~150--400 bytes
SRAM\
- Hard-code a small list of table file paths (no directory scanning)\
- Keep line buffers ≤ 21--25 characters\
- Avoid snprintf(), lambdas, and temporary local buffers\
- Treat SD access as a high-risk operation under low SRAM

## Reason for Pausing This Iteration

The remaining SRAM margin on the Nano Every is too small to safely
support SD access, OLED UI, button handling, and future expansion
simultaneously. Continuing to optimize would significantly constrain
functionality and developer time.

## Next Planned Platform

RP2040-based board (e.g., Adafruit Feather RP2040 / Adalogger)\
- \~264 KB SRAM eliminates current constraints\
- Built-in USB, better SD support, and more robust multitasking\
- Allows restoring:\
• user-addable tables\
• directory browsing UI\
• richer screen layouts\
• future chained table generation

## Current Project State

The AVR prototype successfully validated:\
- core SD streaming approach\
- random table selection algorithm\
- LED and button interaction patterns\
\
Development is paused intentionally until migration to RP2040 hardware.
