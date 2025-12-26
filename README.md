# GDMS:Pocket
porting of the Goblindoid Dungeon Mastering System for microcontroller-based handheld.

porting of the Goblindoif Dungeon Mastering System for a handheld based on Arduino


## component details 

RP 2040


### Sensor details
All chage frequencies, domain variabilities, and values are adjustable by code and installation design.

| Data Stream | Sensor | Frequency of Change | Domain of Variability | Likley Values | Notes |
|-------------|--------|---------------------|-----------------------|---------------|-------|
| Capacitive Touch | MPR121 |Frequent | capacitance registers as a number and touching modifies that number | 300's | 12 contacts map to 12 notes in an octave |
| Light | BH1750 | High | Medium | similar to capacitive touch but with very large swing depending on proximity of strength and proximity of light sources | | Lux is a non-linear parameter |
| Temperature | BME280 | Very Low | Very Low | 24+/- 10 | Perhaps create a tube you can blow into ? |
| Humidity |  BME280 |Very Low | Very Low | | | Perhaps create a tube you can blow into ? |
| Pressure |  BME280 | Extremely Low | Extremeley Low | Think of a way to make this change more noticable? |
| Magnetic Field X, Y, & Z | QMC5883L | Very Low | | | 360 degrees of rotation | Signal to noise ratio for detecting nearby electronics is likley too low - providing Magnetic objects or mounting sensor to a moving object will increase utility |
| PIR | HC-SR501 |  High | Binary Data | Binary Data | 
| Buttons | High | Binary Data | Binary Data (active LOW)|
| Potentiometers | High | Full Range |

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
