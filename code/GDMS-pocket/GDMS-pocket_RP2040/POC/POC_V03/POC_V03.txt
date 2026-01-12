#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <SPI.h>
#include "SdFat.h"

// Code written by Alexander Sousa and ChatGPT January 2026


// =================== PINS (from your FullSuiteTest) ===================
const uint8_t LED_PIN    = 11;   // external LED on D11
const uint8_t BUZZER_PIN = 12;   // passive buzzer on D12

// Buttons per your README wiring:
// Up=D9, Down=D10, A=D5 (back), B=D6 (select) :contentReference[oaicite:5]{index=5}
enum BtnId : uint8_t { BTN_A=0, BTN_B=1, BTN_UP=2, BTN_DOWN=3 };
const uint8_t BTN_PINS[4]  = {6, 5, 9, 10};
const char*   BTN_NAMES[4] = {"A(D5)", "B(D6)", "UP(D9)", "DN(D10)"};

// =================== OLED ===================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =================== LED DIM ===================
const uint8_t LED_DIM = 2; // 0–255

// =================== BUTTONS (debounce + edge detect) ===================
bool wasPressed[4] = {false, false, false, false};
uint32_t lastPressMs[4] = {0, 0, 0, 0};
const uint16_t DEBOUNCE_MS = 40;

// =================== NON-BLOCKING LED BLINK ===================
bool ledBlinkOn = false;
uint32_t ledOffMs = 0;

// =================== NON-BLOCKING BUZZER ===================
bool buzOn = false;
uint32_t buzOffMs = 0;

void buzzerStart(uint16_t freq, uint16_t durMs) {
  tone(BUZZER_PIN, freq);
  buzOn = true;
  buzOffMs = millis() + durMs;
}
void buzzerUpdate(uint32_t now) {
  if (buzOn && (int32_t)(now - buzOffMs) >= 0) {
    noTone(BUZZER_PIN);
    buzOn = false;
  }
}

// =================== SD (Adafruit RP2040 Adalogger) ===================
// Adafruit guide example uses SD CS = 23 and SPI1 config. :contentReference[oaicite:6]{index=6}
#define SD_CS_PIN 23
SdFat SD;
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);

// =================== APP STATE ===================
static const uint8_t MAX_CATS = 40;
String categories[MAX_CATS];
uint8_t catCount = 0;

int16_t cursor = 0;     // which category is selected
int16_t scrollTop = 0;  // first visible row

// =================== OLED THROTTLE POLICY ===================
uint32_t lastInputMs = 0;
uint32_t lastOledMs = 0;
const uint16_t OLED_PERIOD_MS = 120;      // UI feels better a bit faster than 300ms for scrolling
const uint16_t OLED_IDLE_AFTER_MS = 0;    // allow immediate redraw after input for UI

// =================== FILE LIST STATE ===================
static const uint8_t MAX_FILES = 60;
String files[MAX_FILES];
uint8_t fileCount = 0;

int16_t fileCursor = 0;
int16_t fileScrollTop = 0;

// Replace MODE_SELECTED with MODE_FILES
enum UiMode : uint8_t { MODE_CATS = 0, MODE_FILES = 1, MODE_TABLE = 2 };

UiMode mode = MODE_CATS;

String selectedCat;
String selectedFile; // placeholder for next step

String currentEntry;     // the chosen CSV row (raw line)
uint16_t rollCount = 0;  // how many rerolls in this table view

int16_t tableScrollLine = 0;   // which wrapped line to start drawing from


// ---------- helpers ----------
void drawHeader(const char* title, int currentIndexZeroBased = -1, int totalCount = -1) {
  display.setTextColor(SSD1306_WHITE);

  // Header text (small font)
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);

  // Optional counter (top-right)
  if (totalCount > 0 && currentIndexZeroBased >= 0) {
    char buf[12];
    int pos = currentIndexZeroBased + 1;
    if (pos < 1) pos = 1;
    if (pos > totalCount) pos = totalCount;
    snprintf(buf, sizeof(buf), "%d/%d", pos, totalCount);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(128 - (int)w, 0);
    display.print(buf);
  }

  // Divider just under header
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
}

