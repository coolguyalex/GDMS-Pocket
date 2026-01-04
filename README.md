# GDMS:Pocket
Goblindoid Dungeon Mastering System for microcontroller-based handheld.

## Concept: 
Dungeon master's companion designed for ease of use and customization. 
GDMS-pocket is designed to be as ifnoranle as it is helpful - providing that last minute name or encounter when called upon but staying out of the limelight and keeping you engaged with the game.

## future ideas 
- add a little icon to be displayed for each of the pages
- Add an idicator LED (breath for on status), pop for button pushes.
- Add a beep indicator via passive buzzer (different tones for higher or lower results on tables)

## Hardware: 

### Component Table
| Component ID | Other IDs          | Sensor Name         | Operational Voltage | Data Type | I²C address | Notes|
|--------------|--------------------|---------------------|---------------------|-----------|-------------|------|
|RP2040 Adalogger|------------------|---------------------|3.3-5   -------------|-----------|-------------| built in SD - See MCU table in logs and documentation|
|--------------|--------------------|---------------------|---------------------|-----------|-------------|------|
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
| Up | GND | D2 |
| Down | GND | D3 |
| A | GND | D4 |
| B | GND | D4 |

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


## Firmware

### Function Summary:
1. Navigate pages with titles corresponding to folders contained within the root directory.
2. Display the titles of csv files contained within folders as a list on each page.
3. Allow users to select which entry they would like to "Roll" on
4. "Roll" a result by randomly selecting an entry from lists contained within csv files.

### Additional functionality
1. When CSV files contain 2 columns, column 1 is used to provide weights to different results. Otherwise entries have equal weights.
2. Interpret JSON files containing "recipes" which chain csv files together to create more complex generators.
3. Display the titiles of JSONS along side simple csv files (such as in item 2).
4. Allow users to save up to 10 generated items to a "Saved" page. Each saved entry is saved to the SD card and thus creates a new entry in a list.
5. Allow users to delete saved entries on the "Saved" page


### Folder and Data Structure
- Data directory: Folders are all held in a main directory named: "Data"
- Page directory: Each folder's name within the data directory will be used to generate the titles at the top of the pages in the interface. e.g. "names", "dice", "encounters". 


### Library Table
| Relevant Device | Library Names | Include call | Author | Desciption |
|-----------------|---------------|--------------|--------|------------|
| IIC devices | Wire | <Wire.h> | Arduino ? | used for IIC work |
| SSD1306 OLED | Adafruit SSD1306 | <Adafruit_SSD1306.h> |  Adafruit | OLED driver library for small screens | 
| SSD1306 OLED | Adafruit GFX library | <Adafruit_GFX.h> | Adafruit | core graphics library for Adafruit displays| 
| SSD1306 OLED | U8g2 |  | olikraus | SSD1306 screen library with smaller RAM footprint for many sensors | 


## AI generated Description

Functional Specification: SD-Based Random Table System (POC)

### Overview
The system is a microcontroller-based Dungeon Master aid that reads plain text data from an SD card and displays randomly selected entries on a small screen. All interaction is driven by a simple category-based interface.

### Data Layout
- The SD card contains a top-level directory named /DATA.
- Inside /DATA, users may create one-level-deep category folders.
    - Folder names define category/page names in the UI.
    - No nested folders beyond this level are supported.
- Each category folder contains .txt files only.
- Each .txt file represents a random table with one entry per line.

Example:
    /DATA
        /NPC
            names.txt
            flaws.txt
        /DICE
            d20.txt
            d100.txt

### Startup / Discovery
- On boot (or user-triggered rescan), the system:
    1. Enumerates all immediate subfolders of /DATA.
    2. Stores their names as available Categories.
- No recursive directory traversal is performed beyond this level.

### UI Behavior
- Each Category corresponds to one folder under /DATA.
- When a Category is selected:
    - The system lists all .txt files in that folder.
- The user scrolls and selects a file.
- Upon selection:
    - The system reads the file and displays one randomly selected line.

### Random Selection
- Files are processed in a memory-efficient manner.
- The system should avoid loading entire files into RAM.
- Random selection is done by streaming line-by-line and selecting an entry using a lightweight algorithm.

### Constraints
- Target hardware is Arduino-class with limited RAM.
- SD access is via SPI.
- The system favors predictable behavior, low memory usage, and simple control flow.

- All content is read-only; no file modification or persistence is required.

