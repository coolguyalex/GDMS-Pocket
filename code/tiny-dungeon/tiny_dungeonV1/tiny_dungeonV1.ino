#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// HARDWARE
// ============================================================
// OLED: SDA=A4, SCL=A5 on Nano (I2C)
// Using U8x8 text mode to save RAM.
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

const uint8_t PIN_BTN_LEFT   = 2;  // Back / Guard
const uint8_t PIN_BTN_MID    = 3;  // Cycle
const uint8_t PIN_BTN_RIGHT  = 4;  // Commit / Attack

const uint8_t PIN_BUZZER     = 5;  // passive buzzer (optional)
const uint8_t PIN_LED        = 6;  // LED (optional)

// Button handling (simple debounced edge detect)
struct Button {
  uint8_t pin;
  bool lastStable;
  bool lastReading;
  unsigned long lastChangeMs;
};

Button btnL = {PIN_BTN_LEFT,  true, true, 0};
Button btnM = {PIN_BTN_MID,   true, true, 0};
Button btnR = {PIN_BTN_RIGHT, true, true, 0};

const unsigned long DEBOUNCE_MS = 25;

// ============================================================
// GAME DESIGN (TINY & BEATABLE)
// ============================================================

enum GameState : uint8_t {
  ST_TITLE = 0,
  ST_ROOM_INTRO,
  ST_ROOM_CHOICE,
  ST_COMBAT,
  ST_LOOT,
  ST_REST,
  ST_GAMEOVER,
  ST_WIN
};

enum RoomType : uint8_t {
  ROOM_FIGHT = 0,
  ROOM_LOOT  = 1,
  ROOM_REST  = 2,
  ROOM_BOSS  = 3
};

enum CombatPhase : uint8_t {
  C_PLAYER_MENU = 0,
  C_ENEMY_TELEGRAPH,
  C_ENEMY_STRIKE,
  C_RESOLVE
};

struct Player {
  int8_t hp;
  int8_t hpMax;
  int8_t st;
  int8_t stMax;
  uint8_t potions;
  uint8_t bombs;
  uint16_t souls; // score-ish
};

struct Monster {
  const char* name;
  int8_t hp;
  int8_t hpMax;
  int8_t dmg;
  uint8_t telegraphMs;  // how long telegraph lasts
  uint8_t strikeMs;     // how long strike window lasts
};

// ============================================================
// GLOBAL GAME VARIABLES
// ============================================================

GameState state = ST_TITLE;

Player P;

const uint8_t RUN_ROOMS = 12;
RoomType dungeon[RUN_ROOMS];
uint8_t roomIndex = 0;

RoomType currentRoom = ROOM_FIGHT;

// Combat
Monster M;
CombatPhase cphase = C_PLAYER_MENU;
uint8_t menuIndex = 0; // 0=Attack,1=Guard,2=Item
uint8_t itemIndex = 0; // 0=Potion,1=Bomb

unsigned long phaseStartMs = 0;
bool guardedThisStrike = false;   // did player guard in strike window
bool tookHitThisTurn   = false;
char msgLine[17]; // 16 chars + null for UI

// ============================================================
// UTILS: RNG, BEEP, LED, TEXT
// ============================================================

void beep(uint16_t freq, uint16_t durMs) {
  // If no buzzer wired, it’s harmless.
  tone(PIN_BUZZER, freq, durMs);
}

void ledPulse(uint8_t times, uint16_t onMs, uint16_t offMs) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(onMs);
    digitalWrite(PIN_LED, LOW);
    delay(offMs);
  }
}

// Safe 16-char print helper
void print16(uint8_t row, const char* s) {
  // Clear row then print (U8x8 is character grid)
  u8x8.drawString(0, row, "                ");
  u8x8.drawString(0, row, s);
}

