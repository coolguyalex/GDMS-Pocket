#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

const uint8_t SD_CS = 5;
const char* FILEPATH = "/DICE/D20.TXT";

// X button (active LOW)
const uint8_t BTN_X = 4;

char currLine[33];
char chosen[33];

void draw2(const char* l1, const char* l2) {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, l1);
    if (l2) u8g2.drawStr(0, 28, l2);
  } while (u8g2.nextPage());
}

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

bool rollOnce(char* out, uint8_t outSize) {
  File f = SD.open(FILEPATH);
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

// simple debounce: detect a "new press"
bool wasPressed = false;
unsigned long lastChangeMs = 0;
const unsigned long DEBOUNCE_MS = 30;

bool xJustPressed() {
  bool reading = (digitalRead(BTN_X) == LOW);

  if (reading != wasPressed) {
    lastChangeMs = millis();
    wasPressed = reading;
  }

  // stable LOW for debounce window = a press event
  static bool latched = false;
  if (wasPressed && !latched && (millis() - lastChangeMs) > DEBOUNCE_MS) {
    latched = true;
    return true;
  }

  // release resets latch
  if (!wasPressed) latched = false;

  return false;
}

void setup() {
  Wire.begin();
  u8g2.begin();

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  pinMode(BTN_X, INPUT_PULLUP);

  // Seed randomness (A0 floating is fine)
  randomSeed(analogRead(A0));

  draw2("Init SD...", nullptr);
  if (!SD.begin(SD_CS)) {
    draw2("SD INIT FAIL", "Power SD @ 5V");
    while (1) {}
  }

  draw2("Ready", "Press X to roll");
}

void loop() {
  if (xJustPressed()) {
    draw2("Rolling...", nullptr);

    if (rollOnce(chosen, sizeof(chosen))) {
      draw2("ROLL:", chosen);
    } else {
      draw2("ROLL FAIL", "can't open/read");
    }
  }
}