// Draw footer line + left hints + right counter "i/n"
void drawFooterWithCounter(const char* hints, int currentIndexZeroBased, int totalCount) {
  display.drawLine(0, 52, 127, 52, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(hints);

  // Right aligned counter
  char buf[12];
  if (totalCount <= 0) {
    snprintf(buf, sizeof(buf), "-/-");
  } else {
    // show 1-based position
    int pos = currentIndexZeroBased + 1;
    if (pos < 1) pos = 1;
    if (pos > totalCount) pos = totalCount;
    snprintf(buf, sizeof(buf), "%d/%d", pos, totalCount);
  }

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(128 - (int)w, 55);
  display.print(buf);
}

// Simple case-insensitive ".csv" check
bool endsWithCsv(const String& s) {
  if (s.length() < 4) return false;
  String tail = s.substring(s.length() - 4);
  tail.toLowerCase();
  return tail == ".csv";
}

String stripCsvExt(const String& s) {
  if (s.length() >= 4) {
    String tail = s.substring(s.length() - 4);
    tail.toLowerCase();
    if (tail == ".csv") return s.substring(0, s.length() - 4);
  }
  return s;
}

// Truncate to fit a max character count. Adds "..." if truncated.
// For size-1 font on 128px wide screen, ~21 chars fits with our "> " prefix.
String ellipsize(const String& s, uint8_t maxChars) {
  if (s.length() <= maxChars) return s;
  if (maxChars <= 3) return s.substring(0, maxChars);
  return s.substring(0, maxChars - 3) + "...";
}

// Count wrapped lines needed for a string at a given width (in chars).
uint16_t countWrappedLines(const String& s, uint8_t charsPerLine) {
  if (charsPerLine == 0) return 0;
  uint16_t lines = 1;
  uint8_t col = 0;

  for (uint16_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\n') { lines++; col = 0; continue; }
    col++;
    if (col >= charsPerLine) { lines++; col = 0; }
  }
  return lines;
}


// Scan selected folder for CSV files: /DATA/<selectedCat>
void scanCsvFilesForSelectedCategory() {
  fileCount = 0;
  fileCursor = 0;
  fileScrollTop = 0;

  String path = String("/DATA/") + selectedCat;

  if (!SD.exists(path.c_str())) {
    Serial.print("Folder missing: ");
    Serial.println(path);
    return;
  }

  FsFile dir = SD.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    Serial.print("Not a directory: ");
    Serial.println(path);
    return;
  }

  FsFile entry;
  while (fileCount < MAX_FILES && (entry = dir.openNextFile())) {
    if (!entry.isDirectory()) {
      char nameBuf[64];
      entry.getName(nameBuf, sizeof(nameBuf));
      String fn = String(nameBuf);
      if (endsWithCsv(fn)) {
        files[fileCount++] = fn;
      }
    }
    entry.close();
  }
  dir.close();

  Serial.print("CSV files in ");
  Serial.print(path);
  Serial.print(": ");
  Serial.println(fileCount);
  for (uint8_t i = 0; i < fileCount; i++) {
    Serial.print("  - ");
    Serial.println(files[i]);
  }
}

bool pickRandomCsvLine(const String& fullPath, String& outLine) {
  FsFile f = SD.open(fullPath.c_str(), O_RDONLY);
  if (!f) {
    Serial.print("Open failed: ");
    Serial.println(fullPath);
    return false;
  }

  // Seed RNG from a little timing jitter (good enough for this)
  randomSeed(micros());

  // We'll read line-by-line into a fixed buffer
  const size_t BUF_SZ = 180; // keep modest to avoid RAM spikes
  char buf[BUF_SZ];

  uint32_t seen = 0;
  bool chosen = false;
  outLine = "";

  while (true) {
    int n = f.fgets(buf, (int)BUF_SZ);
    if (n <= 0) break;               // EOF

    // Trim CR/LF
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
      buf[--n] = '\0';
    }

    // Skip empty lines
    if (n == 0) continue;

    // Skip comment-ish lines (optional, harmless if you don’t use them)
    if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/')) continue;

    // Reservoir sampling: choose this line with probability 1/seen
    seen++;
    if (random((long)seen) == 0) {
      outLine = String(buf);
      chosen = true;
    }
  }

  f.close();
  return chosen;
}



// Render category list (folders under /DATA)
void drawCategories() {
  display.clearDisplay();
  drawHeader("GDMS-pocket", cursor, catCount);


  if (catCount == 0) {
    display.setTextSize(2);
    display.setCursor(0, 22);
    display.print("No folders found.");
    display.setCursor(0, 32);
    display.print("Need /DATA/<FOLDER>");
    display.display();
    return;
  }

  // Layout:
  // Header ends at y=17 (divider). List starts at y=20.
  // Footer lives at y=54..63.
  const int listY0 = 16;
  const int rowH   = 8;     // size-1 font is 8px tall
  const int rows   = 6;     // 4 rows => 20..51, footer at 54 is clear

  // Keep cursor in bounds
  if (cursor < 0) cursor = 0;
  if (cursor >= (int)catCount) cursor = catCount - 1;

  // Keep cursor visible
  if (cursor < scrollTop) scrollTop = cursor;
  if (cursor >= scrollTop + rows) scrollTop = cursor - rows + 1;

  display.setTextSize(1);

  for (int r = 0; r < rows; r++) {
    int idx = scrollTop + r;
    if (idx >= (int)catCount) break;

    int y = listY0 + r * rowH;
    display.setCursor(0, y);

    if (idx == cursor) display.print("> ");
    else               display.print("  ");

    String name = categories[idx];
    name = ellipsize(name, 19); // 19 chars + "> " fits well
    display.print(name);

  }

// drawFooterWithCounter("A:open  B:back", cursor, catCount);
display.display();
 

}