// Print a label with small numbers (no sprintf heavy lifting)
void printStatsLine() {
  // Row 0: "HP 12/20 ST 6/10"
  char line[17];
  // Manual formatting to avoid heavy sprintf usage:
  // Note: this is small and safe for our sizes.
  // We'll build it piece by piece.
  for (uint8_t i = 0; i < 16; i++) line[i] = ' ';
  line[16] = '\0';

  line[0]='H'; line[1]='P'; line[2]=' ';
  // hp
  int hp = P.hp;
  int hm = P.hpMax;
  int st = P.st;
  int sm = P.stMax;

  // Simple 2-digit write
  auto put2 = [&](uint8_t col, int v) {
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    line[col]   = char('0' + (v / 10));
    line[col+1] = char('0' + (v % 10));
  };

  put2(3, hp);
  line[5]='/';
  put2(6, hm);

  line[9]='S'; line[10]='T'; line[11]=' ';
  put2(12, st);
  line[14]='/';
  put2(15, sm);

  print16(0, line);
}

void printItemsLine() {
  // Row 1: "POT x2  BOMB x1"
  char line[17];
  for (uint8_t i = 0; i < 16; i++) line[i] = ' ';
  line[16] = '\0';

  line[0]='P'; line[1]='O'; line[2]='T'; line[3]=' ';
  line[4]='x';
  line[5]= char('0' + (P.potions % 10));

  line[8]='B'; line[9]='O'; line[10]='M'; line[11]='B'; line[12]=' ';
  line[13]='x';
  line[14]= char('0' + (P.bombs % 10));

  print16(1, line);
}

// ============================================================
// INPUT: DEBOUNCED "just pressed"
// Buttons are wired to GND with INPUT_PULLUP
// ============================================================

bool buttonJustPressed(Button &b) {
  bool reading = digitalRead(b.pin); // HIGH = not pressed, LOW = pressed

  if (reading != b.lastReading) {
    b.lastChangeMs = millis();
    b.lastReading = reading;
  }

  if ((millis() - b.lastChangeMs) > DEBOUNCE_MS) {
    if (reading != b.lastStable) {
      b.lastStable = reading;
      if (b.lastStable == LOW) {
        return true; // edge: pressed
      }
    }
  }
  return false;
}

// ============================================================
// DUNGEON GENERATION (BEATABLE)
// 12 rooms: fixed curve, lightly shuffled within bands
// ============================================================

void shuffleBand(uint8_t start, uint8_t end) {
  // Fisher-Yates within [start, end) end exclusive
  for (uint8_t i = end - 1; i > start; i--) {
    uint8_t j = start + (uint8_t)random(i - start + 1);
    RoomType tmp = dungeon[i];
    dungeon[i] = dungeon[j];
    dungeon[j] = tmp;
  }
}

void generateDungeon() {
  // Band plan:
  // Rooms 0-2: easier (fight, loot, rest)
  // Rooms 3-8: mixed (fights weighted)
  // Rooms 9-10: harder + rest
  // Room 11: boss
  dungeon[0] = ROOM_FIGHT;
  dungeon[1] = ROOM_LOOT;
  dungeon[2] = ROOM_REST;

  dungeon[3] = ROOM_FIGHT;
  dungeon[4] = ROOM_FIGHT;
  dungeon[5] = ROOM_LOOT;
  dungeon[6] = ROOM_FIGHT;
  dungeon[7] = ROOM_REST;
  dungeon[8] = ROOM_FIGHT;

  dungeon[9]  = ROOM_FIGHT;
  dungeon[10] = ROOM_REST;
  dungeon[11] = ROOM_BOSS;

  // Shuffle within bands so it feels procedural but still fair.
  shuffleBand(0, 3);
  shuffleBand(3, 9);
  shuffleBand(9, 11);
}

// ============================================================
// MONSTERS (TINY POOL)
// ============================================================

