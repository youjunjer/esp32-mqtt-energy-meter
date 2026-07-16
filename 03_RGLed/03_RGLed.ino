// Pin mapping inferred from the wiring photo.
// Adjust these three pins here if your LEDs are on different GPIOs.
#define RED_PIN 4
#define YELLOW_PIN 2
#define GREEN_PIN 15

void setLights(bool redOn, bool yellowOn, bool greenOn) {
  digitalWrite(RED_PIN, redOn ? HIGH : LOW);
  digitalWrite(YELLOW_PIN, yellowOn ? HIGH : LOW);
  digitalWrite(GREEN_PIN, greenOn ? HIGH : LOW);
}

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  setLights(false, false, false);
}

void loop() {
  setLights(true, false, false);
  delay(5000);

  setLights(false, true, false);
  delay(1000);

  setLights(false, false, true);
  delay(3000);
}

