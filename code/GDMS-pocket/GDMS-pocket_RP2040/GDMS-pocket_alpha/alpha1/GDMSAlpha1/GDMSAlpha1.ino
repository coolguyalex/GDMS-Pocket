/*
  POC_V04_SPI128x128_SH1107.ino

  Rewrite of POC_V04 for:
  - Feather RP2040 Adalogger
  - 1.5" SH1107 128x128 OLED (SPI) via U8g2
  - SD card via SdFat on SPI1 (OLED uses SW SPI to avoid conflicts)

  Based on:
  - POC_V04 structure/state machine/UI logic  :contentReference[oaicite:3]{index=3}
  - Log8 pin map + SH1107 offset note        :contentReference[oaicite:4]{index=4}
*/


#include <Arduino.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SdFat.h>


// =================== PINS (from Log8 known-good) ===================
// Buttons (active-low, INPUT_PULLUP). Wired pin -> GND
// Up=A2, Down=A3, A=A1, B=A0
enum BtnId : uint8_t { BTN_A = 0, BTN_B = 1, BTN_UP = 2, BTN_DOWN = 3 };
const uint8_t BTN_PINS[4]  = { A3, 24, A1, A2 };
const char*   BTN_NAMES[4] = { "A(A1)", "B(A0)", "UP(A2)", "DN(A3)" };

// Buzzer signal on D5 (passive buzzer module)
const uint8_t BUZZER_PIN = 6;

// External LED on D24 (anode via resistor -> D24, cathode -> GND)
const uint8_t LED_PIN = 25;

// OLED (software SPI) pin mapping per Log8:
// SCL/CLK=D13, SDA/DATA=D12, DC=D11, CS=D10, RST=D9
const uint8_t OLED_CLK = 13;
const uint8_t OLED_DAT = 12;
const uint8_t OLED_DC  = 11;
const uint8_t OLED_CS  = 10;
const uint8_t OLED_RST = 9;

// =================== OLED (U8g2) ===================
// Page-buffer constructor (lighter RAM). You can switch to _F_ later if desired.
// NOTE: If you see a 32px wrap/shift, apply the U8g2 SH1107 x_offset patch described in Log8.
U8G2_SH1107_128X128_1_4W_SW_SPI u8g2(
  U8G2_R0,
  /* clock=*/ OLED_CLK,
  /* data=*/  OLED_DAT,
  /* cs=*/    OLED_CS,
  /* dc=*/    OLED_DC,
  /* reset=*/ OLED_RST
);

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

// =================== SD (Adafruit Feather RP2040 Adalogger) ===================
#define SD_CS_PIN 23
SdFat SD;
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);

// =================== APP STATE ===================
static const uint8_t MAX_CATS = 40;
String categories[MAX_CATS];
uint8_t catCount = 0;

int16_t cursor = 0;     // which category is selected
int16_t scrollTop = 0;  // first visible row

static const uint8_t MAX_FILES = 60;
String files[MAX_FILES];
uint8_t fileCount = 0;

int16_t fileCursor = 0;
int16_t fileScrollTop = 0;

enum UiMode : uint8_t { MODE_CATS = 0, MODE_FILES = 1, MODE_TABLE = 2 };
UiMode mode = MODE_CATS;

String selectedCat;
String selectedFile;

String currentEntry;     // the chosen CSV row (raw line)
uint16_t rollCount = 0;  // how many rerolls in this table view
int16_t tableScrollLine = 0; // which wrapped line to start drawing from

// =================== OLED THROTTLE POLICY ===================
uint32_t lastInputMs = 0;
uint32_t lastOledMs = 0;
const uint16_t OLED_PERIOD_MS = 90;
const uint16_t OLED_IDLE_AFTER_MS = 0;

// ---------- helpers ----------
// static inline String stripCsvExt(const String& s) {
//   if (s.length() >= 4) {
//     String tail = s.substring(s.length() - 4);
//     tail.toLowerCase();
//     if (tail == ".csv") return s.substring(0, s.length() - 4);
//   }
//   return s;
// }