Monster makeMonsterForRoom(uint8_t idx, bool boss) {
  // A few templates. Keep names short.
  // Difficulty ramps with room index.
  if (boss) {
    Monster b;
    b.name = "WRAITH LORD";
    b.hpMax = 18;
    b.hp = b.hpMax;
    b.dmg = 5;
    b.telegraphMs = 800;
    b.strikeMs = 450;
    return b;
  }

  uint8_t tier = 0;
  if (idx >= 6) tier = 2;
  else if (idx >= 3) tier = 1;

  uint8_t roll = (uint8_t)random(3);

  Monster m;
  if (tier == 0) {
    if (roll == 0) { m.name="RAT";    m.hpMax=6;  m.dmg=2; }
    if (roll == 1) { m.name="SLIME";  m.hpMax=7;  m.dmg=2; }
    if (roll == 2) { m.name="GOBLIN"; m.hpMax=8;  m.dmg=3; }
    m.telegraphMs = 700;
    m.strikeMs = 420;
  } else if (tier == 1) {
    if (roll == 0) { m.name="WOLF";   m.hpMax=10; m.dmg=3; }
    if (roll == 1) { m.name="SKELE";  m.hpMax=11; m.dmg=4; }
    if (roll == 2) { m.name="BANDIT"; m.hpMax=12; m.dmg=4; }
    m.telegraphMs = 650;
    m.strikeMs = 400;
  } else {
    if (roll == 0) { m.name="OGRE";   m.hpMax=14; m.dmg=5; }
    if (roll == 1) { m.name="KNIGHT"; m.hpMax=15; m.dmg=5; }
    if (roll == 2) { m.name="SHADE";  m.hpMax=13; m.dmg=6; }
    m.telegraphMs = 600;
    m.strikeMs = 380;
  }

  m.hp = m.hpMax;
  return m;
}

// ============================================================
// GAME INIT / RESET
// ============================================================

void newRun() {
  P.hpMax = 20;
  P.hp = P.hpMax;
  P.stMax = 10;
  P.st = P.stMax;
  P.potions = 2;
  P.bombs = 1;
  P.souls = 0;

  roomIndex = 0;
  generateDungeon();

  state = ST_ROOM_INTRO;
  currentRoom = dungeon[roomIndex];

  menuIndex = 0;
  itemIndex = 0;
  msgLine[0] = '\0';
}

// ============================================================
// DRAW SCREENS
// ============================================================

void drawTitle() {
  u8x8.clearDisplay();
  print16(1, "  POCKET DUNGEON");
  print16(3, " L/M cycle  R go");
  print16(5, "  Tiny Souls-ish");
}

void drawRoomIntro() {
  u8x8.clearDisplay();
  printStatsLine();
  printItemsLine();

  print16(3, "ENTERING ROOM...");
  char line[17];
  for (uint8_t i=0;i<16;i++) line[i]=' ';
  line[16]='\0';
  line[0]='R'; line[1]='O'; line[2]='O'; line[3]='M'; line[4]=' ';
  // room number 1-12
  uint8_t rn = roomIndex + 1;
  line[5] = char('0' + (rn / 10));
  line[6] = char('0' + (rn % 10));
  print16(4, line);

  print16(6, "R=continue");
}

void drawRoomChoice() {
  u8x8.clearDisplay();
  printStatsLine();
  printItemsLine();

  if (currentRoom == ROOM_FIGHT) {
    print16(3, "A FOE APPEARS!");
    print16(5, "R=FIGHT   L=BACK");
  } else if (currentRoom == ROOM_LOOT) {
    print16(3, "YOU FIND LOOT.");
    print16(5, "R=TAKE    L=LEAVE");
  } else if (currentRoom == ROOM_REST) {
    print16(3, "A QUIET SHRINE.");
    print16(5, "R=REST    L=LEAVE");
  } else {
    print16(3, "A DARK GATE...");
    print16(5, "R=BOSS    L=BACK");
  }
}

