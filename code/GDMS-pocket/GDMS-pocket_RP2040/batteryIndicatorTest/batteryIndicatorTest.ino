#include <Arduino.h>
#include <U8g2lib.h>

// ===== OLED PINS (same as your main project) =====
const uint8_t OLED_CLK = 13;
const uint8_t OLED_DAT = 12;
const uint8_t OLED_DC  = 11;
const uint8_t OLED_CS  = 10;
const uint8_t OLED_RST = 9;

U8G2_SH1107_128X128_1_4W_SW_SPI u8g2(
  U8G2_R0,
  OLED_CLK,
  OLED_DAT,
  OLED_CS,
  OLED_DC,
  OLED_RST
);

// ===== BATTERY + LED =====
const uint8_t BATTERY_PIN = A0;
const uint8_t LED_PIN = 25;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 255);  // MAX brightness to drain battery

  analogReadResolution(12);   // 12-bit ADC (0-4095)

  u8g2.begin();
  u8g2.setContrast(255);

  Serial.begin(115200);
}

void loop() {

  // Take multiple samples for stability
  uint32_t sum = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(BATTERY_PIN);
    delay(2);
  }

  float adc = sum / (float)samples;

  float vA0 = (adc / 4095.0) * 3.3;
  float vBat = vA0 * 2.0;

  Serial.print("ADC: ");
  Serial.print(adc);
  Serial.print("  Battery: ");
  Serial.println(vBat, 3);

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tf);

    u8g2.drawStr(0, 20, "Battery Test");

    char buf[32];

    snprintf(buf, sizeof(buf), "ADC: %.0f", adc);
    u8g2.drawStr(0, 45, buf);

    snprintf(buf, sizeof(buf), "A0: %.3f V", vA0);
    u8g2.drawStr(0, 65, buf);

    snprintf(buf, sizeof(buf), "Battery: %.3f V", vBat);
    u8g2.drawStr(0, 85, buf);

  } while (u8g2.nextPage());

  delay(500);
}