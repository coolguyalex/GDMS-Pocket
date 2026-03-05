# GDMS:Pocket — Getting Started Guide

**Goblinoid Dungeon Mastering System**  
*A handheld random table roller for tabletop RPGs*

---

## What Is This?

GDMS:Pocket is a small handheld device that rolls on random tables for you at the table. Press a button, get a result. It runs entirely from a microSD card loaded with plain text files — no internet, no app, no phone required.

Out of the box it comes loaded with a set of tables for names, encounters, rooms, loot, and more. Everything on the card can be replaced or added to using any computer and a text editor.

---

## The Buttons

The device has four buttons. Their functions depend on which screen you're on.

```
          [ UP ]         [ B ]

                       

         [ DOWN ]     [ A ]          
```

| Button | What it does |
|--------|--------------|
| **UP / DOWN** | Move the cursor up or down through a list. Hold for fast scrolling. |
| **A** | Select the highlighted item, or re-roll a result you're already looking at. |
| **B** | Go back to the previous screen. |
| **A + B together** | Open the options menu from anywhere. |

---

## The Screens

### Category List (home screen)

When the device boots you'll see a list of categories — Names, Encounters, Rooms, and so on. These correspond to folders on the SD card. Scroll with UP/DOWN and press A to enter a category.

### File List

Inside a category you'll see a list of generators. Some are simple tables (single rolls), others are recipes that combine several tables into one result. Scroll and press A to roll.

### Result View

The rolled result is displayed here. If it's longer than the screen, scroll with UP/DOWN to read the rest. Press A to roll again on the same generator. Press B to go back to the file list.

### Options Menu

Hold A and B simultaneously from any screen to open options. Scroll with UP/DOWN, press A to cycle a setting's value. Press B to close. Settings are saved automatically and reload next time you power on.

| Setting | What it controls |
|---------|-----------------|
| Buzzer | Turn audio feedback on or off |
| Buz Vol | Loud or soft beeps |
| LED | Turn the indicator LED on or off |
| LED Pop | Flash the LED on button presses |
| LED Brt | LED brightness — Lo, Med, or Hi |
| Breathe | Speed of the LED breathing animation |
| Sleep | How long before the display turns off automatically. Press any button to wake it. |

### About Screen

Accessible from the bottom of the options menu. Shows the control reference and device information.

---

## The Battery Indicator

The small icon in the top-right corner of every screen shows the battery level.

| Icon | Meaning |
|------|---------|
| Three bars | Full — plenty of charge |
| Two bars | OK — no rush |
| One bar | Low — charge soon |
| Empty, flashing | Critical — charge now |

The device runs on a rechargeable LiPo battery. Charge it using a USB-C cable. The small orange LED near the USB port will light while charging and go out when full.

---

## What's on the SD Card

The microSD card holds all the content the device displays. To access it, power the device off and remove the card from the slot on the underside of the board. Insert it into your computer using a card reader.

The card is organised like this:

```
/DATA/
  /names/
      fantasy-names.csv
      tavern-names.csv
  /encounters/
      wilderness.csv
      dungeon.csv
      npc.json
  /rooms/
      room.json
      _features.csv
      _hazards.csv
```

Each folder inside `/DATA` becomes a category on the device. The folder's name is what you see on screen. Files inside the folder are the generators you can roll on.

---

## Removing Default Content

To remove a category entirely, delete its folder from the card. To remove a single generator, delete the `.csv` or `.json` file. The device will not show entries that don't exist — there's nothing else to update or configure.

If you want to keep a file on the card but hide it from the device's UI (for example, a support table used by a recipe but not useful to roll on directly), rename it so it starts with an underscore: `_myfile.csv`. It will be invisible to the interface but still accessible internally.

---

## Adding Your Own Content

### Simple Table (CSV)

Create a plain text file with a `.csv` extension. Put it inside a category folder under `/DATA`. Each line becomes one possible result. That's it.

**Example — `/DATA/names/pirate-names.csv`:**

```
Barnacle Pete
One-Eyed Sal
The Drowned Captain
Mira of the Tides
Deckhand Corrigan
```

Any line editor will do — Notepad on Windows, TextEdit on Mac (save as plain text, not rich text), or any code editor.

**Rules for CSV files:**
- One result per line
- Blank lines are skipped automatically
- Lines starting with `#` or `//` are treated as comments and skipped
- File encoding should be UTF-8 (the default on all modern systems)

### Weighted Table

If you want some results to come up more often than others, add a number at the start of each line followed by a comma. Higher numbers mean higher probability.

```
3,Common sword
3,Shortbow
2,Crossbow
1,Hand axe
1,War hammer
```

In this example a common sword or shortbow is three times more likely than a war hammer. You can mix weighted and unweighted files freely — any line without a leading number is treated as weight 1.

### New Category

