/*
  GDMS-p_Alpha.ino

  
  - Feather RP2040 Adalogger
  - 1.5" SH1107 128x128 OLED (SPI) via U8g2
  - SD card via SdFat on SPI1 (OLED uses SW SPI to avoid conflicts)

  Based on:
  - POC_V04 structure/state machine/UI logic  :contentReference[oaicite:3]{index=3}
  - Log8 pin map + SH1107 offset note        :contentReference[oaicite:4]{index=4}

Alpha   -  Core functionality fully implemented
Alpha 2 - Word-aware wrapping 260228
Alpha 3 - Hold  to scroll 250228
Alpha 4 - List wrapping 
Alpha 5 - LED breathe and pop. 
Alpha 6 - Options A+B options menu
Alpha 7 - Sleep and wake, scroll bar + about in options menu
Alpha 8 - more LED options, scroll bar on CATS menu. 
Alpha 9 - Battery Indicator implementation

*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SdFat.h>


// =================== PINS (from Log8 known-good) ===================
// Buttons (active-low, INPUT_PULLUP). Wired pin -> GND
// Up=A1, Down=A2, A=A3, B=24
enum BtnId : uint8_t { BTN_A = 0, BTN_B = 1, BTN_UP = 2, BTN_DOWN = 3 };
const uint8_t BTN_PINS[4]  = { A3, 24, A1, A2 };
const char*   BTN_NAMES[4] = { "A(A3)", "B(24)", "UP(A1)", "DN(A2)" };

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

// Battery monitor — external voltage divider: BAT → 100kΩ → A0 → 100kΩ → GND
// Center tap (A0) reads half the battery voltage; we double it in software.
const uint8_t BATTERY_PIN = A0;

// =================== BATTERY STATE ===================
float    battVoltage      = 4.2f;   // last measured battery voltage (V); starts optimistic
uint32_t lastBattSampleMs = 0;
const uint32_t BATT_SAMPLE_INTERVAL_MS = 5000; // sample every 5 s

// Voltage thresholds (LiPo 4.2V full -> ~3.2V cutoff)
// 3 fill levels: FULL=3 bars >=4.0V, OK=2 bars >=3.7V, LOW=1 bar >=3.5V, CRIT=0 bars <3.5V
enum BattLevel : uint8_t { BATT_CRIT = 0, BATT_LOW = 1, BATT_OK = 2, BATT_FULL = 3 };

BattLevel battLevelFromVoltage(float v) {
  if (v >= 4.0f) return BATT_FULL;
  if (v >= 3.7f) return BATT_OK;
  if (v >= 3.5f) return BATT_LOW;
  return BATT_CRIT;
}

// Take 8 ADC samples and average them; update battVoltage.
void sampleBattery() {
  analogReadResolution(12); // ensure 12-bit (0-4095)
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(BATTERY_PIN);
    delay(1);
  }
  float raw = sum / 8.0f;
  float vA0 = (raw / 4095.0f) * 3.3f;
  battVoltage = vA0 * 2.0f; // undo the divider
}

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

// =================== SLEEP / WAKE ===================
bool     deviceSleeping = false;  // true while OLED is blanked
uint32_t lastActivityMs = 0;      // updated on every button press

// =================== USER SETTINGS (runtime, not persisted) ===================
// These are controlled via the Options menu and may change at runtime.

// --- Buzzer ---
// buzzerEnabled already declared below with buzzer state vars

// Buzzer volume: two levels via a series-resistor workaround in software
// (we vary tone duration; hardware resistor remains the real fix,
//  but shorter bursts feel meaningfully quieter)
enum BuzVol  : uint8_t { BUZ_VOL_SOFT = 0, BUZ_VOL_LOUD = 1 };
BuzVol buzVolume = BUZ_VOL_LOUD; // default: loud

// --- LED master on/off and pop on/off ---
bool ledEnabled    = true;  // master LED switch (breathing + pop)
bool ledPopEnabled = true;  // pop flash on button press (requires ledEnabled)

// --- LED brightness (affects BREATH_MAX and POP_BRIGHTNESS) ---
enum LedBright : uint8_t { LED_BRIGHT_LOW = 0, LED_BRIGHT_MED = 1, LED_BRIGHT_HIGH = 2 };
LedBright ledBrightness = LED_BRIGHT_MED; // default: medium

// Lookup tables indexed by LedBright
const uint8_t  LED_BREATH_MAX_TBL[3] = { 20,  80, 180 };
const uint8_t  LED_POP_TBL[3]        = { 25, 128, 255 };

// --- Breath speed ---
enum BreathSpd : uint8_t { BREATH_FAST = 0, BREATH_MED = 1, BREATH_SLOW = 2 };
BreathSpd breathSpeed = BREATH_MED; // default: medium

// Lookup table indexed by BreathSpd (ms per full cycle)
const uint32_t BREATH_PERIOD_TBL[3] = { 500, 3000, 10000 };

// --- Sleep timeout ---
enum SleepTimeout : uint8_t { SLEEP_5MIN = 0, SLEEP_10MIN = 1, SLEEP_30MIN = 2, SLEEP_NEVER = 3 };
SleepTimeout sleepTimeout = SLEEP_5MIN; // default: 5 minutes

// Lookup table: timeout in milliseconds (0 = never)
const uint32_t SLEEP_TIMEOUT_TBL[4] = {
  5UL  * 60UL * 1000UL,  // 5 min
  10UL * 60UL * 1000UL,  // 10 min
  30UL * 60UL * 1000UL,  // 30 min
  0UL                    // never
};

// =================== LED BREATHING ===================
// Sine-wave breathing: LED fades in and out continuously.
// On button press a full-brightness "pop" interrupts, then breathing resumes.
// (BREATH_MIN is always 0; other params are read from tables above at runtime.)
const uint32_t POP_DURATION_MS  = 100;  // how long the pop lasts

bool     ledPopActive = false;   // true while the pop flash is showing
uint32_t ledPopEndMs  = 0;       // when the pop should end
uint32_t ledBreathOffset = 0;    // phase offset so breath resumes smoothly after pop

// =================== BUTTONS (debounce + edge detect + hold-repeat) ===================
bool wasPressed[4] = {false, false, false, false};
uint32_t lastPressMs[4] = {0, 0, 0, 0};
const uint16_t DEBOUNCE_MS = 40;

// Hold-to-repeat: only BTN_UP and BTN_DOWN auto-repeat; A and B are edge-only.
const uint32_t HOLD_DELAY_MS  = 500;  // pause before repeat starts
const uint32_t HOLD_REPEAT_MS = 100;  // interval between repeated fires
uint32_t holdStartMs[4]  = {0, 0, 0, 0}; // when the button was first confirmed pressed
uint32_t lastRepeatMs[4] = {0, 0, 0, 0}; // last time a repeat event fired
bool     holdActive[4]   = {false, false, false, false}; // button is currently held

// =================== NON-BLOCKING BUZZER ===================

bool buzzerEnabled = false; //master switch (to be made otggleable in options menu sousa260228)
bool buzOn = false;
uint32_t buzOffMs = 0;

void buzzerStart(uint16_t freq, uint16_t durMs) {
  if (!buzzerEnabled) return;
  // Soft volume: halve the duration so the burst feels shorter/quieter
  if (buzVolume == BUZ_VOL_SOFT) durMs = max((uint16_t)1, (uint16_t)(durMs / 2));
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

void ledPop() {
  if (!ledEnabled || !ledPopEnabled) return;
  uint32_t now = millis();
  uint32_t period = BREATH_PERIOD_TBL[breathSpeed];
  ledBreathOffset = now % period;
  ledPopActive = true;
  ledPopEndMs  = now + POP_DURATION_MS;
  analogWrite(LED_PIN, LED_POP_TBL[ledBrightness]);
}

void ledUpdate(uint32_t now) {
  if (!ledEnabled) {
    analogWrite(LED_PIN, 0);
    ledPopActive = false;
    return;
  }
  if (ledPopActive) {
    if ((int32_t)(now - ledPopEndMs) >= 0) {
      ledPopActive = false;
      // Fall through immediately to resume breath on this same tick
    } else {
      return; // still popping — don't touch the LED
    }
  }

  // Triangle-wave breath using runtime period and brightness settings.
  uint32_t period   = BREATH_PERIOD_TBL[breathSpeed];
  uint8_t  breathMax = LED_BREATH_MAX_TBL[ledBrightness];
  uint32_t t = (now + ledBreathOffset) % period;
  uint8_t  val;
  if (t < period / 2) {
    val = (uint8_t)((uint32_t)breathMax * 2 * t / period);
  } else {
    val = (uint8_t)((uint32_t)breathMax * 2 * (period - t) / period);
  }
  analogWrite(LED_PIN, val);
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

enum UiMode : uint8_t { MODE_CATS = 0, MODE_FILES = 1, MODE_TABLE = 2, MODE_OPTIONS = 3, MODE_ABOUT = 4 };
UiMode mode     = MODE_CATS;
UiMode prevMode = MODE_CATS; // mode to return to when exiting options

// Options menu
const uint8_t OPT_COUNT = 8;   // Buzzer,BuzVol,LEDon,LEDpop,LEDBrt,Breathe,Sleep,About
int8_t optCursor = 0;          // which row is selected (0–OPT_COUNT-1)

// A+B combo detection
bool     abComboFired  = false;   // prevent repeated firing while both held
uint32_t abComboArmMs  = 0;       // when the second button of the pair came down
const uint16_t AB_COMBO_MS = 80;  // both must be held within this window

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

// ---------------------------------------------------------------------------
// Word-aware line wrapper — Arduino-compatible (no templates, no lambdas)
// ---------------------------------------------------------------------------
// Each wrapped line is described by a WrapLine: either a slice of the source
// string (via start+len) or a pre-built buffer (isOwned=true) for hyphenated
// hard-cuts.  MAX_WRAP_LINES caps memory; entries beyond it are silently lost
// (a safety net — typical entries are well under this limit).
// ---------------------------------------------------------------------------

const uint8_t  WRAP_CHARS  = 20;   // characters per display line (21st column reserved for scrollbar)
const uint16_t MAX_WRAP_LINES = 64; // max lines we'll ever store

struct WrapLine {
  char     buf[WRAP_CHARS + 1]; // null-terminated text for this line
};

// Shared buffer reused each time buildWrappedLines() is called.
// Declared at file scope so it lives in static RAM, not the call stack.
static WrapLine  wrapLines[MAX_WRAP_LINES];
static uint16_t  wrapLineCount = 0;

// Helper: copy ptr[0..pLen-1] into wrapLines[], appending '-' if hyphenSuffix.
static void wrapEmitLine(const char* ptr, uint8_t pLen, bool hyphenSuffix) {
  if (wrapLineCount >= MAX_WRAP_LINES) return;
  WrapLine& wl = wrapLines[wrapLineCount++];
  uint8_t copyN = (pLen <= WRAP_CHARS) ? pLen : WRAP_CHARS;
  if (hyphenSuffix && copyN == WRAP_CHARS) copyN = WRAP_CHARS - 1;
  memcpy(wl.buf, ptr, copyN);
  if (hyphenSuffix) { wl.buf[copyN] = '-'; copyN++; }
  wl.buf[copyN] = '\0';
}

// Populate wrapLines[] for string s at WRAP_CHARS columns.
// Returns number of lines produced (also stored in wrapLineCount).
static uint16_t buildWrappedLines(const String& s) {
  wrapLineCount = 0;
  const uint8_t  CPL = WRAP_CHARS;
  const uint16_t len = s.length();
  const char*    src = s.c_str();

  uint16_t i        = 0;  // read cursor in s
  uint8_t  lineLen  = 0;  // chars accumulated on the current line
  uint16_t lineStart = 0; // index in s where current line's content begins

  while (i < len) {
    char c = src[i];

    // ── Explicit newline ────────────────────────────────────────────────────
    if (c == '\n') {
      wrapEmitLine(src + lineStart, lineLen, false);
      i++;
      lineStart = i;
      lineLen   = 0;
      continue;
    }

    // ── Find end of next word ───────────────────────────────────────────────
    uint16_t wordEnd = i;
    while (wordEnd < len && src[wordEnd] != ' ' && src[wordEnd] != '\n') {
      wordEnd++;
    }
    uint16_t wordLen16 = wordEnd - i;
    uint8_t  wordLen   = (wordLen16 > 255) ? 255 : (uint8_t)wordLen16;

    // Eat leading space at the start of a line (prevents accidental indent)
    if (wordLen == 0 && c == ' ' && lineLen == 0) {
      lineStart++;
      i++;
      continue;
    }

    uint8_t spaceNeeded = (lineLen > 0) ? 1 : 0;

    // ── Word fits on current line ───────────────────────────────────────────
    if (lineLen + spaceNeeded + wordLen <= CPL) {
      if (lineLen > 0 && c == ' ') {
        lineLen++; // include the separating space in this line's slice
        i++;
      }
      lineLen += wordLen;
      i       += wordLen;

    // ── Word is too long to ever fit — hyphenated hard-cut ─────────────────
    } else if (wordLen >= CPL) {
      if (lineLen > 0) {
        wrapEmitLine(src + lineStart, lineLen, false);
        lineStart += lineLen;
        lineLen    = 0;
      }
      // Eat inter-word space
      if (i < len && src[i] == ' ') { lineStart++; i++; }

      uint16_t remaining = wordEnd - i;
      while (remaining > 0) {
        if (remaining <= (uint16_t)CPL) {
          // Last chunk — no hyphen, leave open for possible continuation
          lineLen    = (uint8_t)remaining;
          i         += remaining;
          remaining  = 0;
        } else {
          // Intermediate chunk — emit with hyphen
          uint8_t chunkChars = CPL - 1;
          wrapEmitLine(src + lineStart, chunkChars, true);
          i         += chunkChars;
          lineStart  = i;
          lineLen    = 0;
          remaining -= chunkChars;
        }
      }

    // ── Word doesn't fit — wrap to next line ───────────────────────────────
    } else {
      wrapEmitLine(src + lineStart, lineLen, false);
      lineStart += lineLen;
      lineLen    = 0;
      // Eat inter-word space that triggered the wrap
      if (i < len && src[i] == ' ') { lineStart++; i++; }
      // Re-evaluate the same word on the fresh line
    }
  }

  // Flush any remaining content (or emit at least one line for empty input)
  if (lineLen > 0 || wrapLineCount == 0) {
    wrapEmitLine(src + lineStart, lineLen, false);
  }

  return wrapLineCount;
}

// Convenience: just return the line count (same as buildWrappedLines but
// named clearly for the scroll-math call site).
static uint16_t countWrappedLines(const String& s, uint8_t /*charsPerLine*/) {
  return buildWrappedLines(s);
}

