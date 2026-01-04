#include <Wire.h>
#include <U8g2lib.h>

// ===================== OLED (U8x8 text mode) =====================
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

// ===================== PINS =====================
const uint8_t PIN_BTN_LEFT   = 2;  // back (active LOW)
const uint8_t PIN_BTN_CENTER = 3;  // forward (active LOW)
const uint8_t PIN_BTN_X      = 4;  // select / action (active LOW)

const uint8_t PIN_BUZZER     = 5;  // passive buzzer signal
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
}

void ledPop() {
  popActive = true;
  popUntilMs = millis() + POP_MS;
  analogWrite(PIN_LED, LED_POP);
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

// ===================== BUZZER =====================
bool buzOn = false;
unsigned long buzUntilMs = 0;

void buzzerInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);
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
  }
}

// ===================== BUTTONS (DEBOUNCE, NO STRUCTS) =====================
const unsigned long DEBOUNCE_MS = 30;

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

// helper: stable pressed state (for combos like LEFT+CENTER)
bool isHeldLeft()   { return left_stablePressed; }
bool isHeldCenter() { return ctr_stablePressed;  }
bool isHeldX()      { return x_stablePressed;    }

// ===================== SYSTEM: MODES + APPLETS =====================
enum UiMode : uint8_t { MODE_HOME = 0, MODE_APPLET = 1 };
UiMode mode = MODE_HOME;

enum Applet : uint8_t { APP_DICE = 0, APP_NAMES = 1, APP_ROOM = 2, APPLET_COUNT = 3 };
uint8_t applet = APP_DICE;

// ===================== DICE APPLET =====================
enum DicePage : uint8_t {
  P_D4 = 0,
  P_D6,
  P_D8,
  P_D10,
  P_D12,
  P_D20,
  P_D100,
  P_DICE_COUNT
};

uint8_t dicePage = P_D20;
uint16_t lastRoll = 0;
bool hasRoll = false;

uint16_t rollDie(uint16_t sides) {
  return (uint16_t)random(sides) + 1;
}