Create a new folder inside `/DATA` and add `.csv` or `.json` files inside it. The folder name is what appears on the device. Keep names short — the display is small and long names will be automatically truncated with `...`.

**Tips for folder names:**
- Aim for 12 characters or fewer for clean display
- Use hyphens instead of spaces if you need multi-word names (`wild-magic` not `wild magic`)
- Avoid special characters

---

## JSON Recipes (Advanced)

A recipe is a `.json` file that generates a multi-part result by chaining rolls from multiple CSV tables. This is what makes a generator that produces a full NPC profile — name, job, personality trait, secret — all in one roll.

### Basic Recipe Structure

```json
{
  "parts": [
    { "label": "Name",  "roll": "names.csv" },
    { "label": "Job",   "roll": "jobs.csv" },
    { "label": "Trait", "roll": "_traits.csv", "p": 0.75 }
  ]
}
```

Save this as `npc.json` inside a category folder. When selected, the device rolls each part in order and displays the combined result.

### Recipe Fields

**`roll`** — the CSV file to sample from. Can be a filename relative to the recipe's folder, or a path from `/DATA` for cross-category references:

```json
{ "roll": "names.csv" }
{ "roll": "/items/treasure.csv" }
```

**`label`** — optional text shown before the result on that line:

```
Name: Aldric Vane
Job: Gravedigger
```

**`p`** — optional probability from 0.0 to 1.0 that this part appears at all. Useful for optional details that shouldn't show up every time:

```json
{ "label": "Secret", "roll": "secrets.csv", "p": 0.40 }
```

A `p` of 0.40 means this line appears roughly 40% of rolls.

### Example: Full NPC Generator

File: `/DATA/npcs/npc.json`

```json
{
  "parts": [
    { "label": "Name",       "roll": "_names.csv" },
    { "label": "Occupation", "roll": "_jobs.csv" },
    { "label": "Trait",      "roll": "_traits.csv" },
    { "label": "Wants",      "roll": "_wants.csv",  "p": 0.8 },
    { "label": "Secret",     "roll": "_secrets.csv", "p": 0.5 }
  ]
}
```

The ingredient tables (`_names.csv`, `_jobs.csv`, etc.) are named with a leading underscore so they don't clutter the file list — only `npc.json` appears to the user.

### Format String (Optional)

For a single-line output style rather than labelled lines, add a `format` key using `{label}` placeholders:

```json
{
  "format": "{Name}, {Occupation} — {Trait}",
  "parts": [
    { "label": "Name",       "roll": "_names.csv" },
    { "label": "Occupation", "roll": "_jobs.csv" },
    { "label": "Trait",      "roll": "_traits.csv" }
  ]
}
```

This produces output like:

```
Aldric Vane, Gravedigger — Speaks only in proverbs
```

---

## Troubleshooting

**The device doesn't turn on.**  
Check that the battery is charged. Connect via USB-C and look for the orange charging LED near the port. If it lights, the battery was flat — give it 30–60 minutes before trying again.

**The screen is blank but the LED is breathing.**  
The device has entered sleep mode. Press any button to wake it. If you'd prefer a longer timeout or no timeout at all, adjust the Sleep setting in the options menu (A+B).

**A category or file I added isn't showing up.**  
Make sure the folder is directly inside `/DATA` with no intermediate folders. Make sure the file has a `.csv` or `.json` extension (not `.csv.txt`, which some systems add by default). If the filename starts with `_` it will be deliberately hidden — rename it without the underscore.

**The display shows "SD fail, retry..." on boot.**  
The SD card isn't being read. Re-seat the card and power cycle. If the problem persists, try reformatting the card as FAT32 on your computer and copying the content back.

**Results look garbled or CSV entries aren't rolling correctly.**  
Open the file in a plain text editor and check that it saved as UTF-8 without a BOM (Byte Order Mark). Some editors on Windows default to UTF-16 or add a BOM, which breaks line parsing. In Notepad, use File → Save As → Encoding: UTF-8.

**A JSON recipe shows "No selectable output."**  
This usually means one of the CSV files referenced in the recipe's `roll` paths couldn't be found. Check that the paths are correct, including capitalisation — the SD card filesystem is case-sensitive. Check that any cross-category paths start with `/` and reference folders that actually exist under `/DATA`.

---

## Tips

- Category and file names are displayed without their extensions. Name your files descriptively — `dungeon-rooms.csv` shows as `dungeon-rooms` on screen.
- You can have multiple recipes in the same category, each combining tables in different ways.
- Tables can be shared across categories using cross-category paths in recipes. A single `names.csv` can serve NPC generators, tavern generators, and ship generators all at once.
- The device saves your options settings automatically. You don't need to do anything special — just change a setting in the menu and it's stored.
- If you're playtesting new tables, add them to the card and roll through them to check for formatting issues, weird entries, or lines that are too long for the screen before your session.

---

*GDMS:Pocket by Alexander Sousa, 2026*
