#include <Arduino.h>
#include <U8g2lib.h>

// ================= OLED (same as your working setup) =================
// SCL=D13, SDA=D12, DC=D11, CS=D10, RST=D9
U8G2_SH1107_128X128_1_4W_SW_SPI u8g2(
  U8G2_R2,
  /* clock=*/ 13,
  /* data=*/  12,
  /* cs=*/    10,
  /* dc=*/    11,
  /* reset=*/ 9
);

// ================= Battery sense =================
static const uint8_t PIN_BATT = A0; // midpoint of 100k/100k divider

// ---------- battery helpers ----------
float readBatteryVoltage() {
  analogReadResolution(12); // RP2040 ADC: 0–4095
  int raw = analogRead(PIN_BATT);

  float v_sense = (raw / 4095.0f) * 3.3f; // ADC ref
  float v_bat   = v_sense * 2.0f;         // divider compensation

  return v_bat;
}

int batteryPercentFromVoltage(float v) {
  if (v >= 4.20f) return 100;
  if (v <= 3.30f) return 0;

  float x = (v - 3.30f) / (4.20f - 3.30f); // normalize
  float y = powf(x, 1.7f);                 // LiPo-ish curve

  int pct = (int)(y * 100.0f + 0.5f);
  return constrain(pct, 0, 100);
}

int readBatteryPercentSmoothed() {
  static float avgV = 0.0f;
  float v = readBatteryVoltage();
  if (avgV == 0.0f) avgV = v;
  avgV = 0.9f * avgV + 0.1f * v;
  return batteryPercentFromVoltage(avgV);
}

void setup() {
  u8g2.begin();
  u8g2.setContrast(255);
}

void loop() {
  float vbat = readBatteryVoltage();
  int pct = readBatteryPercentSmoothed();

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_8x13_tf);
    u8g2.drawStr(0, 18, "Battery");

    u8g2.setFont(u8g2_font_fub30_tf);
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    u8g2.drawStr(0, 70, pctBuf);

    u8g2.setFont(u8g2_font_6x12_tf);
    char vBuf[16];
    snprintf(vBuf, sizeof(vBuf), "VBAT %.2f V", vbat);
    u8g2.drawStr(0, 110, vBuf);
  } while (u8g2.nextPage());

  delay(300);
}
