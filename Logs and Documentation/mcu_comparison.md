# MCU Comparison Table

| MCU / Board | Release (approx) | MCU | CPU / Clock | SRAM | Flash | Wireless | Typical Notes |
|------------|------------------|-----|-------------|------|-------|----------|---------------|
| Elegoo Nano (Arduino Nano clone) | ~2016 (clone era) | ATmega328P | 8-bit AVR @ 16 MHz | 2 KB | 32 KB | No | Very limited RAM, tight for SD + display, good for learning basics |
| Arduino Nano Every | 2019 | ATmega4809 | 8-bit AVR @ 20 MHz | 6 KB | 48 KB | No | Slightly more RAM than Nano, still constrained but more usable |
| Arduino Uno R4 WiFi | 2023 | Renesas RA4M1 (Cortex-M4F) + ESP32-S3 | 48 MHz (RA4M1) | 32 KB | 256 KB | Yes (ESP32-S3) | Modern MCU, good peripherals, wireless adds complexity |
| SAMD21 (M0+) | ~2014 | Cortex-M0+ | 48 MHz | 32 KB | up to 256 KB | No | Low power, limited RAM for UI-heavy work |
| RP2040 | 2021 | Dual Cortex-M0+ | 133 MHz | 264 KB | External QSPI | No | Excellent RAM for SD/UI, weaker deep sleep |
| ESP32 (original) | 2016 | Xtensa LX6 | up to 240 MHz | ~520 KB | External (module) | Yes | Powerful, good sleep, WiFi can dominate power |
| STM32F4 (example) | ~2011 | Cortex-M4F | up to 168 MHz | up to 192 KB | up to 1 MB | No | Powerful but older and less power-efficient |

