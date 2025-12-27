#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

// --- LED breathing + pop ---
const uint8_t LED_PIN = 6;

// Breath range (keep low so it's not blinding)
const uint8_t LED_MIN = 5;     // dim floor
const uint8_t LED_MAX = 20;    // dim ceiling (breath peak)

// Pop on press (a bit brighter than breath peak, but still not full)
const uint8_t LED_POP = 30;    // adjust to taste

// Timing
const uint16_t BREATH_STEP_MS = 50; // 25ms * (LED_MAX-LED_MIN)*2 ≈ ~700ms..1.2s depending on range
const uint16_t POP_MS = 60;         // how long the pop lasts

uint8_t ledLevel = LED_MIN;
int8_t ledDir = +1;
unsigned long lastBreathMs = 0;

bool popActive = false;
unsigned long popUntilMs = 0;

void ledInit() {
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, LED_MIN);
}

void ledPop() {
  popActive = true;
  popUntilMs = millis() + POP_MS;
  analogWrite(LED_PIN, LED_POP);
}

void ledUpdate() {
  unsigned long now = millis();

  // If pop is active, hold until it expires
  if (popActive) {
    if ((long)(now - popUntilMs) >= 0) {
      popActive = false;
      // fall back to breathing at current ledLevel
      analogWrite(LED_PIN, ledLevel);
    }
    return;
  }

  // Breath update (non-blocking)
  if (now - lastBreathMs >= BREATH_STEP_MS) {
    lastBreathMs = now;

    // advance level
    int16_t next = (int16_t)ledLevel + ledDir;

    if (next >= LED_MAX) {
      next = LED_MAX;
      ledDir = -1;
    } else if (next <= LED_MIN) {
      next = LED_MIN;
      ledDir = +1;
    }

    ledLevel = (uint8_t)next;
    analogWrite(LED_PIN, ledLevel);
  }
}


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

// void ledBlink() {
//   analogWrite(LED_PIN, LED_BLINK);
//   delay(40);
//   analogWrite(LED_PIN, LED_IDLE);
// }

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


  ledInit();

//  pinMode(LED_PIN, OUTPUT);
//  analogWrite(LED_PIN, LED_IDLE);


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
  ledUpdate();

  if (xJustPressed()) {
    ledPop();                 // brighten pop on press
    draw2("Rolling...", nullptr);

    if (rollOnce(chosen, sizeof(chosen))) {
      draw2("ROLL:", chosen);
    } else {
      draw2("ROLL FAIL", "can't open/read");
    }
  }
}
