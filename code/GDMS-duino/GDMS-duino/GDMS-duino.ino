#include <Wire.h>
#include <U8g2lib.h>


// This sketch is designed to run on the arduino nano with extremely limitted on-board program storage and RAM. 
//thus it focues primarily on simplified functions. 

//currently only a dice roller and some UI is implement. 

// ===================== OLED =====================
// Low-RAM page buffer mode (_1_)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

// ===================== PINS =====================
const uint8_t PIN_BTN_LEFT   = 2;  // page back (active LOW)
const uint8_t PIN_BTN_CENTER = 3;  // page forward (active LOW)
const uint8_t PIN_BTN_X      = 4;  // roll (active LOW)

const uint8_t PIN_BUZZER     = 5;  // passive buzzer (HW-508) signal
const uint8_t PIN_LED        = 6;  // PWM LED

// ===================== LED BREATH + POP =====================
const uint8_t LED_MIN = 5;
const uint8_t LED_MAX = 20;
const uint8_t LED_POP = 30;

const uint16_t BREATH_STEP_MS = 50;
const uint16_t POP_MS         = 60;

uint8_t ledLevel = LED_MIN;
int8_t  ledDir   = +1;
unsigned long lastBreathMs = 0;

bool popActive = false;
unsigned long popUntilMs = 0;

void ledInit() {
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, LED_MIN);
  Serial.println("ledInit");
}

void ledPop() {
  popActive = true;
  popUntilMs = millis() + POP_MS;
  analogWrite(PIN_LED, LED_POP);
  Serial.println("ledPop");
}

void ledUpdate() {
  unsigned long now = millis();

  if (popActive) {
    if ((long)(now - popUntilMs) >= 0) {
      popActive = false;
      analogWrite(PIN_LED, ledLevel);
    }
    return;
  }

  if (now - lastBreathMs >= BREATH_STEP_MS) {
    lastBreathMs = now;

    int16_t next = (int16_t)ledLevel + ledDir;
    if (next >= LED_MAX) { next = LED_MAX; ledDir = -1; }
    else if (next <= LED_MIN) { next = LED_MIN; ledDir = +1; }

    ledLevel = (uint8_t)next;
    analogWrite(PIN_LED, ledLevel);
  }
}

// ===================== BUZZER (PASSIVE via tone()) =====================
bool buzOn = false;
unsigned long buzUntilMs = 0;

void buzzerInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);   // silent on boot
    Serial.println("buzzerInit");
}

void buzzerBeepHz(uint16_t hz, uint16_t ms) {
  tone(PIN_BUZZER, hz);
  buzOn = true;
  buzUntilMs = millis() + ms;
}

void buzzerUpdate() {
  if (!buzOn) return;
  if ((long)(millis() - buzUntilMs) >= 0) {
    buzOn = false;
    noTone(PIN_BUZZER);
      Serial.println("buzzerUpdate");
  }
}

// Optional: pitch per page
uint16_t freqForPage(uint8_t p) {
  switch (p) {
    case 0: return 988;  // D4  (B5)
    case 1: return 880;  // D6  (A5)
    case 2: return 784;  // D8  (G5)
    case 3: return 698;  // D10 (F5)
    case 4: return 659;  // D12 (E5)
    case 5: return 587;  // D20 (D5)
    case 6: return 523;  // D100(C5)
    default: return 660;
  }
}

// ===================== BUTTONS (DEBOUNCE, NO STRUCTS) =====================
const unsigned long DEBOUNCE_MS = 30;

// per-button state
bool left_lastReading=false,   left_stablePressed=false,   left_latched=false;
bool ctr_lastReading=false,    ctr_stablePressed=false,    ctr_latched=false;
bool x_lastReading=false,      x_stablePressed=false,      x_latched=false;

unsigned long left_lastFlipMs=0, ctr_lastFlipMs=0, x_lastFlipMs=0;

void buttonsInit() {
  pinMode(PIN_BTN_LEFT,   INPUT_PULLUP);
  pinMode(PIN_BTN_CENTER, INPUT_PULLUP);
  pinMode(PIN_BTN_X,      INPUT_PULLUP);
}

