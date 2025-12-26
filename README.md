# GDMS:Pocket
porting of the Goblindoid Dungeon Mastering System for microcontroller-based handheld.


### Component Technical Data

| Component ID | Other IDs          | Sensor Name         | Operational Voltage | Data Type | I²C address | Notes|
|-----------|--------------------|---------------------|---------------------|-----------|-------------|------|
| SSD1306   |                    | 0.96" OLED Display         | 3.3-5 | Digital |0x3C ||
| WWZMDiB   |                    | SD TF Card Adapter Reader Module | 3.3-5 | | NA - SPI | |

### Library Table

| Relevant Device | Library Names | Include call | Author | Desciption |
|-----------------|---------------|--------------|--------|------------|
| IIC devices | Wire | <Wire.h> | Arduino ? | used for IIC work |
| SSD1306 OLED | Adafruit SSD1306 | <Adafruit_SSD1306.h> |  Adafruit | OLED driver library for small screens | 
| SSD1306 OLED | Adafruit GFX library | <Adafruit_GFX.h> | Adafruit | core graphics library for Adafruit displays| 
| SSD1306 OLED | U8g2 |  | olikraus | SSD1306 screen library with smaller RAM footprint for many sensors | 
