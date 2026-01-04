# GDMS:Pocket
Goblindoid Dungeon Mastering System for microcontroller-based handheld.

## Concept: 
Dungeon master's companion designed for ease of use and customization. 
GDMS-pocket is designed to be as ifnoranle as it is helpful - providing that last minute name or encounter when called upon but staying out of the limelight and keeping you engaged with the game.

## future ideas 
- add a little icon to be displayed for each of  the pages

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
| Button | Program Name | Right Pin | Left Pin| 
|---|---|---|---|
| left | B | GND | D2 |
| middle | A | GND | D3 |
| eight | X | GND | D4 |

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

### Function Summary:
GDMS performs two major functions:
1. Navigates the file tree.
2. Pulls a random entry from the txt files.
Some of this functionality will need to be dyanmic in order to incorporate new user-uplaoded data.

### Data 
GDMS will only contain txt files arranged in a specific folder structure composed of a main directroy, page directory,  and text files containing lists of entries contained in the page directory. 

### Folder Structure
- Main directory: Folders are all held in a main directory: "Data"
- Page directory: Each folder's name within the main directory will be used to generate the titles at the top of the pages in the interface. e.g. "names", "dice", "encounters". 
- The function directory contains txt files containing a list of entries. 

### Functionality 
- All user-facing data is held within the data folder. UI should begin within the data folder.
- GDMS will dynamically assign the title of a page based on the names of folders held in the pages directory. e.g. "names", "dice", "encounters". 
- Pages will contain eith er a list of tables which may be rolled or offer a single table to be rolled 
    - users may advance to the next the page by hitting the A buttons and go page to the last page with the B button. 
- Each page has two display possibilities:
    - If the page directory only contains one text file then the page should simple allow to roll the relevant table by hitting the x button
    - If the page directory contains further folders the page should display a list which the user may begin to navigate by hitting the x button 
        - The may then switch selection by hitting the A button and chose which random table they want to roll by hitting the X button. Pressing the B button goes back a step


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