bool justPressed(uint8_t pin,
                 bool &lastReading,
                 bool &stablePressed,
                 bool &latched,
                 unsigned long &lastFlipMs)
{
  bool readingPressed = (digitalRead(pin) == LOW);

  if (readingPressed != lastReading) {
    lastReading = readingPressed;
    lastFlipMs = millis();
  }

  if ((millis() - lastFlipMs) > DEBOUNCE_MS) {
    stablePressed = lastReading;
  }

  if (stablePressed && !latched) {
    latched = true;
    return true;
  }

  if (!stablePressed) latched = false;
  return false;
}

// ===================== DICE PAGES =====================
enum Page : uint8_t {
  P_D4 = 0,
  P_D6,
  P_D8,
  P_D10,
  P_D12,
  P_D20,
  P_D100,
  P_COUNT
};

uint8_t page = P_D20;

uint16_t lastRoll = 0;
bool hasRoll = false;

uint16_t rollDie(uint16_t sides) {
  return (uint16_t)random(sides) + 1; // 1..sides
}

uint16_t sidesForPage(uint8_t p) {
  switch (p) {
    case P_D4:   return 4;
    case P_D6:   return 6;
    case P_D8:   return 8;
    case P_D10:  return 10;
    case P_D12:  return 12;
    case P_D20:  return 20;
    case P_D100: return 100;
    default:     return 20;
  }
}

void doRollForPage() {
  uint16_t sides = sidesForPage(page);
  lastRoll = rollDie(sides);
  hasRoll = true;
}

// ===================== UI DRAW =====================
const char label_d4[]    PROGMEM = "D4";
const char label_d6[]    PROGMEM = "D6";
const char label_d8[]    PROGMEM = "D8";
const char label_d10[]   PROGMEM = "D10";
const char label_d12[]   PROGMEM = "D12";
const char label_d20[]   PROGMEM = "D20";
const char label_d100[]  PROGMEM = "D100";

const char* const PAGE_LABELS[] PROGMEM = {
  label_d4, label_d6, label_d8, label_d10, label_d12, label_d20, label_d100
};

void getPageLabel(char* out, uint8_t outSize) {
  out[0] = '\0';
  const char* p = (const char*)pgm_read_ptr(&PAGE_LABELS[page]);
  strncpy_P(out, p, outSize - 1);
  out[outSize - 1] = '\0';
}

// Minimal UI: big label + big number, tiny hints
void drawScreen() {
  char title[8];
  getPageLabel(title, sizeof(title));

  u8g2.firstPage();
  do {
    // Big die label at top-left
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, title);

    // Big roll centered-ish
    if (hasRoll) {
      char buf[8];
      uint16_t v = lastRoll;
      uint8_t n = 0;
      if (v >= 100) buf[n++] = '0' + (v / 100);
      if (v >= 10)  buf[n++] = '0' + ((v / 10) % 10);
      buf[n++] = '0' + (v % 10);
      buf[n] = '\0';

      u8g2.setFont(u8g2_font_logisoso20_tf);   // larger than 24
      // y=52 tends to fit nicely with this font on 64px height
      u8g2.drawStr(30, 54, buf);
    } else {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(0, 40, "Press X to roll");
    }

    // Footer hints
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(0, 64, "<back    forward>    roll");
  } while (u8g2.nextPage());
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(9600);
  Serial.println("Setup");
  Wire.begin();
  u8g2.begin();

  ledInit();
  buzzerInit();
  buttonsInit();

  randomSeed(analogRead(A0));

  drawScreen();
}

void loop() {
  ledUpdate();
  buzzerUpdate();

  // Next page
  if (justPressed(PIN_BTN_CENTER, ctr_lastReading, ctr_stablePressed, ctr_latched, ctr_lastFlipMs)) {
    page = (page + 1) % P_COUNT;
    hasRoll = false;                 // clear old roll when switching pages
    buzzerBeepHz(880, 10);           // click
    drawScreen();
    Serial.println("next page");
  }

  // Prev page
  if (justPressed(PIN_BTN_LEFT, left_lastReading, left_stablePressed, left_latched, left_lastFlipMs)) {
    page = (page == 0) ? (P_COUNT - 1) : (page - 1);
    hasRoll = false;
    buzzerBeepHz(880, 10);           // click
    drawScreen();
    Serial.println("prev page");
  }

  // Roll
  if (justPressed(PIN_BTN_X, x_lastReading, x_stablePressed, x_latched, x_lastFlipMs)) {
    ledPop();
    doRollForPage();
    buzzerBeepHz(freqForPage(page), 18);  // pitch mapped to die
    drawScreen();
    Serial.println("Roll");
  }
}