// ---------------------------------------------------------------------------
// Battery icon — drawn in the top-right corner of the header.
//
// Layout (all coords relative to top-left of icon):
//   Body:  18 × 9 px  outline rectangle  (x, iconY) .. (x+17, iconY+8)
//   Nub:    2 × 5 px  solid rect on the right end    (x+18, iconY+2)
//   Fill:  up to 4 segments, each 3 px wide, 5 px tall, 1 px gap between
//          segments sit inside the body with 1 px padding all round
//
// At CRIT level the entire icon is inverted (white box, black internals)
// and blinks on a 500 ms period to draw attention.
//
// Icon total width = 20 px. We anchor it at x = 128 - 20 = 108.
// ---------------------------------------------------------------------------
static void drawBatteryIcon(uint32_t now) {
  const int ICON_X  = 111;  // left edge of body — sized for 3 bars, flush-right with nub at x=127
  const int ICON_Y  = 3;    // top of body (fits in 0..19 header zone)
  const int BODY_W  = 15;   // 2px outline + 1px pad + 3×(3px bar + 1px gap) - 1 gap + 1px pad = 15
  const int BODY_H  = 9;
  const int NUB_W   = 2;
  const int NUB_H   = 5;
  const int NUB_Y   = ICON_Y + (BODY_H - NUB_H) / 2;

  // Fill segment geometry: 4 segments, 3 px wide, 5 px tall, 1 px gap
  // Padding inside body: 1 px top/bottom, 1 px left, 1 px right (before nub)
  const int SEG_W   = 3;
  const int SEG_H   = BODY_H - 4;  // 5 px tall
  const int SEG_GAP = 1;
  const int SEG_Y   = ICON_Y + 2;  // 1 px outline + 1 px inner padding (top)
  // Leftmost segment x: 1 px outline + 1 px inner padding (left)
  // This ensures all 4 sides of every bar have >=1 px clearance from the body outline.
  const int SEG_X0  = ICON_X + 2;

  BattLevel lvl = battLevelFromVoltage(battVoltage);
  // 3 bars: FULL=3, OK=2, LOW=1, CRIT=0 (empty body + flashing)
  int fillSegs = (int)lvl; // CRIT=0->0 bars, FULL=3->3 bars

  // Blink logic: at CRIT, invert every 500 ms
  bool invert = false;
  if (lvl == BATT_CRIT) {
    invert = ((now / 500) % 2 == 1);
  }

  if (invert) {
    // Inverted: flood-fill the body+nub white, then punch black internals
    u8g2.drawBox(ICON_X, ICON_Y, BODY_W, BODY_H);
    u8g2.drawBox(ICON_X + BODY_W, NUB_Y, NUB_W, NUB_H);
    // Erase the fill area that should be "empty" (draw black boxes over white flood)
    u8g2.setDrawColor(0);
    for (int s = fillSegs; s < 3; s++) {
      int sx = SEG_X0 + s * (SEG_W + SEG_GAP);
      u8g2.drawBox(sx, SEG_Y, SEG_W, SEG_H);
    }
    u8g2.setDrawColor(1); // restore
  } else {
    // Normal: outline body, solid nub, filled segments
    u8g2.drawFrame(ICON_X, ICON_Y, BODY_W, BODY_H);
    u8g2.drawBox(ICON_X + BODY_W, NUB_Y, NUB_W, NUB_H);
    for (int s = 0; s < fillSegs; s++) {
      int sx = SEG_X0 + s * (SEG_W + SEG_GAP);
      u8g2.drawBox(sx, SEG_Y, SEG_W, SEG_H);
    }
  }
}