void showSplashScreen(uint16_t ms = 2000) {
  const uint32_t t0 = millis();

  while ((uint32_t)(millis() - t0) < ms) {
    u8g2.firstPage();
    do {
      // Big title block
      u8g2.setFont(u8g2_font_8x13_tf);

      u8g2.drawStr(0, 18,  "GOBLINOID");
      u8g2.drawStr(0, 36,  "DUNGEON");
      u8g2.drawStr(0, 54,  "MASTERING");
      u8g2.drawStr(0, 72,  "SYSTEM");

      // Footer
      u8g2.setFont(u8g2_font_6x12_tf);
      u8g2.drawStr(0, 102, "-Alexander Sousa");
      u8g2.drawStr(0, 118, "2026");
    } while (u8g2.nextPage());

    delay(10); // small yield
  }
}


static inline bool endsWithCsv(const String& s) {
  if (s.length() < 4) return false;
  String tail = s.substring(s.length() - 4);
  tail.toLowerCase();
  return tail == ".csv";
}

static inline bool endsWithJson(const String& s) {
  if (s.length() < 5) return false;
  String tail = s.substring(s.length() - 5);
  tail.toLowerCase();
  return tail == ".json";
}


static inline String stripCsvOrJsonExt(const String& s) {
  String lower = s;
  lower.toLowerCase();
  if (lower.endsWith(".csv"))  return s.substring(0, s.length() - 4);
  if (lower.endsWith(".json")) return s.substring(0, s.length() - 5);
  return s;
}

// Conservative ellipsize by character count (works fine for your mostly-ASCII UI labels)
static inline String ellipsize(const String& s, uint8_t maxChars) {
  if (s.length() <= maxChars) return s;
  if (maxChars <= 3) return s.substring(0, maxChars);
  return s.substring(0, maxChars - 3) + "...";
}

// Count wrapped lines for a string at charsPerLine
static uint16_t countWrappedLines(const String& s, uint8_t charsPerLine) {
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

// Simple right-aligned counter "i/n" on header line
static void drawHeader(const String& title, int currentIndexZeroBased = -1, int totalCount = -1) {
  const int W = 128;

  // Larger header font ONLY
  u8g2.setFont(u8g2_font_8x13_tf);

  const int y = 15;   // baseline for header text
  u8g2.drawStr(0, y, title.c_str());

  if (totalCount > 0 && currentIndexZeroBased >= 0) {
    int pos = currentIndexZeroBased + 1;
    if (pos < 1) pos = 1;
    if (pos > totalCount) pos = totalCount;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%d", pos, totalCount);

    int textW = u8g2.getStrWidth(buf);
    u8g2.drawStr(W - textW, y, buf);
  }

  // Divider just under header
  u8g2.drawHLine(0, 20, 128);
}


// ---------- SD scanning ----------
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
      if (nameBuf[0] != '.') {
        categories[catCount++] = String(nameBuf);
      }
    }
    entry.close();
  }
  dataDir.close();

  cursor = 0;
  scrollTop = 0;

  Serial.print("Found categories: ");
  Serial.println(catCount);
}

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
      if (fn.length() > 0 && fn[0] == '_') {
        entry.close();
        continue;
      }

if (endsWithCsv(fn) || endsWithJson(fn)) {
  files[fileCount++] = fn;
}

    }
    entry.close();
  }
  dir.close();

  // Sort files: JSON files first, then case-insensitive alphabetical by name (without extension)
  for (int i = 0; i < (int)fileCount - 1; i++) {
    for (int j = i + 1; j < (int)fileCount; j++) {
      bool iJson = endsWithJson(files[i]);
      bool jJson = endsWithJson(files[j]);
      bool doSwap = false;

      if (!iJson && jJson) {
        // bring JSON earlier
        doSwap = true;
      } else if (iJson == jJson) {
        // same type: compare base names case-insensitively
        String a = stripCsvOrJsonExt(files[i]);
        String b = stripCsvOrJsonExt(files[j]);
        a.toLowerCase();
        b.toLowerCase();
        if (a.compareTo(b) > 0) doSwap = true;
      }

      if (doSwap) {
        String tmp = files[i];
        files[i] = files[j];
        files[j] = tmp;
      }
    }
  }

  Serial.print("CSV files in ");
  Serial.print(path);
  Serial.print(": ");
  Serial.println(fileCount);
}

