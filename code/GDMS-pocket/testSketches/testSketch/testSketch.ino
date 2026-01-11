#include <U8g2lib.h>

// ================= OLED =================
U8G2_SH1107_128X128_1_4W_SW_SPI u8g2(
  U8G2_R2,
  /* clock=*/ 13,
  /* data=*/  12,
  /* cs=*/    10,
  /* dc=*/    11,
  /* reset=*/ 9
);

// ================= PINS =================
constexpr int BUZZER_PIN = 5;
constexpr int LED_PIN    = 24;

constexpr int BTN_UP   = A2;
constexpr int BTN_DOWN = A3;
constexpr int BTN_A    = A1;
constexpr int BTN_B    = A0;

void setup() {
  // Buttons
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_A,    INPUT_PULLUP);
  pinMode(BTN_B,    INPUT_PULLUP);

  // LED + buzzer
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x12_tr);
}

void loop() {
  bool up    = (digitalRead(BTN_UP)   == LOW);
  bool down  = (digitalRead(BTN_DOWN) == LOW);
  bool a     = (digitalRead(BTN_A)    == LOW);
  bool b     = (digitalRead(BTN_B)    == LOW);

  bool anyPressed = up || down || a || b;

  // LED follows any button
  digitalWrite(LED_PIN, anyPressed ? HIGH : LOW);

  // Buzzer feedback (short beeps)
  if (up)   tone(BUZZER_PIN, 880, 40);
  if (down) tone(BUZZER_PIN, 660, 40);
  if (a)    tone(BUZZER_PIN, 990, 40);
  if (b)    tone(BUZZER_PIN, 440, 40);

  // OLED output
  u8g2.firstPage();
  do {
    u8g2.drawStr(0, 12, "GDMS HW TEST");

    u8g2.drawStr(0, 32, up   ? "UP:   ON" : "UP:   off");
    u8g2.drawStr(0, 46, down ? "DOWN: ON" : "DOWN: off");
    u8g2.drawStr(0, 60, a    ? "A:    ON" : "A:    off");
    u8g2.drawStr(0, 74, b    ? "B:    ON" : "B:    off");

    u8g2.drawStr(0, 100, anyPressed ? "LED: ON" : "LED: off");
  } while (u8g2.nextPage());

  delay(50);
}