// Header: title on the left, battery icon on the right.
// Title is truncated with "..." if it would overlap the icon.
// ICON_X (108) minus a 2 px gap = 106 px available for title text.
static void drawHeader(const String& title, int currentIndexZeroBased = -1, int totalCount = -1) {
  u8g2.setFont(u8g2_font_8x13_tf);

  const int y           = 15;    // text baseline
  const int MAX_TITLE_W = 109;   // px available before battery icon (ICON_X=111 minus 2px gap)

  // Measure and optionally truncate the title
  String t = title;
  if (u8g2.getStrWidth(t.c_str()) > MAX_TITLE_W) {
    // Shorten until "title..." fits
    while (t.length() > 0 && u8g2.getStrWidth((t + "...").c_str()) > MAX_TITLE_W) {
      t.remove(t.length() - 1);
    }
    t += "...";
  }

  u8g2.drawStr(0, y, t.c_str());

  // Battery icon — pass millis() so the blink animation works
  drawBatteryIcon(millis());

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



// ---------- shared list scrollbar ----------
// Draws the same triangle+track+thumb scrollbar used in table view,
// but parameterised for a simple indexed list (items/visibleRows).
// Call this inside the u8g2 page-buffer loop.
// SB_X=125, SB_W=3, track runs from y0 to y0+trackH
static void drawListScrollbar(int totalItems, int visibleRows, int scrollTop, int listY0, int listH) {
  if (totalItems <= visibleRows) return; // nothing to scroll, don't draw

  const int SB_X   = 125;
  const int SB_W   = 3;
  const int SB_MID = SB_X + SB_W / 2;
  const int TRI_H  = 4;

  const int TRK_TOP = listY0 + TRI_H + 1;
  const int TRK_BOT = listY0 + listH - TRI_H - 1;
  const int TRK_H   = TRK_BOT - TRK_TOP;

  bool canUp   = (scrollTop > 0);
  bool canDown = (scrollTop + visibleRows < totalItems);
  int  maxScroll = totalItems - visibleRows;

  // Up triangle
  if (canUp) {
    u8g2.drawTriangle(SB_MID, listY0,
                      SB_X,   listY0 + TRI_H,
                      SB_X + SB_W - 1, listY0 + TRI_H);
  } else {
    u8g2.drawLine(SB_MID, listY0, SB_X, listY0 + TRI_H);
    u8g2.drawLine(SB_MID, listY0, SB_X + SB_W - 1, listY0 + TRI_H);
    u8g2.drawLine(SB_X,   listY0 + TRI_H, SB_X + SB_W - 1, listY0 + TRI_H);
  }

  // Down triangle
  int dBase = listY0 + listH - TRI_H;
  int dApex = listY0 + listH;
  if (canDown) {
    u8g2.drawTriangle(SB_MID, dApex, SB_X, dBase, SB_X + SB_W - 1, dBase);
  } else {
    u8g2.drawLine(SB_MID, dApex, SB_X, dBase);
    u8g2.drawLine(SB_MID, dApex, SB_X + SB_W - 1, dBase);
    u8g2.drawLine(SB_X, dBase, SB_X + SB_W - 1, dBase);
  }

  // Centre track line
  if (TRK_H > 0) u8g2.drawVLine(SB_MID, TRK_TOP, TRK_H);

  // Thumb
  if (TRK_H > 0 && maxScroll > 0) {
    int thumbH     = max(3, TRK_H * visibleRows / totalItems);
    int thumbRange = TRK_H - thumbH;
    int thumbY     = TRK_TOP + (int)((long)thumbRange * scrollTop / maxScroll);
    if (thumbY + thumbH > TRK_BOT) thumbY = TRK_BOT - thumbH;
    u8g2.drawBox(SB_X, thumbY, SB_W, thumbH);
  }
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
      name = ellipsize(name, 17); // 17 leaves room for scrollbar strip

      if (idx == cursor) {
        u8g2.drawStr(0, y, "> ");
        u8g2.drawStr(14, y, name.c_str());
      } else {
        u8g2.drawStr(0, y, "  ");
        u8g2.drawStr(14, y, name.c_str());
      }
    }

    drawListScrollbar((int)catCount, rows, scrollTop, listY0, rows * rowH);
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
      shown = ellipsize(shown, 17); // 17 leaves room for scrollbar strip

      if (idx == fileCursor) {
        u8g2.drawStr(0, y, "> ");
        u8g2.drawStr(14, y, shown.c_str());
      } else {
        u8g2.drawStr(0, y, "  ");
        u8g2.drawStr(14, y, shown.c_str());
      }
    }

    drawListScrollbar((int)fileCount, rows, fileScrollTop, listY0, rows * rowH);
  } while (u8g2.nextPage());
}