bool pickRandomCsvLine(const String& fullPath, String& outLine) {
  FsFile f = SD.open(fullPath.c_str(), O_RDONLY);
  if (!f) {
    Serial.print("Open failed: ");
    Serial.println(fullPath);
    return false;
  }

  const size_t BUF_SZ = 220;
  char buf[BUF_SZ];

  uint32_t cumulative = 0;
  bool chosen = false;
  outLine = "";

  // Single-pass weighted reservoir sampling:
  // - If a line begins with a numeric value followed by a comma, that value is treated as an integer weight.
  // - If the first token is missing or not a valid integer, the entire line is treated as the entry with weight 1.
  // - We default any non-positive weights to 1.
  while (true) {
    int n = f.fgets(buf, (int)BUF_SZ);
    if (n <= 0) break;

    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
      buf[--n] = '\0';
    }

    if (n == 0) continue;
    if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/')) continue;

    // Work with an Arduino String for convenient parsing
    String line = String(buf);

    // Trim only leading/trailing whitespace for robust parsing
    line.trim();
    if (line.length() == 0) continue;

    uint32_t weight = 1;
    String entryText = line; // default: whole line is the entry

    int commaIdx = line.indexOf(',');
    if (commaIdx >= 0) {
      String firstToken = line.substring(0, commaIdx);
      firstToken.trim();

      // Attempt to parse an integer weight from the first token using strtol to detect invalid parses
      const char* tok = firstToken.c_str();
      char* endptr = nullptr;
      long parsed = strtol(tok, &endptr, 10);

      // If parsing consumed at least one character and the token wasn't empty, treat it as a weight
      if (endptr != tok && firstToken.length() > 0) {
        if (parsed <= 0) parsed = 1; // enforce minimum weight
        weight = (uint32_t)parsed;

        // The entry text is the remainder after the comma
        entryText = line.substring(commaIdx + 1);
        entryText.trim();
        // leave entryText as-is; we'll skip empty/invalid entries below
      }
      // else: first token wasn't a valid number — keep whole line as entryText and weight=1
    }

    // Skip empty or malformed entries (e.g., a lone comma "," or blank line)
    if (entryText.length() == 0) continue;
    if (entryText == ",") continue;

    // Update weighted reservoir selection
    cumulative += weight;
    // random(x) yields [0..x-1], so chance to pick this item is weight / cumulative
    if (random((long)cumulative) < (long)weight) {
      outLine = entryText;
      chosen = true;
    }
  }

  f.close();
  return chosen;
}

String joinRelativePath(const String& baseFilePath, const String& rel) {
  // rel like "/items/treasure.csv" means "/DATA/items/treasure.csv"
  // Also accept leading "/data/..." (case-insensitive) which some recipes use.
  if (rel.startsWith("/")) {
    String lower = rel;
    lower.toLowerCase();
    if (lower.startsWith("/data/")) {
      // strip the "/data" prefix and prepend the correct "/DATA"
      return String("/DATA") + rel.substring(5); // keep the leading '/'
    }
    return String("/DATA") + rel;
  }

  // otherwise: relative to recipe's folder
  int slash = baseFilePath.lastIndexOf('/');
  if (slash < 0) return rel; // fallback
  String baseDir = baseFilePath.substring(0, slash + 1);
  return baseDir + rel;
}