void drawCombat() {
  u8x8.clearDisplay();
  printStatsLine();
  printItemsLine();

  // Monster line: "GOBLIN HP:08"
  char line[17];
  for (uint8_t i=0;i<16;i++) line[i]=' ';
  line[16]='\0';

  // name up to 10 chars
  const char* n = M.name;
  uint8_t col = 0;
  while (*n && col < 10) { line[col++] = *n++; }
  line[11]='H'; line[12]='P'; line[13]=':';
  line[14]=char('0' + (M.hp / 10));
  line[15]=char('0' + (M.hp % 10));
  print16(3, line);

  // Phase messaging
  if (cphase == C_PLAYER_MENU) {
    print16(4, "CHOOSE:");
    // menu lines
    const char* a = (menuIndex==0) ? ">ATTACK" : " ATTACK";
    const char* g = (menuIndex==1) ? ">GUARD"  : " GUARD";
    const char* i = (menuIndex==2) ? ">ITEM"   : " ITEM";
    print16(5, a);
    print16(6, g);
    print16(7, i);
  } else if (cphase == C_ENEMY_TELEGRAPH) {
    print16(5, "ENEMY WINDS UP");
    print16(7, "...");
  } else if (cphase == C_ENEMY_STRIKE) {
    print16(5, "ENEMY STRIKES!");
    print16(6, "PRESS L TO GUARD");
  } else {
    // resolve message
    if (msgLine[0] != '\0') {
      print16(6, msgLine);
    } else {
      print16(6, "...");
    }
  }
}

void drawLoot() {
  u8x8.clearDisplay();
  printStatsLine();
  printItemsLine();
  print16(3, "LOOT!");
  print16(5, "R=TAKE   L=SKIP");
}

void drawRest() {
  u8x8.clearDisplay();
  printStatsLine();
  printItemsLine();
  print16(3, "RESTING...");
  print16(5, "R=HEAL+ST  L=SKIP");
}

void drawGameOver() {
  u8x8.clearDisplay();
  print16(2, "   YOU DIED");
  print16(4, "R=RETRY");
}

void drawWin() {
  u8x8.clearDisplay();
  print16(2, "  VICTORY!");
  print16(4, "R=NEW RUN");
}

// ============================================================
// STATE HELPERS
// ============================================================

void nextRoom() {
  roomIndex++;
  if (roomIndex >= RUN_ROOMS) {
    // should not happen; boss is last
    state = ST_WIN;
    return;
  }
  currentRoom = dungeon[roomIndex];
  state = ST_ROOM_INTRO;
}

void startCombat(bool boss) {
  M = makeMonsterForRoom(roomIndex, boss);
  cphase = C_PLAYER_MENU;
  menuIndex = 0;
  guardedThisStrike = false;
  tookHitThisTurn = false;
  msgLine[0] = '\0';
  state = ST_COMBAT;
}

void clampPlayer() {
  if (P.hp < 0) P.hp = 0;
  if (P.hp > P.hpMax) P.hp = P.hpMax;
  if (P.st < 0) P.st = 0;
  if (P.st > P.stMax) P.st = P.stMax;
}

void combatPlayerAttack() {
  // Attack costs stamina; deals small damage with tiny variation
  if (P.st <= 0) {
    strncpy(msgLine, "NO STAMINA", 16);
    msgLine[16] = '\0';
    beep(160, 80);
    return;
  }

  P.st -= 2;
  if (P.st < 0) P.st = 0;

  int8_t dmg = 3 + (int8_t)random(2); // 3-4
  // Slight scaling by run depth
  if (roomIndex >= 6) dmg += 1;

  M.hp -= dmg;
  if (M.hp < 0) M.hp = 0;

  strncpy(msgLine, "YOU HIT!", 16);
  msgLine[16] = '\0';
  beep(880, 40);

  // Add score
  P.souls += (uint16_t)dmg;
}

void combatPlayerGuard() {
  // Guard restores a bit of stamina and sets guard for next strike window
  P.st += 2;
  if (P.st > P.stMax) P.st = P.stMax;

  strncpy(msgLine, "YOU BRACE", 16);
  msgLine[16] = '\0';
  beep(440, 40);
}