void drawTable() {
  // Layout constants
  const int y0              = 30;   // top of text area (below header divider)
  const int lineH           = 14;   // px per line (12px font + breathing room)
  const int maxLinesOnScreen = (128 - y0) / lineH; // ~7 lines

  // Scrollbar geometry — 3px wide strip flush to the right edge
  const int SB_X     = 125; // left edge of scrollbar track
  const int SB_W     = 3;   // track width in pixels
  const int SB_TOP   = y0;
  const int SB_H     = 128 - y0; // full height of text area

  // Build word-wrapped lines (populates wrapLines[], sets wrapLineCount).
  uint16_t totalLines = buildWrappedLines(currentEntry);

  if (tableScrollLine < 0) tableScrollLine = 0;
  int16_t maxScroll = (int16_t)totalLines - (int16_t)maxLinesOnScreen;
  if (maxScroll < 0) maxScroll = 0;
  if (tableScrollLine > maxScroll) tableScrollLine = maxScroll;

  bool needsScrollbar = (totalLines > (uint16_t)maxLinesOnScreen);

  String hdr = stripCsvOrJsonExt(selectedFile);
  hdr = ellipsize(hdr, 20); // one char narrower to match WRAP_CHARS

  u8g2.firstPage();
  do {
    int cur = (rollCount > 0 ? (int)rollCount - 1 : 0);
    int tot = (rollCount > 0 ? (int)rollCount : 0);
    drawHeader(hdr, cur, tot);

    u8g2.setFont(u8g2_font_6x12_tf);

    if (currentEntry.length() == 0) {
      u8g2.drawStr(0, 40, "No entry read.");
      continue;
    }

    // Render visible text lines
    for (uint16_t li = 0; li < wrapLineCount; li++) {
      if ((int16_t)li < tableScrollLine) continue;
      if ((li - tableScrollLine) >= (uint16_t)maxLinesOnScreen) break;
      int y = y0 + (int)(li - tableScrollLine) * lineH;
      u8g2.drawStr(0, y, wrapLines[li].buf);
    }

    // Scrollbar — only drawn when content overflows
    if (needsScrollbar) {
      bool canScrollUp   = (tableScrollLine > 0);
      bool canScrollDown = (tableScrollLine < maxScroll);

      // Scroll track occupies the middle strip, leaving room for triangles at ends.
      // Triangle height = 4px, so track starts 4px in from top and ends 4px up from bottom.
      const int TRI_H   = 4;  // triangle height in pixels
      const int TRK_TOP = SB_TOP + TRI_H + 1;
      const int TRK_BOT = SB_TOP + SB_H - TRI_H - 1;
      const int TRK_H   = TRK_BOT - TRK_TOP;
      const int SB_MID  = SB_X + SB_W / 2; // horizontal centre of the strip

      // ── Up triangle (filled when scrollable, hollow when not) ──────────────
      // Apex at top, base at bottom — pointing up
      if (canScrollUp) {
        u8g2.drawTriangle(SB_MID, SB_TOP,           // apex
                          SB_X,   SB_TOP + TRI_H,   // base left
                          SB_X + SB_W - 1, SB_TOP + TRI_H); // base right
      } else {
        // hollow: just the outline
        u8g2.drawLine(SB_MID, SB_TOP, SB_X, SB_TOP + TRI_H);
        u8g2.drawLine(SB_MID, SB_TOP, SB_X + SB_W - 1, SB_TOP + TRI_H);
        u8g2.drawLine(SB_X,   SB_TOP + TRI_H, SB_X + SB_W - 1, SB_TOP + TRI_H);
      }

      // ── Down triangle (filled when scrollable, hollow when not) ────────────
      // Apex at bottom, base at top — pointing down
      int dBase = SB_TOP + SB_H - TRI_H; // y of base
      int dApex = SB_TOP + SB_H;         // y of apex
      if (canScrollDown) {
        u8g2.drawTriangle(SB_MID, dApex,
                          SB_X,   dBase,
                          SB_X + SB_W - 1, dBase);
      } else {
        u8g2.drawLine(SB_MID, dApex, SB_X, dBase);
        u8g2.drawLine(SB_MID, dApex, SB_X + SB_W - 1, dBase);
        u8g2.drawLine(SB_X,   dBase, SB_X + SB_W - 1, dBase);
      }

      // ── Track (thin centre line between the two triangles) ─────────────────
      if (TRK_H > 0) {
        u8g2.drawVLine(SB_MID, TRK_TOP, TRK_H);
      }

      // ── Thumb (filled rect on the centre line) ─────────────────────────────
      if (TRK_H > 0) {
        int thumbH = max(3, (int)((long)TRK_H * maxLinesOnScreen / (int)totalLines));
        int thumbRange = TRK_H - thumbH;
        int thumbY = TRK_TOP;
        if (maxScroll > 0) {
          thumbY = TRK_TOP + (int)((long)thumbRange * tableScrollLine / maxScroll);
        }
        if (thumbY + thumbH > TRK_BOT) thumbY = TRK_BOT - thumbH;
        // Draw thumb as a small filled box centred on the track line
        u8g2.drawBox(SB_X, thumbY, SB_W, thumbH);
      }
    }

  } while (u8g2.nextPage());
}


