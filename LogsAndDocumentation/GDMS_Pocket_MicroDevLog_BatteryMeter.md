# Micro Dev Log — Battery Meter Test (RP2040 Adalogger)

**Date:** January 2026  
**Project:** GDMS‑Pocket  
**Focus:** External battery voltage sensing + OLED display validation

---

## Goal

Create a **minimal, isolated test sketch** to validate:
- External LiPo battery voltage sensing via a **100kΩ / 100kΩ voltage divider**
- Stable ADC readings on the **RP2040 Adalogger**
- Correct display of **battery voltage and percentage** on the **1.5” SH1107 SPI OLED**
- Use of the **same OLED library stack (U8g2)** and pin mapping as the main project

This test intentionally excluded:
- SD card
- Buttons
- Buzzer
- UI state machine

The goal was confidence and clarity, not integration.

---

## Hardware Setup

### Battery Meter
- External voltage divider:
  - `BAT → 100kΩ → A0 → 100kΩ → GND`
- A0 reads **VBAT / 2**
- Divider current ≈ **21 µA @ 4.2 V** (acceptable for this device)

### OLED (SPI, Software SPI via U8g2)

OLED header (left → right):
`RST, CS, DC, SDA, SCL, GND, VCC`

Wiring:
- RST → D9  
- CS  → D10  
- DC  → D11  
- SDA → D12  
- SCL → D13  
- VCC → 3.3V  
- GND → GND  

Display driver:
- **SH1107 128×128**
- **U8g2 software SPI constructor**
- Page buffer (`_1_`) for low RAM use

---

## Software Stack

- **Library:** U8g2  
- **Constructor:**  
  `U8G2_SH1107_128X128_1_4W_SW_SPI`
- **ADC:** 12‑bit (`analogReadResolution(12)`)

### Battery Math

- ADC range: `0–4095`
- Reference voltage: `~3.3 V`
- Divider compensation: `×2.0`

Voltage:
```
VBAT = (raw / 4095.0) * 3.3 * 2.0
```

Percentage:
- Clamped range: **3.30 V → 0%**, **4.20 V → 100%**
- Non‑linear curve (`x^1.7`) to better match LiPo discharge behavior
- Exponential moving average to reduce jitter

---

## Test Result

✅ OLED initialized correctly using U8g2 + SW SPI  
✅ Battery voltage displayed correctly (stable, realistic values)  
✅ Percentage mapping behaved sensibly  
✅ No visible flicker or ADC noise after smoothing  
✅ Pin mapping confirmed safe and repeatable  

This validated:
- External divider approach
- Use of **A0** for battery sensing
- Continued use of **U8g2** for all OLED work
- Readiness to integrate battery indicator into main UI

---

## Design Decisions Locked In

- Battery sensing will use **external 100kΩ / 100kΩ divider**
- **A0** reserved exclusively for battery measurement
- OLED remains on **software SPI via U8g2**
- Battery % logic is suitable for header/UI integration
- Roll counter UI element can safely be replaced by battery indicator

---

## Next Steps

- Integrate battery reading into main GDMS‑Pocket UI
- Replace top‑right roll counter with:
  - Battery percentage text, or
  - Small battery icon + percent
- Optionally add:
  - Low‑battery warning threshold
  - Icon blink or chirp behavior

---

**Status:** ✅ Battery meter subsystem validated and ready for integration