void drawFiles() {
  display.clearDisplay();

  // Header shows folder name
  drawHeader(selectedCat.c_str(), fileCursor, fileCount);

  display.setTextSize(1);

  if (fileCount == 0) {
    display.setCursor(0, 22);
    display.print("No .csv files.");
    //drawFooterWithCounter("B:back", 0, 0);
    display.display();
    return;
  }

  const int listY0 = 16;
  const int rowH   = 8;
  const int rows   = 6;

  // Clamp cursor
  if (fileCursor < 0) fileCursor = 0;
  if (fileCursor >= (int)fileCount) fileCursor = fileCount - 1;

  // Keep visible
  if (fileCursor < fileScrollTop) fileScrollTop = fileCursor;
  if (fileCursor >= fileScrollTop + rows) fileScrollTop = fileCursor - rows + 1;

  for (int r = 0; r < rows; r++) {
    int idx = fileScrollTop + r;
    if (idx >= (int)fileCount) break;

    int y = listY0 + r * rowH;
    display.setCursor(0, y);

    if (idx == fileCursor) display.print("> ");
    else                   display.print("  ");

    String shown = stripCsvExt(files[idx]);   // hide .csv
    shown = ellipsize(shown, 19);
    display.print(shown);

  }

  // A=select is a placeholder for next step
  //drawFooterWithCounter("A:select  B:back", fileCursor, fileCount);
  display.display();
}

void drawTable() {
  display.clearDisplay();

  // Header: show file name without .csv + roll counter
  String hdr = stripCsvExt(selectedFile);
hdr = ellipsize(hdr, 21);  // <= 9 chars is reliably safe at size 2
drawHeader(hdr.c_str(), (rollCount > 0 ? rollCount - 1 : 0), (rollCount > 0 ? rollCount : 0));


  display.setTextSize(1);

  if (currentEntry.length() == 0) {
    display.setCursor(0, 22);
    display.print("No entry read.");
    display.display();
    return;
  }

  // Text area: y=20 down to y=63 (no footer now)
  const int y0 = 20;
  const int lineH = 8;
  const int maxLinesOnScreen = (64 - y0) / lineH; // 44/8 = 5 lines

  // Wrapping assumptions (size-1 font): ~21 chars across (128/6)
  const uint8_t CHARS_PER_LINE = 21;

  // Total wrapped lines
  uint16_t totalLines = countWrappedLines(currentEntry, CHARS_PER_LINE);

  // Clamp scroll
  if (tableScrollLine < 0) tableScrollLine = 0;
  int16_t maxScroll = (int16_t)totalLines - (int16_t)maxLinesOnScreen;
  if (maxScroll < 0) maxScroll = 0;
  if (tableScrollLine > maxScroll) tableScrollLine = maxScroll;

  // Render: skip wrapped lines until tableScrollLine
  int16_t curLine = 0;
  int16_t drawLine = 0;
  uint8_t col = 0;

  for (uint16_t i = 0; i < currentEntry.length(); i++) {
    char c = currentEntry[i];

    // handle explicit newlines
    if (c == '\n') {
      curLine++;
      col = 0;
      continue;
    }

    // wrap
    if (col >= CHARS_PER_LINE) {
      curLine++;
      col = 0;
    }

    // draw only if within window
    if (curLine >= tableScrollLine && drawLine < maxLinesOnScreen) {
      int y = y0 + drawLine * lineH;
      int x = col * 6;
      display.setCursor(x, y);
      display.write(c);
    }

    // advance col + manage drawLine transitions
    col++;
    if (col >= CHARS_PER_LINE) {
      if (curLine >= tableScrollLine) drawLine++;
    }
    if (drawLine >= maxLinesOnScreen) break;
  }

  display.display();
}


void uiRedraw() {
  if (mode == MODE_CATS) drawCategories();
  else if (mode == MODE_FILES) drawFiles();
  else drawTable();
}



// Scan /DATA for immediate subfolders (one-level deep)
void scanCategories() {
  catCount = 0;

  if (!SD.exists("/DATA")) {
    Serial.println("No /DATA directory found.");
    return;
  }

  FsFile dataDir = SD.open("/DATA");
  if (!dataDir || !dataDir.isDirectory()) {
    Serial.println("Failed to open /DATA as directory.");
    return;
  }

  FsFile entry;
  while (catCount < MAX_CATS && (entry = dataDir.openNextFile())) {
    if (entry.isDirectory()) {
      char nameBuf[64];
      entry.getName(nameBuf, sizeof(nameBuf));
      // skip "." and ".." if they appear
      if (nameBuf[0] != '.') {
        categories[catCount++] = String(nameBuf);
      }
    }
    entry.close();
  }
  dataDir.close();

  Serial.print("Found categories: ");
  Serial.println(catCount);
  for (uint8_t i = 0; i < catCount; i++) {
    Serial.print("  - ");
    Serial.println(categories[i]);
  }

  cursor = 0;
  scrollTop = 0;
}

