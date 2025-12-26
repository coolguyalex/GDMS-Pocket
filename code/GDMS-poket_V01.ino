#include <Wire.h>
#include <U8g2lib.h>

// OLED (keep your working rotation)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

// Buttons
const uint8_t BTN_A = 8;   // back
const uint8_t BTN_B = 9;   // commit
const uint8_t BTN_C = 10;  // advance

const uint16_t DEBOUNCE_MS = 35;

struct BtnState {
  uint8_t pin;
  bool raw;
  bool stable;
  bool lastStable;
  unsigned long lastChangeMs;
};

BtnState A{BTN_A, false, false, false, 0};
BtnState B{BTN_B, false, false, false, 0};
BtnState C{BTN_C, false, false, false, 0};

void updateBtn(BtnState &b) {
  bool newRaw = (digitalRead(b.pin) == LOW); // pressed = LOW
  if (newRaw != b.raw) {
    b.raw = newRaw;
    b.lastChangeMs = millis();
  }
  if (millis() - b.lastChangeMs > DEBOUNCE_MS) {
    b.lastStable = b.stable;
    b.stable = b.raw;
  }
}
bool justPressed(const BtnState &b) { return (b.stable && !b.lastStable); }

// ---------------- UI STATE ----------------
enum UiState {
  PAGE_NAMES,
  PAGE_DICE,
  DICE_MENU,
  DICE_ROLL
};

UiState ui = PAGE_NAMES;

// ---------------- NAMES (stub for now) ----------------
// Keep strings in flash to save RAM
const char n0[] PROGMEM = "Aldren Voss";
const char n1[] PROGMEM = "Brina Mourn";
const char n2[] PROGMEM = "Cato Merrow";
const char n3[] PROGMEM = "Dessa Flint";
const char n4[] PROGMEM = "Eryk Thane";
const char* const NAME_LIST[] PROGMEM = { n0, n1, n2, n3, n4 };
const uint8_t NAME_COUNT = sizeof(NAME_LIST) / sizeof(NAME_LIST[0]);

char currentName[22] = "Press B for name"; // fits 128px well

void pickRandomName() {
  uint8_t idx = random(NAME_COUNT);

  // copy PROGMEM string into RAM buffer
  const char* p = (const char*)pgm_read_ptr(&NAME_LIST[idx]);
  strncpy_P(currentName, p, sizeof(currentName) - 1);
  currentName[sizeof(currentName) - 1] = '\0';

  Serial.print(F("NAME: "));
  Serial.println(currentName);
}

// ---------------- DICE ----------------
const uint16_t DICE_SIDES[] = {4, 6, 8, 10, 12, 20, 100};
const uint8_t DICE_COUNT = sizeof(DICE_SIDES) / sizeof(DICE_SIDES[0]);
uint8_t diceIndex = 5; // default d20
uint16_t lastRoll = 0;

uint16_t rollDie(uint16_t sides) {
  // Arduino random(a,b) is [a, b)
  return (uint16_t)random(1, sides + 1);
}

// ---------------- DRAW ----------------
void drawNames() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("NAMES");

  u8g2.drawHLine(0, 16, 128);

  u8g2.setCursor(0, 34);
  u8g2.print(currentName);

  u8g2.setCursor(0, 62);
  u8g2.print("B:new  C:to DICE");
}

void drawDiceTop() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("DICE");

  u8g2.drawHLine(0, 16, 128);

  u8g2.setCursor(0, 34);
  u8g2.print("B:select die");

  u8g2.setCursor(0, 62);
  u8g2.print("C:to NAMES");
}

void drawDiceMenu() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("DICE MENU");

  u8g2.drawHLine(0, 16, 128);

  u8g2.setCursor(0, 36);
  u8g2.print("die: d");
  u8g2.print(DICE_SIDES[diceIndex]);

  u8g2.setCursor(0, 62);
  u8g2.print("A:back C:next B:roll");
}

void drawDiceRoll() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("ROLL d");
  u8g2.print(DICE_SIDES[diceIndex]);

  u8g2.drawHLine(0, 16, 128);

  // big-ish number
  u8g2.setFont(u8g2_font_logisoso24_tf);
  u8g2.setCursor(0, 52);
  u8g2.print(lastRoll);

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 62);
  u8g2.print("B:reroll  A:back");
}

void drawUI() {
  u8g2.firstPage();
  do {
    switch (ui) {
      case PAGE_NAMES: drawNames(); break;
      case PAGE_DICE:  drawDiceTop(); break;
      case DICE_MENU:  drawDiceMenu(); break;
      case DICE_ROLL:  drawDiceRoll(); break;
    }
  } while (u8g2.nextPage());
}

// ---------------- INPUT HANDLING ----------------
void handleInput() {
  if (justPressed(A)) {
    Serial.println(F("A"));
    if (ui == DICE_MENU) ui = PAGE_DICE;
    else if (ui == DICE_ROLL) ui = DICE_MENU;
    // On PAGE_NAMES / PAGE_DICE: A does nothing
  }

  if (justPressed(B)) {
    Serial.println(F("B"));
    if (ui == PAGE_NAMES) {
      pickRandomName();
    } else if (ui == PAGE_DICE) {
      ui = DICE_MENU;
    } else if (ui == DICE_MENU) {
      lastRoll = rollDie(DICE_SIDES[diceIndex]);
      Serial.print(F("ROLL d"));
      Serial.print(DICE_SIDES[diceIndex]);
      Serial.print(F(": "));
      Serial.println(lastRoll);
      ui = DICE_ROLL;
    } else if (ui == DICE_ROLL) {
      lastRoll = rollDie(DICE_SIDES[diceIndex]);
      Serial.print(F("REROLL d"));
      Serial.print(DICE_SIDES[diceIndex]);
      Serial.print(F(": "));
      Serial.println(lastRoll);
    }
  }

  if (justPressed(C)) {
    Serial.println(F("C"));
    if (ui == PAGE_NAMES) ui = PAGE_DICE;
    else if (ui == PAGE_DICE) ui = PAGE_NAMES;
    else if (ui == DICE_MENU) {
      diceIndex = (diceIndex + 1) % DICE_COUNT;
    }
    // In DICE_ROLL: C does nothing
  }
}

void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_C, INPUT_PULLUP);

  Wire.begin();
  u8g2.begin();

  // random seed from floating analog pin noise (good enough for POC)
  randomSeed(analogRead(A0));

  Serial.println(F("--- GDMS Pocket POC UI ---"));
  drawUI();
}

void loop() {
  updateBtn(A);
  updateBtn(B);
  updateBtn(C);

  handleInput();

  // redraw at a steady rate
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 60) {
    lastDraw = millis();
    drawUI();
  }
}