// About screen scroll position — file scope so it persists while the screen is open
int16_t aboutScrollLine = 0;

// Full about text. Use \n for intentional paragraph/line breaks.
// buildWrappedLines() handles both explicit \n and word-wrap at WRAP_CHARS (20 chars).
static const char ABOUT_TEXT[] =
  "GDMS Pocket was built as an inspirational storytelling companion to creatives of all sorts.\n"
  "\n"
  "GDMS is inspired by the minimalism and simplicity of classic TTRPGs and the amazing work of "
  "Kelsey Dionne, Ben Milton, Gavin Norman, and many more.\n"
  "\n"
  "GDMS was created in 2020 and ported to pocket version by Alexander Sousa in Pawtucket, "
  "Rhode Island in 2026.\n"
  "\n"
  "Special thanks to Kristina Michael, Claudia Sousa, Alexander Silva, Jake Vieira, Scout, "
  "and the Durfee High School Tabletop Club.";

void drawAbout() {
  const int y0             = 30;
  const int lineH          = 14;
  const int maxLines       = (128 - y0) / lineH; // ~7 visible lines

  uint16_t totalLines = buildWrappedLines(String(ABOUT_TEXT));

  // Clamp scroll
  if (aboutScrollLine < 0) aboutScrollLine = 0;
  int16_t maxScroll = (int16_t)totalLines - (int16_t)maxLines;
  if (maxScroll < 0) maxScroll = 0;
  if (aboutScrollLine > maxScroll) aboutScrollLine = maxScroll;

  bool needsScrollbar = (totalLines > (uint16_t)maxLines);

  u8g2.firstPage();
  do {
    drawHeader("About");
    u8g2.setFont(u8g2_font_6x12_tf);

    for (uint16_t li = 0; li < wrapLineCount; li++) {
      if ((int16_t)li < aboutScrollLine) continue;
      if ((li - aboutScrollLine) >= (uint16_t)maxLines) break;
      int y = y0 + (int)(li - aboutScrollLine) * lineH;
      u8g2.drawStr(0, y, wrapLines[li].buf);
    }

    if (needsScrollbar) {
      drawListScrollbar((int)totalLines, maxLines, aboutScrollLine, y0, maxLines * lineH);
    }
  } while (u8g2.nextPage());
}