void combatUseItem() {
  if (itemIndex == 0) {
    // Potion
    if (P.potions == 0) {
      strncpy(msgLine, "NO POTIONS", 16);
      msgLine[16] = '\0';
      beep(160, 80);
      return;
    }
    P.potions--;
    P.hp += 7;
    clampPlayer();
    strncpy(msgLine, "YOU HEAL", 16);
    msgLine[16] = '\0';
    beep(660, 60);
  } else {
    // Bomb
    if (P.bombs == 0) {
      strncpy(msgLine, "NO BOMBS", 16);
      msgLine[16] = '\0';
      beep(160, 80);
      return;
    }
    P.bombs--;
    M.hp -= 6;
    if (M.hp < 0) M.hp = 0;
    strncpy(msgLine, "BOMB! BOOM!", 16);
    msgLine[16] = '\0';
    beep(990, 60);
  }
}

void beginEnemyTelegraph() {
  cphase = C_ENEMY_TELEGRAPH;
  phaseStartMs = millis();
  guardedThisStrike = false;
  digitalWrite(PIN_LED, LOW);
}

void beginEnemyStrike() {
  cphase = C_ENEMY_STRIKE;
  phaseStartMs = millis();
  // LED = "danger"
  digitalWrite(PIN_LED, HIGH);
  beep(220, 40);
}

void resolveEnemyStrike() {
  // Damage is reduced heavily if guardedThisStrike
  int8_t dmg = M.dmg;
  if (guardedThisStrike) {
    dmg = 1; // big reduction
  }
  P.hp -= dmg;
  clampPlayer();

  digitalWrite(PIN_LED, LOW);

  if (guardedThisStrike) {
    strncpy(msgLine, "GUARD SUCCESS", 16);
    msgLine[16] = '\0';
    beep(520, 60);
  } else {
    strncpy(msgLine, "YOU ARE HIT", 16);
    msgLine[16] = '\0';
    beep(120, 80);
  }

  // After enemy acts, give player a bit of stamina back
  P.st += 2;
  clampPlayer();

  cphase = C_RESOLVE;
  phaseStartMs = millis();
}

bool monsterDead() {
  return (M.hp <= 0);
}

bool playerDead() {
  return (P.hp <= 0);
}

// ============================================================
// SETUP + LOOP
// ============================================================