bool runJsonRecipeV1(const String& recipePath, String& outText) {
  FsFile f = SD.open(recipePath.c_str(), O_RDONLY);
  if (!f) {
    Serial.print("Recipe open failed: ");
    Serial.println(recipePath);
    return false;
  }

  // Keep modest; increase if your recipe JSON grows.
  // If you see "NoMemory", bump this number.
  JsonDocument doc;

  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.print("Recipe JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray parts = doc["parts"].as<JsonArray>();
  if (parts.isNull()) {
    Serial.println("Recipe missing parts[]");
    return false;
  }

  outText = "";

  // We'll collect each part's inline and block forms so we can substitute into a format string if present.
  const int MAX_PARTS_IN_RECIPE = 40;
  String partLabels[MAX_PARTS_IN_RECIPE];
  String partInline[MAX_PARTS_IN_RECIPE];
  String partBlock[MAX_PARTS_IN_RECIPE];
  int partsFound = 0;
  const int MAX_REPEAT = 20;

  for (JsonObject part : parts) {
    // p defaults to 1.0
    double p = 1.0;
    if (part["p"].is<double>()) p = part["p"].as<double>();
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;

    long threshold = (long)(p * 1000.0 + 0.5);
    if (threshold < 1000) {
      if (random(1000) >= threshold) continue;
    }

    const char* rollC = part["roll"];
    if (!rollC || rollC[0] == '\0') continue;

    String rollPath = joinRelativePath(recipePath, String(rollC));

    // Determine how many times to sample this part (default 1)
    int repeatCount = 1;

    if (part["repeat"].is<int>()) {
      repeatCount = part["repeat"].as<int>();
    } else if (part["repeat"].is<JsonObject>()) {
      int rmin = 1, rmax = 1;
      if (part["repeat"]["min"].is<int>()) rmin = part["repeat"]["min"].as<int>();
      if (part["repeat"]["max"].is<int>()) rmax = part["repeat"]["max"].as<int>();
      if (rmax < rmin) { int t = rmin; rmin = rmax; rmax = t; }
      repeatCount = (int)random(rmin, rmax + 1);
    } else if (part["repeat"].is<const char*>()) {
      // accept simple "min-max" string
      const char* rs = part["repeat"].as<const char*>();
      int a=0, b=0;
      if (sscanf(rs, "%d-%d", &a, &b) == 2) {
        if (b < a) { int t = a; a = b; b = t; }
        repeatCount = (int)random(a, b + 1);
      } else {
        int v = atoi(rs);
        if (v > 0) repeatCount = v;
      }
    }

    // Safety clamp: avoid runaway counts
    if (repeatCount < 0) repeatCount = 0;
    if (repeatCount > MAX_REPEAT) repeatCount = MAX_REPEAT;

    // Collect sampled values
    String sampled[MAX_REPEAT];
    int sv = 0;
    for (int ri = 0; ri < repeatCount; ri++) {
      String line;
      if (!pickRandomCsvLine(rollPath, line)) {
        Serial.print("Roll failed: ");
        Serial.println(rollPath);
        continue; // skip this repetition
      }
      sampled[sv++] = line;
    }

    // Determine join option (controls inline substitution behavior): default = "comma"
    String joinOpt = "comma";
    if (part["join"].is<const char*>()) {
      joinOpt = String(part["join"].as<const char*>());
      joinOpt.toLowerCase();
    }

    // Build inline representation
    String inlineVal = "";
    if (sv == 1) {
      inlineVal = sampled[0];
    } else if (sv > 1) {
      if (joinOpt == "nl" || joinOpt == "newline") {
        for (int k = 0; k < sv; k++) {
          if (k) inlineVal += "\n";
          inlineVal += sampled[k];
        }
      } else { // comma (default)
        for (int k = 0; k < sv; k++) {
          if (k) inlineVal += ", ";
          inlineVal += sampled[k];
        }
      }
    }

    // Build block representation (for fallback output if no format string is provided)
    String labelStr = "";
    if (part["label"].is<const char*>()) labelStr = String(part["label"].as<const char*>());

    String blockVal = "";
    if (sv == 0) {
      if (labelStr.length()) {
        blockVal = labelStr + ":\n";
      }
    } else if (sv == 1) {
      if (labelStr.length()) {
        blockVal = labelStr + ": " + sampled[0];
      } else {
        blockVal = sampled[0];
      }
    } else {
      if (labelStr.length()) {
        blockVal = labelStr + ":\n";
        for (int k = 0; k < sv; k++) {
          blockVal += "- ";
          blockVal += sampled[k];
          blockVal += "\n";
        }
        // remove trailing newline
        if (blockVal.endsWith("\n")) blockVal.remove(blockVal.length() - 1);
      } else {
        for (int k = 0; k < sv; k++) {
          blockVal += sampled[k];
          blockVal += "\n";
        }
        if (blockVal.endsWith("\n")) blockVal.remove(blockVal.length() - 1);
      }
    }

    // Store for substitution and ordered fallback
    if (partsFound < MAX_PARTS_IN_RECIPE) {
      partLabels[partsFound] = labelStr;
      partInline[partsFound] = inlineVal;
      partBlock[partsFound] = blockVal;
      partsFound++;
    }
  }

  // If a format string is present, perform placeholder substitution using the inline forms
  const char* fmtC = doc["format"];
  if (fmtC && fmtC[0] != '\0') {
    String fmt = String(fmtC);

    for (int i = 0; i < partsFound; i++) {
      if (partLabels[i].length() == 0) continue;
      String ph = "{" + partLabels[i] + "}";
      int pos = fmt.indexOf(ph);
      while (pos >= 0) {
        fmt = fmt.substring(0, pos) + partInline[i] + fmt.substring(pos + ph.length());
        pos = fmt.indexOf(ph);
      }
    }

    outText = fmt;
  } else {
    // Fallback behavior: emit each part's block form in order (previous behavior)
    for (int i = 0; i < partsFound; i++) {
      if (partBlock[i].length() == 0) continue;
      outText += partBlock[i];
      outText += "\n";
    }
  }

  // trim trailing newlines
  while (outText.endsWith("\n") || outText.endsWith("\r")) {
    outText.remove(outText.length() - 1);
  }

  return outText.length() > 0;
}


// ---------- UI draw ----------
void drawCategories() {
  u8g2.firstPage();
  do {
    drawHeader("GDMS-pocket", cursor, catCount);

    if (catCount == 0) {
      u8g2.drawStr(0, 40, "No folders found.");
      u8g2.drawStr(0, 55, "Need /DATA/<FOLDER>");
      continue;
    }

    // Layout on 128x128:
    // Header line at y=12, divider at y=16.
    // List starts y=30-ish, 12px per row with 6x12 font.
    const int listY0 = 30;
    const int rowH   = 14;
    const int rows   = 8;

    if (cursor < 0) cursor = 0;
    if (cursor >= (int)catCount) cursor = catCount - 1;

    if (cursor < scrollTop) scrollTop = cursor;
    if (cursor >= scrollTop + rows) scrollTop = cursor - rows + 1;

    u8g2.setFont(u8g2_font_6x12_tf);

    for (int r = 0; r < rows; r++) {
      int idx = scrollTop + r;
      if (idx >= (int)catCount) break;

      int y = listY0 + r * rowH;

      String name = categories[idx];
      name = ellipsize(name, 19);

      if (idx == cursor) {
        u8g2.drawStr(0, y, "> ");
        u8g2.drawStr(14, y, name.c_str());
      } else {
        u8g2.drawStr(0, y, "  ");
        u8g2.drawStr(14, y, name.c_str());
      }
    }
  } while (u8g2.nextPage());
}

void drawFiles() {
  u8g2.firstPage();
  do {
    drawHeader(selectedCat, fileCursor, fileCount);

    u8g2.setFont(u8g2_font_6x12_tf);

    if (fileCount == 0) {
      u8g2.drawStr(0, 40, "No .csv files.");
      continue;
    }

    const int listY0 = 30;
    const int rowH   = 14;
    const int rows   = 8;

    if (fileCursor < 0) fileCursor = 0;
    if (fileCursor >= (int)fileCount) fileCursor = fileCount - 1;

    if (fileCursor < fileScrollTop) fileScrollTop = fileCursor;
    if (fileCursor >= fileScrollTop + rows) fileScrollTop = fileCursor - rows + 1;

    for (int r = 0; r < rows; r++) {
      int idx = fileScrollTop + r;
      if (idx >= (int)fileCount) break;

      int y = listY0 + r * rowH;

      String shown = stripCsvOrJsonExt(files[idx]);
      shown = ellipsize(shown, 19);

      if (idx == fileCursor) {
        u8g2.drawStr(0, y, "> ");
        u8g2.drawStr(14, y, shown.c_str());
      } else {
        u8g2.drawStr(0, y, "  ");
        u8g2.drawStr(14, y, shown.c_str());
      }
    }
  } while (u8g2.nextPage());
}

void drawTable() {
  // Wrapping assumptions for 6x12 font:
  // 128px / ~6px per char ≈ 21 chars
  const uint8_t CHARS_PER_LINE = 21;

  // Text block: start under header divider
  const int y0 = 30;
  const int lineH = 14; // 12px font + a bit of breathing room
  const int maxLinesOnScreen = (128 - y0) / lineH; // ~7 lines

  uint16_t totalLines = countWrappedLines(currentEntry, CHARS_PER_LINE);

  if (tableScrollLine < 0) tableScrollLine = 0;
  int16_t maxScroll = (int16_t)totalLines - (int16_t)maxLinesOnScreen;
  if (maxScroll < 0) maxScroll = 0;
  if (tableScrollLine > maxScroll) tableScrollLine = maxScroll;

  String hdr = stripCsvOrJsonExt(selectedFile);
  hdr = ellipsize(hdr, 21);

  u8g2.firstPage();
  do {
    // Header: use rollCount as counter (same intent as your POC_V04)
    int cur = (rollCount > 0 ? (int)rollCount - 1 : 0);
    int tot = (rollCount > 0 ? (int)rollCount : 0);
    drawHeader(hdr, cur, tot);

    u8g2.setFont(u8g2_font_6x12_tf);

    if (currentEntry.length() == 0) {
      u8g2.drawStr(0, 40, "No entry read.");
      continue;
    }

    // Render wrapped lines, but only those visible
    int16_t curLine = 0;
    int16_t drawLine = 0;
    uint8_t col = 0;

    // We'll build each visible line into a small char buffer for clean drawStr calls
    // (u8g2 likes whole strings more than per-char writes).
    char lineBuf[CHARS_PER_LINE + 1];
    memset(lineBuf, 0, sizeof(lineBuf));
    uint8_t linePos = 0;

    auto flushLineIfVisible = [&](bool force) {
      if (!force && linePos == 0) return;

      if (curLine >= tableScrollLine && drawLine < maxLinesOnScreen) {
        int y = y0 + drawLine * lineH;
        lineBuf[linePos] = '\0';
        u8g2.drawStr(0, y, lineBuf);
        drawLine++;
      }

      // reset
      memset(lineBuf, 0, sizeof(lineBuf));
      linePos = 0;
    };

    for (uint16_t i = 0; i < currentEntry.length(); i++) {
      char c = currentEntry[i];

      if (c == '\n') {
        flushLineIfVisible(true);
        curLine++;
        col = 0;
        if (drawLine >= maxLinesOnScreen) break;
        continue;
      }

      // wrap
      if (col >= CHARS_PER_LINE) {
        flushLineIfVisible(true);
        curLine++;
        col = 0;
        if (drawLine >= maxLinesOnScreen) break;
      }

      // append char to current line buffer (best-effort; ignore overflow)
      if (linePos < CHARS_PER_LINE) {
        lineBuf[linePos++] = c;
      }

      col++;
    }

    // flush last partial line
    flushLineIfVisible(true);

  } while (u8g2.nextPage());
}

void uiRedraw() {
  if (mode == MODE_CATS) drawCategories();
  else if (mode == MODE_FILES) drawFiles();
  else drawTable();
}

// ---------- input handling ----------
void onButtonPressed(BtnId b) {
  lastInputMs = millis();

  // LED blink
  analogWrite(LED_PIN, LED_DIM);
  ledBlinkOn = true;
  ledOffMs = lastInputMs + 80;

  // button chirp
  uint16_t base = 700;
  uint16_t freq = base + (uint16_t)b * 200;
  buzzerStart(freq, 15);

  if (mode == MODE_CATS) {
    if (b == BTN_UP) cursor--;
    else if (b == BTN_DOWN) cursor++;
    else if (b == BTN_A) {
      if (catCount > 0) {
        selectedCat = categories[cursor];
        scanCsvFilesForSelectedCategory();
        mode = MODE_FILES;
      }
    } else if (b == BTN_B) {
      // no-op at top level
    }
  } else if (mode == MODE_FILES) {
    if (b == BTN_UP) fileCursor--;
    else if (b == BTN_DOWN) fileCursor++;
    else if (b == BTN_A) {
      if (fileCount > 0) {
        selectedFile = files[fileCursor];

        String fullPath = String("/DATA/") + selectedCat + "/" + selectedFile;

        rollCount = 0;
        currentEntry = "";

        String lower = selectedFile;
        lower.toLowerCase();

        bool ok = false;
        if (lower.endsWith(".csv")) {
          ok = pickRandomCsvLine(fullPath, currentEntry);
        } else if (lower.endsWith(".json")) {
          ok = runJsonRecipeV1(fullPath, currentEntry);
        } else {
          currentEntry = "Unsupported file.";
          ok = true;
        }

        rollCount = 1;
        mode = MODE_TABLE;
        tableScrollLine = 0;

        if (!ok) {
          currentEntry = "No selectable output.";
        }
      }
    } else if (b == BTN_B) {
      mode = MODE_CATS;
    }
  } else if (mode == MODE_TABLE) {
  if (b == BTN_A) { // reroll
    String fullPath = String("/DATA/") + selectedCat + "/" + selectedFile;

    String lower = selectedFile;
    lower.toLowerCase();

    bool ok = false;
    if (lower.endsWith(".csv")) {
      ok = pickRandomCsvLine(fullPath, currentEntry);
    } else if (lower.endsWith(".json")) {
      ok = runJsonRecipeV1(fullPath, currentEntry);
    } else {
      currentEntry = "Unsupported file.";
      ok = true;
    }

    if (!ok) currentEntry = "No selectable output.";

    rollCount++;
    tableScrollLine = 0;
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
  randomSeed(micros());

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  // OLED init
  u8g2.begin();
  u8g2.setContrast(255);
  showSplashScreen(2000);


  // Boot screen
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 20, "Boot...");
    u8g2.drawStr(0, 40, "Init SD...");
  } while (u8g2.nextPage());

  // startup chirp
  buzzerStart(880, 40);
  delay(60);
  buzzerStart(1320, 40);

  // SD init
  Serial.println("Initializing SD...");
  while (!SD.begin(sdConfig)) {
    Serial.println("SD init failed, retrying...");
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_6x12_tf);
      u8g2.drawStr(0, 20, "Init SD...");
      u8g2.drawStr(0, 40, "SD fail, retry...");
    } while (u8g2.nextPage());
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

  // Optional periodic refresh hook (generally unnecessary with page-buffer redraw-on-input)
  if ((now - lastOledMs >= OLED_PERIOD_MS) && (now - lastInputMs >= OLED_IDLE_AFTER_MS)) {
    lastOledMs = now;
    // uiRedraw();
  }

  delay(1);
}
