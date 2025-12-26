# GDMS:Pocket
porting of the Goblindoid Dungeon Mastering System for microcontroller-based handheld.

porting of the Goblindoid Dungeon Mastering System for a handheld based on Arduino

## Concept: 
I want to build a dungeon master's companion that effectively dives into folders until it hits txt files and then randomly picks entries from list. It is basically a random table holder and roller.  

## Hardware: 

### Component Table
| Component ID | Other IDs          | Sensor Name         | Operational Voltage | Data Type | I²C address | Notes|
|-----------|--------------------|---------------------|---------------------|-----------|-------------|------|
| SSD1306   |                    | 0.96" OLED Display         | 3.3-5 | Digital |0x3C ||
| WWZMDiB   |                    | SD TF Card Adapter Reader Module | 3.3-5 | | NA - SPI | |


### future components 
- RP 2040 
- ESP32
- SHARP memory LCD 200 x 400

### Wiring: 

#### Buttons
| Button | Right Pin | Left Pin| 
|---|---|---|
| left | GND | D2 |
| middle | GND | D3 |
| eight | GND | D4 |

#### OLED SSD1306 

| SSD1306 Pin | Board Pin |
|-------------|-----------|
| GND | GND |
| VCC | 3v3 |
| SCL | A5 |
| SDA | A4 |

#### WWZMDiB SD Card reader
| SSD1306 Pin SD Card Reader | Board Pin |
|-------------|-----------|
| CS          | D5        |
| SCK         | D13       |
| MOSI        | D11       |
| MISO        | D12       |
| VCC         | 3v3       |
| GND         | GND       |


## Software

### Files structure and Functionality:
I have a screenshot of the folder tree in visual studio code. 

The basic idea is that the program performs two major functions
1. it navigates the file tree and 
2. it pulls a random entry from the txt files. 


### Library Table

| Relevant Device | Library Names | Include call | Author | Desciption |
|-----------------|---------------|--------------|--------|------------|
| IIC devices | Wire | <Wire.h> | Arduino ? | used for IIC work |
| SSD1306 OLED | Adafruit SSD1306 | <Adafruit_SSD1306.h> |  Adafruit | OLED driver library for small screens | 
| SSD1306 OLED | Adafruit GFX library | <Adafruit_GFX.h> | Adafruit | core graphics library for Adafruit displays| 
| SSD1306 OLED | U8g2 |  | olikraus | SSD1306 screen library with smaller RAM footprint for many sensors | 