void setup() {
  pinMode(PIN_BTN_LEFT,  INPUT_PULLUP);
  pinMode(PIN_BTN_MID,   INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // buzzer pin doesn't need pinMode for tone(), but it's fine
  pinMode(PIN_BUZZER, OUTPUT);

  u8x8.begin();
  u8x8.setFlipMode(1);
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  randomSeed(analogRead(A0)); // simple seed; leave A0 floating or add noise

  state = ST_TITLE;
  drawTitle();
}

void loop() {
  // Read inputs
  bool L = buttonJustPressed(btnL);
  bool Mbtn = buttonJustPressed(btnM);
  bool R = buttonJustPressed(btnR);

  switch (state) {
    case ST_TITLE: {
      if (L || Mbtn) {
        // tiny feedback: cycle nothing, just beep
        beep(600, 20);
      }
      if (R) {
        beep(800, 50);
        newRun();
        drawRoomIntro();
      }
    } break;

    case ST_ROOM_INTRO: {
      if (R) {
        state = ST_ROOM_CHOICE;
        drawRoomChoice();
      }
    } break;

    case ST_ROOM_CHOICE: {
      if (L) {
        // Back just goes to intro again (or could skip)
        state = ST_ROOM_INTRO;
        drawRoomIntro();
      }
      if (R) {
        if (currentRoom == ROOM_FIGHT) {
          startCombat(false);
          drawCombat();
        } else if (currentRoom == ROOM_BOSS) {
          startCombat(true);
          drawCombat();
        } else if (currentRoom == ROOM_LOOT) {
          state = ST_LOOT;
          drawLoot();
        } else if (currentRoom == ROOM_REST) {
          state = ST_REST;
          drawRest();
        }
      }
    } break;

    case ST_LOOT: {
      if (L) {
        // Skip loot
        nextRoom();
        drawRoomIntro();
      }
      if (R) {
        // Take: small chance for potion/bomb, else stamina boost
        uint8_t roll = (uint8_t)random(100);
        if (roll < 55) {
          P.potions = (uint8_t)min((uint8_t)9, (uint8_t)(P.potions + 1));
          beep(700, 60);
        } else if (roll < 85) {
          P.bombs   = (uint8_t)min((uint8_t)9, (uint8_t)(P.bombs + 1));
          beep(900, 60);
        } else {
          P.stMax   = (int8_t) min((int8_t)12, (int8_t)(P.stMax + 1));
          P.st = P.stMax;
          beep(500, 60);
        }
        nextRoom();
        drawRoomIntro();
      }
    } break;

    case ST_REST: {
      if (L) {
        nextRoom();
        drawRoomIntro();
      }
      if (R) {
        // Rest: heal + stamina restore
        P.hp += 6;
        P.st = P.stMax;
        clampPlayer();
        beep(420, 80);
        nextRoom();
        drawRoomIntro();
      }
    } break;

    case ST_COMBAT: {
      // Handle combat state machine
      if (cphase == C_PLAYER_MENU) {
        if (Mbtn) {
          menuIndex = (menuIndex + 1) % 3;
          beep(500, 15);
          drawCombat();
        }
        if (L) {
          // Left can act as "quick guard" selection
          menuIndex = 1;
          beep(420, 15);
          drawCombat();
        }
        if (R) {
          // Commit selected action
          msgLine[0] = '\0';

          if (menuIndex == 0) {
            combatPlayerAttack();
          } else if (menuIndex == 1) {
            combatPlayerGuard();
          } else {
            // ITEM: middle cycles item, right uses
            // To keep 3-button simple: if ITEM selected,
            // - Middle toggles potion/bomb
            // - Right uses current item
            // We'll implement that here: if you press R on ITEM menu,
            // it uses current itemIndex.
            combatUseItem();
          }

          clampPlayer();

          // If monster died from attack/bomb
          if (monsterDead()) {
            beep(1200, 80);
            // Boss win?
            if (currentRoom == ROOM_BOSS) {
              state = ST_WIN;
              drawWin();
            } else {
              // Reward: small soul + stamina
              P.souls += 10;
              P.st      = (int8_t) min((int8_t)P.stMax, (int8_t)(P.st + 3));
              nextRoom();
              drawRoomIntro();
            }
            break;
          }

          // Transition to enemy telegraph after player action
          beginEnemyTelegraph();
          drawCombat();
        }

        // Special handling: when on ITEM menu, let middle toggle item
        if (menuIndex == 2 && Mbtn) {
          itemIndex ^= 1;
          // Small UI hint in msgLine
          if (itemIndex == 0) strncpy(msgLine, "ITEM:POTION", 16);
          else strncpy(msgLine, "ITEM:BOMB", 16);
          msgLine[16] = '\0';
          beep(650, 20);
          drawCombat();
        }

      } else if (cphase == C_ENEMY_TELEGRAPH) {
        // Timer: after telegraph -> strike
        if (millis() - phaseStartMs >= M.telegraphMs) {
          beginEnemyStrike();
          drawCombat();
        }

      } else if (cphase == C_ENEMY_STRIKE) {
        // During strike window, pressing LEFT counts as guard
        if (L) {
          guardedThisStrike = true;
          beep(800, 25);
          // LED flicker acknowledgement
          digitalWrite(PIN_LED, LOW);
          delay(20);
          digitalWrite(PIN_LED, HIGH);
        }

        if (millis() - phaseStartMs >= M.strikeMs) {
          resolveEnemyStrike();
          drawCombat();

          if (playerDead()) {
            state = ST_GAMEOVER;
            drawGameOver();
          }
        }

      } else { // C_RESOLVE
        // short pause then back to player
        if (millis() - phaseStartMs >= 450) {
          cphase = C_PLAYER_MENU;
          msgLine[0] = '\0';
          drawCombat();
        }
      }
    } break;

    case ST_GAMEOVER: {
      if (R) {
        state = ST_TITLE;
        drawTitle();
      }
    } break;

    case ST_WIN: {
      if (R) {
        state = ST_TITLE;
        drawTitle();
      }
    } break;
  }
}