// =================== SETTINGS PERSISTENCE ===================
#define SETTINGS_PATH "/DATA/settings.cfg"

void saveSettings() {
  // Remove old file first so we always write a clean copy
  if (SD.exists(SETTINGS_PATH)) SD.remove(SETTINGS_PATH);

  FsFile f = SD.open(SETTINGS_PATH, O_WRONLY | O_CREAT);
  if (!f) {
    Serial.println("saveSettings: open failed");
    return;
  }

  // Each line: key=value\n
  // Values stored as integers matching enum indices — simple and robust.
  f.print("buzzer=");   f.println((int)buzzerEnabled);
  f.print("buzvol=");   f.println((int)buzVolume);
  f.print("ledon=");    f.println((int)ledEnabled);
  f.print("ledpop=");   f.println((int)ledPopEnabled);
  f.print("ledbrt=");   f.println((int)ledBrightness);
  f.print("breathspd=");f.println((int)breathSpeed);
  f.print("sleeptmo="); f.println((int)sleepTimeout);

  f.close();
  Serial.println("saveSettings: saved.");
}

void loadSettings() {
  if (!SD.exists(SETTINGS_PATH)) {
    Serial.println("loadSettings: no settings file, using defaults.");
    return;
  }

  FsFile f = SD.open(SETTINGS_PATH, O_RDONLY);
  if (!f) {
    Serial.println("loadSettings: open failed, using defaults.");
    return;
  }

  const size_t BUF_SZ = 40;
  char buf[BUF_SZ];

  while (true) {
    int n = f.fgets(buf, (int)BUF_SZ);
    if (n <= 0) break;

    // Strip trailing newline/carriage-return
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (n == 0) continue;

    // Split on '='
    char* eq = strchr(buf, '=');
    if (!eq) continue;
    *eq = '\0';
    const char* key = buf;
    int  val = atoi(eq + 1);

    if (strcmp(key, "buzzer") == 0) {
      buzzerEnabled = (val != 0);
    } else if (strcmp(key, "buzvol") == 0) {
      if (val == BUZ_VOL_SOFT || val == BUZ_VOL_LOUD)
        buzVolume = (BuzVol)val;
    } else if (strcmp(key, "ledon") == 0) {
      ledEnabled = (val != 0);
    } else if (strcmp(key, "ledpop") == 0) {
      ledPopEnabled = (val != 0);
    } else if (strcmp(key, "ledbrt") == 0) {
      if (val >= LED_BRIGHT_LOW && val <= LED_BRIGHT_HIGH)
        ledBrightness = (LedBright)val;
    } else if (strcmp(key, "breathspd") == 0) {
      if (val >= BREATH_FAST && val <= BREATH_SLOW)
        breathSpeed = (BreathSpd)val;
    } else if (strcmp(key, "sleeptmo") == 0) {
      if (val >= SLEEP_5MIN && val <= SLEEP_NEVER)
        sleepTimeout = (SleepTimeout)val;
    }
    // Unknown keys are silently ignored — forward-compatible
  }

  f.close();
  Serial.println("loadSettings: loaded.");
}

