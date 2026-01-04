// =================== PINS ===================
const uint8_t LED_PIN    = 11;  // LED (PWM-capable)
const uint8_t PIN_BUZZER = 12;  // passive buzzer

const uint8_t BTN_PINS[4]  = {5, 6, 9, 10};
const char*   BTN_NAMES[4] = {"D5", "D6", "D9", "D10"};

// =================== LED ===================
const uint8_t LED_DIM = 30;     // 0–255 (dim)
uint32_t ledOffMs = 0;
bool ledOn = false;

// =================== BUZZER ===================
bool buzOn = false;
uint32_t buzOffMs = 0;

void beepStart(uint16_t freq, uint16_t durMs) {
  tone(PIN_BUZZER, freq);
  buzOn = true;
  buzOffMs = millis() + durMs;
}

void buzzerUpdate() {
  if (buzOn && (int32_t)(millis() - buzOffMs) >= 0) {
    noTone(PIN_BUZZER);
    buzOn = false;
  }
}

// =================== BUTTON STATE ===================
bool lastState[4] = {HIGH, HIGH, HIGH, HIGH}; // pullups: HIGH idle
const uint16_t DEBOUNCE_MS = 40;
uint32_t lastPressMs[4] = {0, 0, 0, 0};

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  Serial.println("Buttons -> LED blink + buzzer test");
}

void loop() {
  uint32_t now = millis();
  buzzerUpdate();

  // turn LED off when its blink time expires
  if (ledOn && (int32_t)(now - ledOffMs) >= 0) {
    analogWrite(LED_PIN, 0);
    ledOn = false;
  }

  // read buttons
  for (int i = 0; i < 4; i++) {
    bool cur = digitalRead(BTN_PINS[i]); // HIGH idle, LOW pressed

    // edge: HIGH -> LOW (press)
    if (lastState[i] == HIGH && cur == LOW) {
      if (now - lastPressMs[i] >= DEBOUNCE_MS) {
        lastPressMs[i] = now;

        Serial.print(BTN_NAMES[i]);
        Serial.println(" PRESSED");

        // LED blink (non-blocking)
        analogWrite(LED_PIN, LED_DIM);
        ledOn = true;
        ledOffMs = now + 80; // ms

        // Buzzer beep (non-blocking)
        beepStart(880 + i * 200, 40);
      }
    }

    lastState[i] = cur;
  }

  delay(1); // tiny yield
}
