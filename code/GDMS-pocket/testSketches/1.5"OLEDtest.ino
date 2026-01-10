#include <U8g2lib.h>

U8G2_SH1107_128X128_1_4W_SW_SPI u8g2(
  U8G2_R0,
  /* clock=*/ 13,  // D13
  /* data=*/  11,  // D11
  /* cs=*/    10,  // D10
  /* dc=*/    9,   // D9
  /* reset=*/ 12   // D12
);

void setup() {
  u8g2.begin();
}

void loop() {
  u8g2.firstPage();
  do {
    u8g2.drawFrame(0, 0, 128, 128);
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(6, 24, "RP2040 SW SPI");
    u8g2.drawStr(6, 44, "Pins 13/11/10/9/12");
  } while (u8g2.nextPage());

  delay(1000);
}