uint16_t sidesForDicePage(uint8_t p) {
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

uint16_t freqForDicePage(uint8_t p) {
  switch (p) {
    case P_D4: return 988;
    case P_D6: return 880;
    case P_D8: return 784;
    case P_D10:return 698;
    case P_D12:return 659;
    case P_D20:return 587;
    case P_D100:return 523;
    default: return 660;
  }
}

const char label_d4[]   PROGMEM = "D4";
const char label_d6[]   PROGMEM = "D6";
const char label_d8[]   PROGMEM = "D8";
const char label_d10[]  PROGMEM = "D10";
const char label_d12[]  PROGMEM = "D12";
const char label_d20[]  PROGMEM = "D20";
const char label_d100[] PROGMEM = "D100";

const char* const DICE_LABELS[] PROGMEM = {
  label_d4, label_d6, label_d8, label_d10, label_d12, label_d20, label_d100
};

void getDiceLabel(char* out, uint8_t outSize) {
  const char* p = (const char*)pgm_read_ptr(&DICE_LABELS[dicePage]);
  strncpy_P(out, p, outSize - 1);
  out[outSize - 1] = '\0';
}

// ===================== NAMES APPLET (simple list for now) =====================
const char n0[] PROGMEM = "Hera";
const char n1[] PROGMEM = "Torin";
const char n2[] PROGMEM = "Ginny";
const char n3[] PROGMEM = "Gant";
const char n4[] PROGMEM = "Olga";
const char n5[] PROGMEM = "Dendor";
const char n6[] PROGMEM = "Ygrid";
const char n7[] PROGMEM = "Pike";
const char n8[] PROGMEM = "Sarda";
const char n9[] PROGMEM = "Brigg";
const char n10[] PROGMEM = "Zorli";
const char n11[] PROGMEM = "Yorin";
const char n12[] PROGMEM = "Jorgena";
const char n13[] PROGMEM = "Trogin";
const char n14[] PROGMEM = "Riga";
const char n15[] PROGMEM = "Barton";
const char n16[] PROGMEM = "Katrina";
const char n17[] PROGMEM = "Egrim";
const char n18[] PROGMEM = "Elsa";
const char n19[] PROGMEM = "Skylark";
const char n20[] PROGMEM = "Orgo";
const char n21[] PROGMEM = "Fargrim";
const char n22[] PROGMEM = "Gruner";
const char n23[] PROGMEM = "Torbin";
const char n24[] PROGMEM = "Malik";
const char n25[] PROGMEM = "Flea";
const char n26[] PROGMEM = "Crow";
const char n27[] PROGMEM = "Rusty";
const char n28[] PROGMEM = "Spike";
const char n29[] PROGMEM = "Frond";
const char n30[] PROGMEM = "Yuorg";
const char n31[] PROGMEM = "Willow";
const char n32[] PROGMEM = "Blinky";
const char n33[] PROGMEM = "Red";
const char n34[] PROGMEM = "Papa";
const char n35[] PROGMEM = "Slim";
const char n36[] PROGMEM = "Shorty";
const char n37[] PROGMEM = "Oak";
const char n38[] PROGMEM = "Shadow";
const char n39[] PROGMEM = "Sunny";
const char n40[] PROGMEM = "Morg";

const char* const NAME_LIST[] PROGMEM = {
  n0,n1,n2,n3,n4,n5,n6,n7,n8,n9,n10,n11,n12,n13,n14,n15,n16,n17,n18,n19, n20, n21, n22, n23, n24, n25, n26, n27, n28, n29, n30, n31, n32, n33, n34, n35, n36, n37, n38, n39, n40
};

const uint8_t NAME_COUNT = sizeof(NAME_LIST) / sizeof(NAME_LIST[0]);

char nameBuf[16];   // fixed buffer for chosen name (<=15 chars)
bool hasName = false;

void pickRandomName() {
  uint8_t idx = (uint8_t)random(NAME_COUNT);
  const char* p = (const char*)pgm_read_ptr(&NAME_LIST[idx]);
  strncpy_P(nameBuf, p, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = '\0';
  hasName = true;
}


//======================ROOM APPLET =======================

char room_size[18];
char room_feat[18];
char room_threat[18];
char room_exit[18];
bool hasRoom = false;

// ===================== ROOM APPLET (chained mini tables) =====================
const char rs0[] PROGMEM = "Cramped";
const char rs1[] PROGMEM = "Small";
const char rs2[] PROGMEM = "Wide";
const char rs3[] PROGMEM = "Long";
const char rs4[] PROGMEM = "Vaulted";

const char* const ROOM_SIZE[] PROGMEM = { rs0, rs1, rs2, rs3, rs4 };
const uint8_t ROOM_SIZE_N = sizeof(ROOM_SIZE) / sizeof(ROOM_SIZE[0]);

const char rf0[] PROGMEM = "Wet stones";
const char rf1[] PROGMEM = "Cold draft";
const char rf2[] PROGMEM = "Ash on floor";
const char rf3[] PROGMEM = "Mosaic tiles";
const char rf4[] PROGMEM = "Broken statue";

const char* const ROOM_FEAT[] PROGMEM = { rf0, rf1, rf2, rf3, rf4 };
const uint8_t ROOM_FEAT_N = sizeof(ROOM_FEAT) / sizeof(ROOM_FEAT[0]);

const char rt0[] PROGMEM = "Rats";
const char rt1[] PROGMEM = "Goblin scout";
const char rt2[] PROGMEM = "Tripwire";
const char rt3[] PROGMEM = "Loose stones";
const char rt4[] PROGMEM = "Odd whispers";

const char* const ROOM_THREAT[] PROGMEM = { rt0, rt1, rt2, rt3, rt4 };
const uint8_t ROOM_THREAT_N = sizeof(ROOM_THREAT) / sizeof(ROOM_THREAT[0]);

const char re0[] PROGMEM = "Door north";
const char re1[] PROGMEM = "Tunnel east";
const char re2[] PROGMEM = "Stairs down";
const char re3[] PROGMEM = "Crack west";
const char re4[] PROGMEM = "Door sealed";

const char* const ROOM_EXIT[] PROGMEM = { re0, re1, re2, re3, re4 };
const uint8_t ROOM_EXIT_N = sizeof(ROOM_EXIT) / sizeof(ROOM_EXIT[0]);

void pickFromTable(char* out, uint8_t outSize, const char* const* table, uint8_t n) {
  uint8_t idx = (uint8_t)random(n);
  const char* p = (const char*)pgm_read_ptr(&table[idx]);
  strncpy_P(out, p, outSize - 1);
  out[outSize - 1] = '\0';
}

void generateRoom() {
  pickFromTable(room_size,   sizeof(room_size),   ROOM_SIZE,   ROOM_SIZE_N);
  pickFromTable(room_feat,   sizeof(room_feat),   ROOM_FEAT,   ROOM_FEAT_N);
  pickFromTable(room_threat, sizeof(room_threat), ROOM_THREAT, ROOM_THREAT_N);
  pickFromTable(room_exit,   sizeof(room_exit),   ROOM_EXIT,   ROOM_EXIT_N);
  hasRoom = true;
}



// ===================== UI HELPERS =====================
// Center 1x text on a 16-col row
void printCentered1x(uint8_t row, const char* s) {
  uint8_t len = (uint8_t)strlen(s);
  if (len > 16) len = 16;
  uint8_t col = (len >= 16) ? 0 : (uint8_t)((16 - len) / 2);
  u8x8.drawString(col, row, s);
}

// Center 2x2 text: max 8 chars across (since each char uses 2 columns)
void printCentered2x2(uint8_t row, const char* s) {
  uint8_t len = (uint8_t)strlen(s);
  if (len > 8) len = 8;
  uint8_t colsUsed = (uint8_t)(len * 2);
  uint8_t col = (colsUsed >= 16) ? 0 : (uint8_t)((16 - colsUsed) / 2);
  u8x8.draw2x2String(col, row, s);
}

// ===================== DRAW SCREENS =====================
void drawHome() {
  u8x8.clearDisplay();
  u8x8.drawString(0, 0, "GDMS Pocket");

  // Show selected applet
const char* label = (applet == APP_DICE) ? "DICE" :
                    (applet == APP_NAMES) ? "NAMES" : "ROOM";

char line[16];
strcpy(line, "> ");
strcat(line, label);
strcat(line, " <");
printCentered1x(3, line);


  u8x8.drawString(0, 7, "<  >  X=enter");
}

void drawDice() {
  u8x8.clearDisplay();

  char title[8];
  getDiceLabel(title, sizeof(title));
  u8x8.drawString(0, 0, "DICE");
  u8x8.drawString(12, 0, title);

  if (hasRoll) {
    char buf[6]; // up to 100
    uint16_t v = lastRoll;
    uint8_t n = 0;
    if (v >= 100) buf[n++] = '0' + (v / 100);
    if (v >= 10)  buf[n++] = '0' + ((v / 10) % 10);
    buf[n++] = '0' + (v % 10);
    buf[n] = '\0';

    printCentered2x2(2, buf);
  } else {
    printCentered1x(3, "X to roll");
  }

  u8x8.drawString(0, 7, "<  >  X=roll");
}

void drawNames() {
  u8x8.clearDisplay();

  u8x8.drawString(0, 0, "NAMES");

  if (hasName) {
    // Name can be shown 2x2 if short enough
    if (strlen(nameBuf) <= 8) {
      printCentered2x2(2, nameBuf);
    } else {
      printCentered1x(3, nameBuf);
    }
  } else {
    printCentered1x(3, "X for name");
  }

  u8x8.drawString(0, 7, "X=pick  L+R=back");
}

void drawRoom() {
  u8x8.clearDisplay();
  u8x8.drawString(0, 0, "ROOM");

  if (!hasRoom) {
    printCentered1x(3, "X=generate");
  } else {
    // 4 lines, simple and readable
    u8x8.drawString(0, 2, room_size);
    u8x8.drawString(0, 3, room_feat);
    u8x8.drawString(0, 4, room_threat);
    u8x8.drawString(0, 5, room_exit);
  }

  u8x8.drawString(0, 7, "X=gen  L+R=back");
}


void drawUi() {
  if (mode == MODE_HOME) {
    drawHome();
  } else {
    if (applet == APP_DICE) drawDice();
    else if (applet == APP_NAMES) drawNames();
    else drawRoom();
  }
}

// ===================== SETUP / LOOP =====================
void drawSplash() {
  u8x8.clearDisplay();
  u8x8.drawString(0, 0, "Goblinoid");
  u8x8.drawString(0, 1, "Dungeon");
  u8x8.drawString(0, 2, "Mastering");
  u8x8.drawString(0, 3, "System");
  u8x8.drawString(0, 5, "-Alexander Sousa");
  u8x8.drawString(0, 6, "2025");
}

void setup() {
  Wire.begin();

  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  ledInit();
  buzzerInit();
  buttonsInit();

  randomSeed(analogRead(A0));

  drawSplash();
  delay(2000);

  drawUi();
}

void loop() {
  ledUpdate();
  buzzerUpdate();

  // update stable button states by calling justPressed (even if we ignore the event)
  bool evR = justPressed(PIN_BTN_CENTER, ctr_lastReading, ctr_stablePressed, ctr_latched, ctr_lastFlipMs);
  bool evL = justPressed(PIN_BTN_LEFT,   left_lastReading, left_stablePressed, left_latched, left_lastFlipMs);
  bool evX = justPressed(PIN_BTN_X,      x_lastReading, x_stablePressed, x_latched, x_lastFlipMs);

  // exit combo: LEFT + CENTER held at same time
  if (mode == MODE_APPLET && isHeldLeft() && isHeldCenter()) {
    mode = MODE_HOME;
    buzzerBeepHz(440, 20);
    drawUi();
    return;
  }

  if (mode == MODE_HOME) {
    if (evR) {
      applet = (applet + 1) % APPLET_COUNT;
      buzzerBeepHz(880, 10);
      drawUi();
    }
    if (evL) {
      applet = (applet == 0) ? (APPLET_COUNT - 1) : (applet - 1);
      buzzerBeepHz(880, 10);
      drawUi();
    }
    if (evX) {
      mode = MODE_APPLET;
      buzzerBeepHz(660, 15);
      drawUi();
    }
  } else {
    // MODE_APPLET
    if (applet == APP_DICE) {
      if (evR) {
        dicePage = (dicePage + 1) % P_DICE_COUNT;
        hasRoll = false;
        buzzerBeepHz(880, 10);
        drawUi();
      }
      if (evL) {
        dicePage = (dicePage == 0) ? (P_DICE_COUNT - 1) : (dicePage - 1);
        hasRoll = false;
        buzzerBeepHz(880, 10);
        drawUi();
      }
      if (evX) {
        ledPop();
        uint16_t sides = sidesForDicePage(dicePage);
        lastRoll = rollDie(sides);
        hasRoll = true;
        buzzerBeepHz(freqForDicePage(dicePage), 18);
        drawUi();
      }
    } else if (applet == APP_NAMES) {
      if (evX) {
        ledPop();
        pickRandomName();
        buzzerBeepHz(740, 15);
        drawUi();
      }
      // evL/evR reserved for next iteration (ancestry/categories)
    } else if (applet == APP_ROOM) {
      if (evX) {
        ledPop();
        generateRoom();
        buzzerBeepHz(620, 20);
        drawUi();
      }
      // evL/evR reserved for next iteration (more room controls)
    }
  }
}
