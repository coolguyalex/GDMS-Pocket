const int buzzerPin = 24;

void setup() {
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  int freq = 100 + (millis()%1500);
  tone(buzzerPin, freq);
  delay(100);
}
