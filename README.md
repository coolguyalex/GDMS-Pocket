# GDMS:Pocket
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

| Sensor ID | Other IDs          | Sensor Name         | Operational Voltage | Data Type | I²C address | Notes|
|-----------|--------------------|---------------------|---------------------|-----------|-------------|------|
| BH1750    |                    | Light                      | 3–5 | Digital | 0x23 ||
| MPR121    |                    | Capacitive Touch           | 3.3 | Digital |0x5A | May require Soft reset, stop, config, start sequence: write8(0x80, 0x63), write8(0x5E, 0x00), TOUCHTH = 6, RELEASETH = 3, write8(0x5E, 0x8C);|
| BME280    | BMP280             | Temp / Pressure / Humidity | 5   | Digital |0x76 ||
| HW246     | QMC5883L, GY121    | Magnetic Field             | 3–5 | Digital |0x0D ||
| HC-SR501  |                    | Infrared Motion Module     | 3-5 | Digital |N/A  ||
| Buttons   |                    |                            | 3.3-5| Digital| NA ||
| Potentiometers |               |                            | Any | Analog  | NA ||
| SSD1306   |                    | 0.96" OLED Display         | 3.3-5 | Digital |0x3C ||
| WWZMDiB   |                    | SD TF Card Adapter Reader Module | 3.3-5 | | NA - SPI | |

### Library Table

| Relevant Device | Library Names | Include call | Author | Desciption |
|-----------------|---------------|--------------|--------|------------|
| IIC devices | Wire | <Wire.h> | Arduino ? | used for IIC work |
| BME280 Temp, pressure, and humidity sensor | Adafruit_BME280_Library | <Adafruit_BME280.h> | Adafruit | used for the BME280 Temperature, Pressure, and Humnidity sensor |
| Capacitive Touch Sensor | MPR121 | <Adafruit_MPR121.h> | Adafruit | Arduino library for MPR121 cpacitive touch sensors | 
| Adafruit Sensors and Clones | Adafruit Unifed Sensor | <Adafruit_Sensor.h> | Adafruit | Unified library ? | Adafruit Unified Sensor Driver |
| BH1750 Light Sensor | Light Sensor | <BH1750.h> | Christopher Laws | |
| QM5883 Sensors |QMC5883LCompass| <QMC5883LCompass.h> | MPrograms | 3-axis magnetometer library  | 
| SSD1306 OLED | Adafruit SSD1306 | <Adafruit_SSD1306.h> |  Adafruit | OLED driver library for small screens | 
| SSD1306 OLED | Adafruit GFX library | <Adafruit_GFX.h> | Adafruit | core graphics library for Adafruit displays| 
| SSD1306 OLED | U8g2 |  | olikraus | SSD1306 screen library with smaller RAM footprint for many sensors | 
