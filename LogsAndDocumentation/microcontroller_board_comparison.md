# Microcontroller Board Comparison (Hobbyist-Focused)

*Prices are approximate single-board retail. Community size is a qualitative estimate based on forums, tutorials, libraries, and third‑party projects.*

| Board | MCU | CPU / Speed | RAM (SRAM) | Flash | Connectivity | Typical Price (USD) | Community Size | Notable Features / Gotchas |
|---|---|---:|---:|---:|---|---:|---|---|
| Raspberry Pi Pico | RP2040 | Dual-core Cortex-M0+ @ 133 MHz | 264 KB | 2 MB QSPI | USB | ~$4 | **Very Large** | PIO is extremely powerful; massive docs and examples |
| Raspberry Pi Pico W | RP2040 + CYW43439 | Dual-core @ 133 MHz | 264 KB | 2 MB | Wi‑Fi | ~$6 | **Very Large** | Wireless Pico without ESP32 complexity |
| Seeed XIAO RP2040 | RP2040 | Dual-core @ 133 MHz | 264 KB | 2 MB | USB | $5–$10 | **Large** | Ultra‑small form factor, strong Seeed ecosystem |
| Adafruit Feather RP2040 | RP2040 | Dual-core @ 133 MHz | 264 KB | 4–8 MB (varies) | USB | $10–$15 | **Very Large** | Feather ecosystem, LiPo charging, polished UX |
| Adafruit Feather RP2040 Adalogger | RP2040 | Dual-core @ 133 MHz | 264 KB | 8 MB + microSD | USB | $14.95 | **Very Large** | Built‑in SD + datalogging focus |
| Elegoo Nano (Nano V3 clone) | ATmega328P | 8‑bit AVR @ 16 MHz | 2 KB | 32 KB | USB | Very cheap | **Huge (Legacy)** | Arduino classic; RAM is extremely tight |
| Arduino Nano Every | ATmega4809 | AVR @ 20 MHz | 6 KB | 48 KB | USB | ~$10 | **Large** | Still 5V; more headroom than 328P |
| Arduino Nano R4 | Renesas RA4M1 (Cortex‑M4) | 48 MHz | 32 KB | 256 KB | USB | $12 | **Growing** | Modern Nano, 32‑bit, strong Arduino backing |
| ESP32 (WROOM‑32) | ESP32 (Xtensa LX6) | Dual-core up to 240 MHz | 520 KB | External SPI (4 MB common) | Wi‑Fi + BT | $5–$10 | **Huge** | Wireless king; more software complexity |
| ESP32‑S3 Dev Board | ESP32‑S3 | Dual-core up to 240 MHz | On‑chip + PSRAM (varies) | External SPI | Wi‑Fi + BLE | $7–$15 | **Very Large** | Native USB, AI/graphics friendly |
| STM32 “Blue Pill” | STM32F103 | Cortex‑M3 @ 72 MHz | 20 KB | 64 KB | USB | $2–$5 | **Medium** | Clone quality varies; toolchains less friendly |
| STM32 Nucleo‑F401RE | STM32F401RE | Cortex‑M4 @ 84 MHz | 96 KB | 512 KB | USB | ~$15 | **Large (Pro‑leaning)** | On‑board debugger; industry‑style workflows |
| Teensy 4.0 | i.MX RT1062 | Cortex‑M7 @ 600 MHz | 1 MB | 2 MB | USB | ~$16 | **Large (Advanced)** | Insane performance; great audio/DSP support |

## Community Size Key
- **Huge**: Arduino / ESP32 scale, thousands of tutorials and libraries
- **Very Large**: RP2040 + Adafruit ecosystems
- **Large**: Active forums, regular new projects
- **Growing**: Strong backing but newer ecosystem
- **Medium**: Knowledgeable users, fewer beginner guides
