#include <Wire.h>
#include <U8g2lib.h>

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

const uint8_t BTN_C = 3;

bool lastBtn = HIGH;
bool showPressed = false;

void setup() {
  pinMode(BTN_C, INPUT_PULLUP);

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  u8x8.clear();
  u8x8.setCursor(0, 0);
  u8x8.print("HELLO");
}

void loop() {
  bool nowBtn = digitalRead(BTN_C);

  if (lastBtn == HIGH && nowBtn == LOW) {
    showPressed = !showPressed;

    u8x8.clear();
    u8x8.setCursor(0, 0);

    if (showPressed) u8x8.print("PRESSED");
    else             u8x8.print("HELLO");
  }

  lastBtn = nowBtn;
}
