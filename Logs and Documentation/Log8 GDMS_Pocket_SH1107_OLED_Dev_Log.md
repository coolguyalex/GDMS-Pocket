# GDMS Pocket – Dev Log  
## SH1107 1.5" OLED Bring-Up, Pin Mapping, and Hardware Validation

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
