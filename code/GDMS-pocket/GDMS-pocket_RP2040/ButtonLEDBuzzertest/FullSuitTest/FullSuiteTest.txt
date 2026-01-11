// Written for the Adafruit RP2040 Adalogger feather in the Arduino programmgin language

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =================== PINS ===================
const uint8_t LED_PIN    = 11;   // your external LED on D11
const uint8_t BUZZER_PIN = 12;   // passive buzzer on D12

const uint8_t BTN_PINS[4]  = {5, 6, 9, 10};
const char*   BTN_NAMES[4] = {"D5", "D6", "D9", "D10"};

// =================== OLED ===================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // -1 = no reset pin

// =================== LED DIM ===================
const uint8_t LED_DIM = 30; // 0–255

// =================== BUTTONS ===================
bool wasPressed[4] = {false, false, false, false};
uint32_t lastPressMs[4] = {0, 0, 0, 0};
const uint16_t DEBOUNCE_MS = 40;

// =================== NON-BLOCKING LED BLINK ===================
bool ledBlinkOn = false;
uint32_t ledOffMs = 0;

// =================== NON-BLOCKING BUZZER ===================
bool buzOn = false;
uint32_t buzOffMs = 0;

void buzzerStart(uint16_t freq, uint16_t durMs) {
  tone(BUZZER_PIN, freq);
  buzOn = true;
  buzOffMs = millis() + durMs;
}

void buzzerUpdate(uint32_t now) {
  if (buzOn && (int32_t)(now - buzOffMs) >= 0) {
    noTone(BUZZER_PIN);
    buzOn = false;
  }
}

// =================== OLED UPDATE POLICY ===================
// Update OLED only when idle (no recent button activity)
uint32_t lastInputMs = 0;
uint32_t lastOledMs = 0;
const uint16_t OLED_PERIOD_MS = 300;      // at most ~3x/sec when idle
const uint16_t OLED_IDLE_AFTER_MS = 250;  // don't refresh OLED right after input

int lastPressedIdx = -1;

void drawOled(bool pressedNow[4], bool anyPressed) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("GDMS Pocket Test");

  display.setCursor(0, 14);
  display.print("LED: ");
  display.print(anyPressed ? "ON " : "OFF");

  display.setCursor(0, 24);
  display.print("Last: ");
  if (lastPressedIdx >= 0) display.print(BTN_NAMES[lastPressedIdx]);
  else display.print("-");

  display.setCursor(0, 38);
  display.print("D5:");
  display.print(pressedNow[0] ? "D" : "-");
  display.print(" D6:");
  display.print(pressedNow[1] ? "D" : "-");

  display.setCursor(0, 48);
  display.print("D9:");
  display.print(pressedNow[2] ? "D" : "-");
  display.print(" D10:");
  display.print(pressedNow[3] ? "D" : "-");

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP); // button to GND
  }

  Wire.begin();
  Wire.setClock(400000); // faster I2C helps OLED a lot

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed (addr 0x3C?)");
    while (1) delay(10);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Boot...");
  display.display();

  // short startup chirp
  buzzerStart(880, 40);
  delay(60);
  buzzerStart(1320, 40);

  lastInputMs = millis();
}

void loop() {
  uint32_t now = millis();
  buzzerUpdate(now);

  // end LED blink
  if (ledBlinkOn && (int32_t)(now - ledOffMs) >= 0) {
    analogWrite(LED_PIN, 0);
    ledBlinkOn = false;
  }

  // read buttons
  bool pressedNow[4];
  bool anyPressed = false;
  bool gotEvent = false;

  for (int i = 0; i < 4; i++) {
    bool curPressed = (digitalRead(BTN_PINS[i]) == LOW);
    pressedNow[i] = curPressed;
    if (curPressed) anyPressed = true;

    // pressed edge
    if (!wasPressed[i] && curPressed) {
      if (now - lastPressMs[i] >= DEBOUNCE_MS) {
        lastPressMs[i] = now;
        wasPressed[i] = true;

        gotEvent = true;
        lastInputMs = now;
        lastPressedIdx = i;

        // dim blink (non-blocking)
        analogWrite(LED_PIN, LED_DIM);
        ledBlinkOn = true;
        ledOffMs = now + 80;

        // beep (non-blocking)
        buzzerStart(880 + i * 200, 30);

        Serial.print(BTN_NAMES[i]);
        Serial.println(" PRESSED");
      }
    }

    // release edge (just update state)
    if (wasPressed[i] && !curPressed) {
      wasPressed[i] = false;
    }
  }

  // If you prefer LED ON while holding any button (instead of blink), use this:
  // analogWrite(LED_PIN, anyPressed ? LED_DIM : 0);

  // OLED update only when idle and throttled
  if ((now - lastOledMs >= OLED_PERIOD_MS) && (now - lastInputMs >= OLED_IDLE_AFTER_MS)) {
    lastOledMs = now;
    drawOled(pressedNow, anyPressed);
  }

  delay(1);
}
