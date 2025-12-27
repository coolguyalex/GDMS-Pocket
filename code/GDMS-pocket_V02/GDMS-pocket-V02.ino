#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

const uint8_t SD_CS = 5;

// Folder we browse
const char* DIRPATH = "/DICE";

// Buttons (active LOW, INPUT_PULLUP)
const uint8_t BTN_PREV = 2; // left
const uint8_t BTN_NEXT = 3; // middle
const uint8_t BTN_X    = 4; // X = roll

// Small line buffers (keep tight for SRAM)
char currLine[33];
char chosen[33];

// Current selected file (8.3 short names)
char currentFile[13];      // e.g. "D20.TXT"
char currentPath[32];      // e.g. "/DICE/D20.TXT"
uint16_t fileCount = 0;
uint16_t currentIndex = 0;

// ---------- OLED helpers ----------
void draw2(const char* l1, const char* l2) {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, l1);
    if (l2) u8g2.drawStr(0, 28, l2);
  } while (u8g2.nextPage());
}

void showSelected() {
  // Display selected file + hint
  draw2("TABLE:", currentFile);
}

// ---------- SD + file browsing ----------
bool isTxtFile(File &f) {
  if (f.isDirectory()) return false;

  // SD.h name() is 8.3; check .TXT case-insensitively
  const char* n = f.name();
  const char* dot = strrchr(n, '.');
  if (!dot) return false;

  // Compare extension to "TXT" (case-insensitive)
  char a = dot[1], b = dot[2], c = dot[3];
  if (!a || !b || !c) return false;

  auto up = [](char ch) -> char {
    if (ch >= 'a' && ch <= 'z') return ch - 32;
    return ch;
  };

  return (up(a) == 'T' && up(b) == 'X' && up(c) == 'T');
}

uint16_t countTxtFiles(const char* dirPath) {
  File dir = SD.open(dirPath);
  if (!dir) return 0;

  uint16_t count = 0;
  dir.rewindDirectory();

  while (true) {
    File e = dir.openNextFile();
    if (!e) break;
    if (isTxtFile(e)) count++;
    e.close();
  }

  dir.close();
  return count;
}

// Get the Nth .TXT file (0-based) from dirPath into outName
bool getTxtFileByIndex(const char* dirPath, uint16_t index, char* outName, uint8_t outSize) {
  File dir = SD.open(dirPath);
  if (!dir) return false;

  uint16_t i = 0;
  bool found = false;

  dir.rewindDirectory();
  while (true) {
    File e = dir.openNextFile();
    if (!e) break;

    if (isTxtFile(e)) {
      if (i == index) {
        strncpy(outName, e.name(), outSize - 1);
        outName[outSize - 1] = '\0';
        found = true;
        e.close();
        break;
      }
      i++;
    }
    e.close();
  }

  dir.close();
  return found;
}

void buildCurrentPath() {
  // "/DICE/" + "D20.TXT"
  strncpy(currentPath, DIRPATH, sizeof(currentPath) - 1);
  currentPath[sizeof(currentPath) - 1] = '\0';

  size_t len = strlen(currentPath);
  if (len < sizeof(currentPath) - 2 && currentPath[len - 1] != '/') {
    currentPath[len] = '/';
    currentPath[len + 1] = '\0';
  }

  strncat(currentPath, currentFile, sizeof(currentPath) - strlen(currentPath) - 1);
}

// ---------- line reading + reservoir roll ----------
bool readLine(File &f, char* out, uint8_t outSize) {
  uint8_t i = 0;
  bool gotAny = false;

  while (f.available()) {
    char c = f.read();
    if (c == '\r') continue;
    if (c == '\n') break;

    gotAny = true;
    if (i < outSize - 1) out[i++] = c;
  }

  out[i] = '\0';
  return gotAny;
}

bool rollOnceFromPath(const char* path, char* out, uint8_t outSize) {
  File f = SD.open(path);
  if (!f) return false;

  uint16_t n = 0;
  out[0] = '\0';

  while (readLine(f, currLine, sizeof(currLine))) {
    n++;
    if (random(n) == 0) {
      strncpy(out, currLine, outSize - 1);
      out[outSize - 1] = '\0';
    }
  }

  f.close();
  return (n > 0);
}

// ---------- button debounce (generic) ----------
struct DebounceBtn {
  uint8_t pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChangeMs;
  bool latched;
};

const unsigned long DEBOUNCE_MS = 30;

void btnInit(DebounceBtn &b, uint8_t p) {
  b.pin = p;
  pinMode(b.pin, INPUT_PULLUP);
  b.lastReading = (digitalRead(b.pin) == LOW);
  b.stableState = b.lastReading;
  b.lastChangeMs = millis();
  b.latched = false;
}

// returns true once per new press
bool btnJustPressed(DebounceBtn &b) {
  bool reading = (digitalRead(b.pin) == LOW);

  if (reading != b.lastReading) {
    b.lastReading = reading;
    b.lastChangeMs = millis();
  }

  if ((millis() - b.lastChangeMs) > DEBOUNCE_MS) {
    // stable
    if (b.stableState != b.lastReading) {
      b.stableState = b.lastReading;
      if (b.stableState == true) { // became pressed
        if (!b.latched) {
          b.latched = true;
          return true;
        }
      } else {
        b.latched = false; // released
      }
    }
  }

  // if held, keep latched
  if (b.stableState == false) b.latched = false;

  return false;
}

DebounceBtn bPrev, bNext, bX;

// ---------- setup/loop ----------
void setup() {
  Wire.begin();
  u8g2.begin();

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  btnInit(bPrev, BTN_PREV);
  btnInit(bNext, BTN_NEXT);
  btnInit(bX,    BTN_X);

  randomSeed(analogRead(A0));

  draw2("Init SD...", nullptr);
  if (!SD.begin(SD_CS)) {
    draw2("SD INIT FAIL", "Power SD @ 5V");
    while (1) {}
  }

  draw2("Scan /DICE...", nullptr);
  fileCount = countTxtFiles(DIRPATH);
  if (fileCount == 0) {
    draw2("NO .TXT FILES", "in /DICE");
    while (1) {}
  }

  currentIndex = 0;
  if (!getTxtFileByIndex(DIRPATH, currentIndex, currentFile, sizeof(currentFile))) {
    draw2("BROWSE FAIL", "can't read dir");
    while (1) {}
  }
  buildCurrentPath();
  showSelected();
}

void loop() {
  if (btnJustPressed(bNext)) {
    currentIndex = (currentIndex + 1) % fileCount;
    if (getTxtFileByIndex(DIRPATH, currentIndex, currentFile, sizeof(currentFile))) {
      buildCurrentPath();
      showSelected();
    } else {
      draw2("NEXT FAIL", "dir read err");
    }
  }

  if (btnJustPressed(bPrev)) {
    currentIndex = (currentIndex == 0) ? (fileCount - 1) : (currentIndex - 1);
    if (getTxtFileByIndex(DIRPATH, currentIndex, currentFile, sizeof(currentFile))) {
      buildCurrentPath();
      showSelected();
    } else {
      draw2("PREV FAIL", "dir read err");
    }
  }

  if (btnJustPressed(bX)) {
    draw2("Rolling...", currentFile);

    if (rollOnceFromPath(currentPath, chosen, sizeof(chosen))) {
      draw2("ROLL:", chosen);
    } else {
      draw2("ROLL FAIL", "open/read err");
    }
  }
}