// Button press event (edge)
void onButtonPressed(BtnId b) {
  lastInputMs = millis();

  // LED blinkc:\Users\sousa\GitHub\Monolith\MonolithV0\ArduinoCode\workingSketch.ino
  analogWrite(LED_PIN, LED_DIM);
  ledBlinkOn = true;
  ledOffMs = lastInputMs + 80;

  // beep tone per button
  uint16_t base = 700;
  uint16_t freq = base + (uint16_t)b * 200;
  buzzerStart(freq, 30);

  if (mode == MODE_CATS) {
    if (b == BTN_UP) cursor--;
    else if (b == BTN_DOWN) cursor++;
    else if (b == BTN_A) { // A=open folder
      if (catCount > 0) {
        selectedCat = categories[cursor];
        scanCsvFilesForSelectedCategory();
        mode = MODE_FILES;
      }
    } else if (b == BTN_B) {
      // top-level: no-op for now
    }
  } else if (mode == MODE_FILES) {
    if (b == BTN_UP) fileCursor--;
    else if (b == BTN_DOWN) fileCursor++;
    else if (b == BTN_A) { // A=select csv -> open table view
      if (fileCount > 0) {
        selectedFile = files[fileCursor];

        String fullPath = String("/DATA/") + selectedCat + "/" + selectedFile;

        rollCount = 0;
        currentEntry = "";

        if (pickRandomCsvLine(fullPath, currentEntry)) {
          rollCount = 1;
          mode = MODE_TABLE;
          tableScrollLine = 0; // ADDED IN V3
        } else {
          Serial.println("No selectable lines found.");
          currentEntry = "No selectable lines.";
          rollCount = 1;
          mode = MODE_TABLE;
          tableScrollLine = 0; // ADDED IN V3
        }
      }
    } else if (b == BTN_B) { // B=back
      mode = MODE_CATS;
    }
 } else if (mode == MODE_TABLE) {
    if (b == BTN_A) { // reroll
      String fullPath = String("/DATA/") + selectedCat + "/" + selectedFile;
      if (pickRandomCsvLine(fullPath, currentEntry)) {
        rollCount++;
        tableScrollLine = 0;   // <-- add here
      }
    } else if (b == BTN_UP) {
      tableScrollLine--;
    } else if (b == BTN_DOWN) {
      tableScrollLine++;
    } else if (b == BTN_B) {
      mode = MODE_FILES;
    }
  }

  uiRedraw();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP); // buttons to GND
  }

  Wire.begin();
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed (addr 0x3C?)");
    while (1) delay(10);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Boot...");
  display.display();

  // startup chirp
  buzzerStart(880, 40);
  delay(60);
  buzzerStart(1320, 40);

  // ---- SD init ----
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Init SD...");
  display.display();

  Serial.println("Initializing SD...");
  while (!SD.begin(sdConfig)) {
    Serial.println("SD init failed, retrying...");
    display.setCursor(0, 12);
    display.print("SD fail, retry...");
    display.display();
    delay(800);
  }
  Serial.println("SD init OK.");

  scanCategories();

  lastInputMs = millis();
  lastOledMs = 0;
  uiRedraw();
}

void loop() {
  uint32_t now = millis();
  buzzerUpdate(now);

  // end LED blink
  if (ledBlinkOn && (int32_t)(now - ledOffMs) >= 0) {
    analogWrite(LED_PIN, 0);
    ledBlinkOn = false;
  }

  // read buttons (edge detect)
  for (int i = 0; i < 4; i++) {
    bool curPressed = (digitalRead(BTN_PINS[i]) == LOW);

    if (!wasPressed[i] && curPressed) {
      if (now - lastPressMs[i] >= DEBOUNCE_MS) {
        lastPressMs[i] = now;
        wasPressed[i] = true;
        onButtonPressed((BtnId)i);
        Serial.print(BTN_NAMES[i]);
        Serial.println(" PRESSED");
      }
    }

    if (wasPressed[i] && !curPressed) {
      wasPressed[i] = false;
    }
  }

  // Optional periodic refresh (not strictly necessary right now)
  if ((now - lastOledMs >= OLED_PERIOD_MS) && (now - lastInputMs >= OLED_IDLE_AFTER_MS)) {
    lastOledMs = now;
    // uiRedraw(); // Uncomment if you want a steady refresh loop
  }

  delay(1);
}