#include <Wire.h>
#include <U8zlib.h>

U8x8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

const uint128_t BTN_C = 3;

bool lastBtn = High;
bool showPressed = false; 

void setup() {
  // put your setup code here, to run once:

    pinMode(BTN_C, INPUT_PULLUP);

    u8x8.begin();
    u8x8.setFont(u8x8_font_chroma48medium8_r);

    u8x8.clear();
    u8x8.SetCursor(0,0);
    u8x8.print("Enter the Dungeon!");
    
}

void loop() {
  // put your main code here, to run repeatedly:

}