void drawOptions() {
  // Each row: label on the left, < value > on the right.
  // Row layout: y starts at 30, 14px per row, 6x12 font.
  // Max label+value width = 21 chars total per row.

  // Build current value strings for each setting
  const char* buzOnStr  = buzzerEnabled              ? "ON"   : "OFF";
  const char* buzVolStr = (buzVolume == BUZ_VOL_LOUD) ? "LOUD" : "SOFT";
  const char* ledOnStr  = ledEnabled                  ? "ON"   : "OFF";
  const char* ledPopStr = ledPopEnabled               ? "ON"   : "OFF";
  const char* ledBrStr  = (ledBrightness == LED_BRIGHT_LOW) ? "LO"
                        : (ledBrightness == LED_BRIGHT_MED) ? "MED" : "HI";
  const char* bspStr    = (breathSpeed == BREATH_FAST) ? "FAST"
                        : (breathSpeed == BREATH_MED)  ? "MED" : "SLOW";
  const char* slpStr    = (sleepTimeout == SLEEP_5MIN)  ? "5min"
                        : (sleepTimeout == SLEEP_10MIN) ? "10min"
                        : (sleepTimeout == SLEEP_30MIN) ? "30min" : "Never";

  // Row order: Buzzer | Buz Vol | LED | LED Pop | LED Brt | Breathe | Sleep | About
  const char* labels[OPT_COUNT] = { "Buzzer","Buz Vol","LED","LED Pop","LED Brt","Breathe","Sleep","About" };
  const char* values[OPT_COUNT] = { buzOnStr, buzVolStr, ledOnStr, ledPopStr, ledBrStr, bspStr, slpStr, ">" };

  // Options list scrolls when it overflows the screen.
  const int listY0    = 30;
  const int rowH      = 14;
  const int visRows   = (128 - listY0) / rowH; // rows visible at once (~7)
  const int listH     = visRows * rowH;

  // Clamp optCursor and scroll window
  if (optCursor < 0) optCursor = 0;
  if (optCursor >= OPT_COUNT) optCursor = OPT_COUNT - 1;
  static int8_t optScrollTop = 0;
  if (optCursor < optScrollTop) optScrollTop = optCursor;
  if (optCursor >= optScrollTop + visRows) optScrollTop = optCursor - visRows + 1;

  u8g2.firstPage();
  do {
    drawHeader("Options");
    u8g2.setFont(u8g2_font_6x12_tf);

    for (int r = 0; r < visRows; r++) {
      int idx = optScrollTop + r;
      if (idx >= OPT_COUNT) break;
      int y = listY0 + r * rowH;
      bool selected = (idx == (int)optCursor);

      u8g2.drawStr(0, y, selected ? ">" : " ");
      u8g2.drawStr(8, y, labels[idx]);

      char valBuf[10];
      snprintf(valBuf, sizeof(valBuf), "<%s>", values[idx]);
      int valW = u8g2.getStrWidth(valBuf);
      u8g2.drawStr(120 - valW, y, valBuf); // leave 8px on right for scrollbar
    }

    drawListScrollbar(OPT_COUNT, visRows, optScrollTop, listY0, listH);
  } while (u8g2.nextPage());
}

void uiRedraw() {
  if      (mode == MODE_CATS)    drawCategories();
  else if (mode == MODE_FILES)   drawFiles();
  else if (mode == MODE_OPTIONS) drawOptions();
  else if (mode == MODE_ABOUT)   drawAbout();
  else                           drawTable();
}

// ---------- input handling ----------
void onButtonPressed(BtnId b) {
  lastInputMs    = millis();
  lastActivityMs = lastInputMs;

  // Wake from sleep if needed — redraw then continue handling the button
  if (deviceSleeping) {
    deviceSleeping = false;
    uiRedraw();
    // Still process the button that woke us (feel responsive)
  }

  // LED pop
  ledPop();

  // button chirp
  uint16_t base = 700;
  uint16_t freq = base + (uint16_t)b * 200;
  buzzerStart(freq, 15);

  if (mode == MODE_CATS) {
    if (b == BTN_UP)   cursor = (cursor <= 0) ? (int16_t)(catCount - 1) : cursor - 1;
    else if (b == BTN_DOWN) cursor = (cursor >= (int16_t)(catCount - 1)) ? 0 : cursor + 1;
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
    if (b == BTN_UP)   fileCursor = (fileCursor <= 0) ? (int16_t)(fileCount - 1) : fileCursor - 1;
    else if (b == BTN_DOWN) fileCursor = (fileCursor >= (int16_t)(fileCount - 1)) ? 0 : fileCursor + 1;
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
  } else if (mode == MODE_ABOUT) {
    if (b == BTN_B) {
      mode = MODE_OPTIONS;
    } else if (b == BTN_UP) {
      aboutScrollLine--;
    } else if (b == BTN_DOWN) {
      aboutScrollLine++;
    }
    // A is a no-op in About
  } else if (mode == MODE_OPTIONS) {
    if (b == BTN_UP) {
      optCursor = (optCursor <= 0) ? OPT_COUNT - 1 : optCursor - 1;
    } else if (b == BTN_DOWN) {
      optCursor = (optCursor >= OPT_COUNT - 1) ? 0 : optCursor + 1;
    } else if (b == BTN_A) {
      // Cycle the selected setting forward through its values
      switch (optCursor) {
        case 0: // Buzzer on/off
          buzzerEnabled = !buzzerEnabled;
          break;
        case 1: // Buzzer volume
          buzVolume = (buzVolume == BUZ_VOL_LOUD) ? BUZ_VOL_SOFT : BUZ_VOL_LOUD;
          break;
        case 2: // LED master on/off
          ledEnabled = !ledEnabled;
          if (!ledEnabled) analogWrite(LED_PIN, 0); // turn off immediately
          break;
        case 3: // LED pop on/off
          ledPopEnabled = !ledPopEnabled;
          break;
        case 4: // LED brightness: LOW -> MED -> HIGH -> LOW
          ledBrightness = (LedBright)((ledBrightness + 1) % 3);
          break;
        case 5: // Breath speed: FAST -> MED -> SLOW -> FAST
          breathSpeed = (BreathSpd)((breathSpeed + 1) % 3);
          break;
        case 6: // Sleep timeout: 5min -> 10min -> 30min -> Never -> 5min
          sleepTimeout = (SleepTimeout)((sleepTimeout + 1) % 4);
          break;
        case 7: // About — navigate in, don't save settings
          aboutScrollLine = 0;
          mode = MODE_ABOUT;
          uiRedraw();
          return;
      }
      saveSettings(); // persist immediately after any change
    } else if (b == BTN_B) {
      mode = prevMode; // return to wherever we came from
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

  loadSettings();
  scanCategories();

  // Take an initial battery reading so the header icon is accurate on first draw
  sampleBattery();
  lastBattSampleMs = millis();

  lastInputMs    = millis();
  lastActivityMs = lastInputMs;
  lastOledMs     = 0;

  uiRedraw();
}

void loop() {
  uint32_t now = millis();
  buzzerUpdate(now);

  // LED breathing + pop
  ledUpdate(now);

  // ── Battery sampling (every 5 s, non-blocking) ──────────────────────────
  if (now - lastBattSampleMs >= BATT_SAMPLE_INTERVAL_MS) {
    lastBattSampleMs = now;
    sampleBattery();
  }

  // ── Sleep timeout check ─────────────────────────────────────────────────
  {
    uint32_t tmo = SLEEP_TIMEOUT_TBL[sleepTimeout];
    if (!deviceSleeping && tmo > 0 && (now - lastActivityMs >= tmo)) {
      deviceSleeping = true;
      u8g2.firstPage();
      do { /* blank frame */ } while (u8g2.nextPage());
    }
  }

  // ── A+B combo → open options from any mode ──────────────────────────────
  {
    bool aHeld = (digitalRead(BTN_PINS[BTN_A]) == LOW);
    bool bHeld = (digitalRead(BTN_PINS[BTN_B]) == LOW);

    if (aHeld && bHeld) {
      if (!abComboFired) {
        // Require both to have been held for at least AB_COMBO_MS together
        if (abComboArmMs == 0) abComboArmMs = now;
        if (now - abComboArmMs >= AB_COMBO_MS && mode != MODE_OPTIONS) {
          abComboFired = true;
          prevMode = mode;
          mode = MODE_OPTIONS;
          ledPop();
          uiRedraw();
        }
      }
    } else {
      // One or both released — reset combo state
      abComboArmMs = 0;
      abComboFired = false;
    }
  }

  // read buttons — edge detect + hold-to-repeat for UP/DOWN
  for (int i = 0; i < 4; i++) {
    bool curPressed = (digitalRead(BTN_PINS[i]) == LOW);
    bool isRepeatBtn = (i == BTN_UP || i == BTN_DOWN);

    // ── Falling edge (new press) ──────────────────────────────────────────
    if (!wasPressed[i] && curPressed) {
      if (now - lastPressMs[i] >= DEBOUNCE_MS) {
        lastPressMs[i]  = now;
        wasPressed[i]   = true;
        holdActive[i]   = true;
        holdStartMs[i]  = now;
        lastRepeatMs[i] = now;
        onButtonPressed((BtnId)i);
        Serial.print(BTN_NAMES[i]);
        Serial.println(" PRESSED");
      }
    }

    // ── Hold-to-repeat (UP and DOWN only) ────────────────────────────────
    if (wasPressed[i] && curPressed && isRepeatBtn && holdActive[i]) {
      if (now - holdStartMs[i] >= HOLD_DELAY_MS) {
        if (now - lastRepeatMs[i] >= HOLD_REPEAT_MS) {
          lastRepeatMs[i] = now;
          onButtonPressed((BtnId)i);
        }
      }
    }

    // ── Rising edge (release) ─────────────────────────────────────────────
    if (wasPressed[i] && !curPressed) {
      wasPressed[i] = false;
      holdActive[i] = false;
    }
  }

  // Optional periodic refresh hook (generally unnecessary with page-buffer redraw-on-input)
  if ((now - lastOledMs >= OLED_PERIOD_MS) && (now - lastInputMs >= OLED_IDLE_AFTER_MS)) {
    lastOledMs = now;
    // uiRedraw();
  }

  delay(1);
}
